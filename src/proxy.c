#define _POSIX_C_SOURCE 200112L

#include "http.h"
#include "proxy.h"
#include "route.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
    int client_fd;
    char client_ip[MAX_FORWARDED_FOR_LEN];
} client_context_t;

static volatile sig_atomic_t keep_running = 1;
static int active_server_fd = -1;

static void handle_shutdown_signal(int signum) {
    (void)signum;
    keep_running = 0;
    if (active_server_fd >= 0) {
        close(active_server_fd);
        active_server_fd = -1;
    }
}

static void install_signal_handlers(void) {
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = handle_shutdown_signal;

    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    signal(SIGPIPE, SIG_IGN);
}

static int send_all_to_upstream(int upstream_fd, const char *buffer, size_t length) {
    size_t sent = 0;

    while (sent < length) {
        ssize_t n = send(upstream_fd, buffer + sent, length - sent, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        sent += (size_t)n;
    }

    return 0;
}

static void log_request(const http_request_t *request, const route_t *route) {
    printf(
        "[thread:%lu] %s %s -> %s (%s:%s)\n",
        (unsigned long)pthread_self(),
        request->method,
        request->path,
        route->service_name,
        route->upstream_host,
        route->upstream_port
    );
}

void handle_client_connection(int client_fd, const char *client_ip) {
    char raw_request[MAX_REQUEST_SIZE];
    char upstream_request[MAX_REQUEST_SIZE];
    http_request_t request;
    const route_t *route;
    ssize_t received;
    int upstream_fd = -1;
    int upstream_request_size;

    memset(raw_request, 0, sizeof(raw_request));
    received = recv_http_request(client_fd, raw_request, sizeof(raw_request));
    if (received <= 0) {
        return;
    }

    if (parse_http_request(raw_request, &request) != 0) {
        send_simple_response(
            client_fd,
            400,
            "Bad Request",
            "Failed to parse the HTTP request.\n"
        );
        return;
    }

    if (client_ip == NULL ||
        snprintf(request.client_ip, sizeof(request.client_ip), "%s", client_ip) >=
            (int)sizeof(request.client_ip)) {
        request.client_ip[0] = '\0';
    }

    if (strcasecmp(request.method, "CONNECT") == 0) {
        send_simple_response(
            client_fd,
            501,
            "Not Implemented",
            "CONNECT tunneling is not supported in this learning build.\n"
        );
        return;
    }

    route = find_route_for_path(request.path);
    if (route == NULL) {
        send_simple_response(
            client_fd,
            404,
            "Not Found",
            "No upstream route matched the requested path.\n"
        );
        return;
    }

    log_request(&request, route);

    upstream_request_size = build_upstream_request(
        raw_request,
        &request,
        route,
        upstream_request,
        sizeof(upstream_request)
    );
    if (upstream_request_size < 0) {
        send_simple_response(
            client_fd,
            400,
            "Bad Request",
            "Failed to rebuild the upstream request.\n"
        );
        return;
    }

    upstream_fd = connect_to_upstream(route->upstream_host, route->upstream_port);
    if (upstream_fd < 0) {
        send_simple_response(
            client_fd,
            502,
            "Bad Gateway",
            "Failed to connect to the upstream service.\n"
        );
        return;
    }

    if (send_all_to_upstream(upstream_fd, upstream_request, (size_t)upstream_request_size) != 0) {
        send_simple_response(
            client_fd,
            502,
            "Bad Gateway",
            "Failed to forward the request to the upstream service.\n"
        );
        close(upstream_fd);
        return;
    }

    if (relay_upstream_response(upstream_fd, client_fd) != 0) {
        fprintf(stderr, "response relay failed: %s\n", strerror(errno));
    }

    close(upstream_fd);
}

static void *gateway_worker(void *arg) {
    client_context_t *context = (client_context_t *)arg;
    int client_fd = context->client_fd;
    char client_ip[MAX_FORWARDED_FOR_LEN];

    memcpy(client_ip, context->client_ip, sizeof(client_ip));
    free(context);
    pthread_detach(pthread_self());

    handle_client_connection(client_fd, client_ip);
    close(client_fd);

    return NULL;
}

static int spawn_worker(int client_fd, const char *client_ip) {
    pthread_t thread_id;
    client_context_t *context = malloc(sizeof(*context));

    if (context == NULL) {
        return -1;
    }

    context->client_fd = client_fd;
    if (client_ip == NULL) {
        context->client_ip[0] = '\0';
    } else {
        snprintf(context->client_ip, sizeof(context->client_ip), "%s", client_ip);
    }

    if (pthread_create(&thread_id, NULL, gateway_worker, context) != 0) {
        free(context);
        return -1;
    }

    return 0;
}

int run_gateway_server(const char *port) {
    int server_fd;
    int yes = 1;
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *current = NULL;

    install_signal_handlers();

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, port, &hints, &result) != 0) {
        fprintf(stderr, "getaddrinfo failed for port %s\n", port);
        return 1;
    }

    server_fd = -1;
    for (current = result; current != NULL; current = current->ai_next) {
        server_fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (server_fd < 0) {
            continue;
        }

        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
            close(server_fd);
            server_fd = -1;
            continue;
        }

        if (bind(server_fd, current->ai_addr, current->ai_addrlen) == 0) {
            break;
        }

        close(server_fd);
        server_fd = -1;
    }

    freeaddrinfo(result);

    if (server_fd < 0) {
        fprintf(stderr, "failed to bind gateway on port %s\n", port);
        return 1;
    }

    if (listen(server_fd, 32) != 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    active_server_fd = server_fd;
    printf("mini-api-gateway-c listening on port %s\n", port);
    if (get_routes_config_path()[0] == '\0') {
        printf("route config path: built-in defaults\n");
    } else {
        printf("route config path: %s\n", get_routes_config_path());
    }

    while (keep_running) {
        struct sockaddr_storage client_addr;
        socklen_t client_len = sizeof(client_addr);
        char client_ip[MAX_FORWARDED_FOR_LEN] = "";
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) {
            if (!keep_running) {
                break;
            }
            continue;
        }

        if (client_addr.ss_family == AF_INET) {
            struct sockaddr_in *addr4 = (struct sockaddr_in *)&client_addr;
            inet_ntop(AF_INET, &addr4->sin_addr, client_ip, sizeof(client_ip));
        } else if (client_addr.ss_family == AF_INET6) {
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&client_addr;
            inet_ntop(AF_INET6, &addr6->sin6_addr, client_ip, sizeof(client_ip));
        }

        if (spawn_worker(client_fd, client_ip) != 0) {
            fprintf(stderr, "failed to create worker thread\n");
            close(client_fd);
        }
    }

    if (active_server_fd >= 0) {
        close(active_server_fd);
        active_server_fd = -1;
    }
    printf("gateway shutdown complete\n");
    return 0;
}
