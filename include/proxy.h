#ifndef MINI_API_GATEWAY_PROXY_H
#define MINI_API_GATEWAY_PROXY_H

#include <stddef.h>

#define DEFAULT_GATEWAY_PORT "8080"
#define MAX_REQUEST_SIZE 65536
#define IO_BUFFER_SIZE 8192
#define MAX_METHOD_LEN 16
#define MAX_TARGET_LEN 2048
#define MAX_VERSION_LEN 16
#define MAX_HOST_LEN 256
#define MAX_PORT_LEN 8
#define MAX_PATH_LEN 2048
#define MAX_FORWARDED_FOR_LEN 128
#define CONNECT_TIMEOUT_SECONDS 3
#define IO_TIMEOUT_SECONDS 5

typedef struct {
    char method[MAX_METHOD_LEN];
    char target[MAX_TARGET_LEN];
    char version[MAX_VERSION_LEN];
    char host[MAX_HOST_LEN];
    char port[MAX_PORT_LEN];
    char path[MAX_PATH_LEN];
    char client_ip[MAX_FORWARDED_FOR_LEN];
} http_request_t;

int run_gateway_server(const char *port);
void handle_client_connection(int client_fd, const char *client_ip);

#endif
