#define _POSIX_C_SOURCE 200112L

#include "http.h"

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static int set_socket_timeouts(int fd, int seconds) {
    struct timeval timeout;

    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;

    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
        return -1;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) {
        return -1;
    }

    return 0;
}

static int write_all(int fd, const char *buffer, size_t length) {
    size_t sent = 0;

    while (sent < length) {
        ssize_t n = send(fd, buffer + sent, length - sent, 0);
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

static const char *find_header_end(const char *buffer) {
    const char *end = strstr(buffer, "\r\n\r\n");
    if (end != NULL) {
        return end + 4;
    }

    end = strstr(buffer, "\n\n");
    if (end != NULL) {
        return end + 2;
    }

    return NULL;
}

static char *trim_whitespace(char *value) {
    size_t length;

    while (*value != '\0' && isspace((unsigned char)*value)) {
        value++;
    }

    length = strlen(value);
    while (length > 0 && isspace((unsigned char)value[length - 1])) {
        value[--length] = '\0';
    }

    return value;
}

static int copy_limited(char *dst, size_t dst_size, const char *src) {
    size_t length;

    if (src == NULL) {
        return -1;
    }

    length = strlen(src);
    if (length >= dst_size) {
        return -1;
    }

    memcpy(dst, src, length + 1);
    return 0;
}

static int extract_host_and_port(const char *value, http_request_t *request) {
    char host_port[MAX_HOST_LEN + MAX_PORT_LEN + 4];
    char *colon = NULL;

    if (copy_limited(host_port, sizeof(host_port), value) != 0) {
        return -1;
    }

    colon = strrchr(host_port, ':');
    if (colon != NULL && strchr(colon + 1, ':') == NULL) {
        *colon = '\0';
        if (copy_limited(request->port, sizeof(request->port), colon + 1) != 0) {
            return -1;
        }
    }

    return copy_limited(request->host, sizeof(request->host), host_port);
}

ssize_t recv_http_request(int client_fd, char *buffer, size_t capacity) {
    size_t total = 0;

    if (capacity == 0) {
        return -1;
    }

    if (set_socket_timeouts(client_fd, IO_TIMEOUT_SECONDS) != 0) {
        return -1;
    }

    while (total < capacity - 1) {
        ssize_t n = recv(client_fd, buffer + total, capacity - 1 - total, 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            break;
        }

        total += (size_t)n;
        buffer[total] = '\0';

        if (find_header_end(buffer) != NULL) {
            return (ssize_t)total;
        }
    }

    return total > 0 ? (ssize_t)total : -1;
}

int parse_http_request(const char *raw_request, http_request_t *request) {
    char request_copy[MAX_REQUEST_SIZE];
    char *line = NULL;
    char *saveptr = NULL;

    if (strlen(raw_request) >= sizeof(request_copy)) {
        return -1;
    }

    memset(request, 0, sizeof(*request));
    if (copy_limited(request->port, sizeof(request->port), "80") != 0) {
        return -1;
    }

    memcpy(request_copy, raw_request, strlen(raw_request) + 1);

    line = strtok_r(request_copy, "\r\n", &saveptr);
    if (line == NULL) {
        return -1;
    }

    if (sscanf(
            line,
            "%15s %2047s %15s",
            request->method,
            request->target,
            request->version
        ) != 3) {
        return -1;
    }

    if (request->target[0] != '/') {
        return -1;
    }

    if (copy_limited(request->path, sizeof(request->path), request->target) != 0) {
        return -1;
    }

    while ((line = strtok_r(NULL, "\r\n", &saveptr)) != NULL) {
        char *colon = strchr(line, ':');
        char *header_name;
        char *header_value;

        if (colon == NULL) {
            continue;
        }

        *colon = '\0';
        header_name = trim_whitespace(line);
        header_value = trim_whitespace(colon + 1);

        if (strcasecmp(header_name, "Host") == 0) {
            if (extract_host_and_port(header_value, request) != 0) {
                return -1;
            }
        }
    }

    return 0;
}

int build_upstream_request(
    const char *raw_request,
    const http_request_t *request,
    const route_t *route,
    char *output,
    size_t output_size
) {
    char request_copy[MAX_REQUEST_SIZE];
    char *line = NULL;
    char *saveptr = NULL;
    size_t used = 0;
    int first_line = 1;
    int host_written = 0;
    int forwarded_for_written = 0;
    int forwarded_proto_written = 0;

    (void)request;

    if (strlen(raw_request) >= sizeof(request_copy)) {
        return -1;
    }

    memcpy(request_copy, raw_request, strlen(raw_request) + 1);
    line = strtok_r(request_copy, "\r\n", &saveptr);

    while (line != NULL) {
        int written;

        if (first_line) {
            written = snprintf(
                output + used,
                output_size - used,
                "%s %s %s\r\n",
                request->method,
                request->path,
                request->version
            );
            if (written < 0 || (size_t)written >= output_size - used) {
                return -1;
            }
            used += (size_t)written;
            first_line = 0;
            line = strtok_r(NULL, "\r\n", &saveptr);
            continue;
        }

        if (line[0] == '\0') {
            line = strtok_r(NULL, "\r\n", &saveptr);
            continue;
        }

        if (strncasecmp(line, "Proxy-Connection:", 17) == 0 ||
            strncasecmp(line, "Connection:", 11) == 0 ||
            strncasecmp(line, "X-Forwarded-For:", 16) == 0 ||
            strncasecmp(line, "X-Forwarded-Proto:", 18) == 0) {
            line = strtok_r(NULL, "\r\n", &saveptr);
            continue;
        }

        if (strncasecmp(line, "Host:", 5) == 0) {
            written = snprintf(
                output + used,
                output_size - used,
                "Host: %s:%s\r\n",
                route->upstream_host,
                route->upstream_port
            );
            if (written < 0 || (size_t)written >= output_size - used) {
                return -1;
            }
            used += (size_t)written;
            host_written = 1;
            line = strtok_r(NULL, "\r\n", &saveptr);
            continue;
        }

        written = snprintf(output + used, output_size - used, "%s\r\n", line);
        if (written < 0 || (size_t)written >= output_size - used) {
            return -1;
        }
        used += (size_t)written;
        line = strtok_r(NULL, "\r\n", &saveptr);
    }

    if (!host_written) {
        int written = snprintf(
            output + used,
            output_size - used,
            "Host: %s:%s\r\n",
            route->upstream_host,
            route->upstream_port
        );
        if (written < 0 || (size_t)written >= output_size - used) {
            return -1;
        }
        used += (size_t)written;
    }

    if (request->client_ip[0] != '\0' && !forwarded_for_written) {
        int written = snprintf(
            output + used,
            output_size - used,
            "X-Forwarded-For: %s\r\n",
            request->client_ip
        );
        if (written < 0 || (size_t)written >= output_size - used) {
            return -1;
        }
        used += (size_t)written;
        forwarded_for_written = 1;
    }

    if (!forwarded_proto_written) {
        int written = snprintf(output + used, output_size - used, "X-Forwarded-Proto: http\r\n");
        if (written < 0 || (size_t)written >= output_size - used) {
            return -1;
        }
        used += (size_t)written;
    }

    {
        int written = snprintf(output + used, output_size - used, "Connection: close\r\n\r\n");
        if (written < 0 || (size_t)written >= output_size - used) {
            return -1;
        }
        used += (size_t)written;
    }

    return (int)used;
}

int connect_to_upstream(const char *host, const char *port) {
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    struct addrinfo *current = NULL;
    int upstream_fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port, &hints, &result) != 0) {
        return -1;
    }

    for (current = result; current != NULL; current = current->ai_next) {
        upstream_fd = socket(current->ai_family, current->ai_socktype, current->ai_protocol);
        if (upstream_fd < 0) {
            continue;
        }

        if (set_socket_timeouts(upstream_fd, CONNECT_TIMEOUT_SECONDS) != 0) {
            close(upstream_fd);
            upstream_fd = -1;
            continue;
        }

        if (connect(upstream_fd, current->ai_addr, current->ai_addrlen) == 0) {
            (void)set_socket_timeouts(upstream_fd, IO_TIMEOUT_SECONDS);
            break;
        }

        close(upstream_fd);
        upstream_fd = -1;
    }

    freeaddrinfo(result);
    return upstream_fd;
}

int relay_upstream_response(int upstream_fd, int client_fd) {
    char buffer[IO_BUFFER_SIZE];

    for (;;) {
        ssize_t n = recv(upstream_fd, buffer, sizeof(buffer), 0);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (n == 0) {
            return 0;
        }
        if (write_all(client_fd, buffer, (size_t)n) != 0) {
            return -1;
        }
    }
}

int send_simple_response(
    int client_fd,
    int status_code,
    const char *reason,
    const char *message
) {
    char response[1024];
    int body_length = (int)strlen(message);
    int written = snprintf(
        response,
        sizeof(response),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: text/plain; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status_code,
        reason,
        body_length,
        message
    );

    if (written < 0 || (size_t)written >= sizeof(response)) {
        return -1;
    }

    return write_all(client_fd, response, (size_t)written);
}
