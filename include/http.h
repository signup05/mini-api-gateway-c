#ifndef MINI_API_GATEWAY_HTTP_H
#define MINI_API_GATEWAY_HTTP_H

#include <stddef.h>
#include <sys/types.h>

#include "proxy.h"
#include "route.h"

ssize_t recv_http_request(int client_fd, char *buffer, size_t capacity);
int parse_http_request(const char *raw_request, http_request_t *request);
int build_upstream_request(
    const char *raw_request,
    const http_request_t *request,
    const route_t *route,
    char *output,
    size_t output_size
);
int connect_to_upstream(const char *host, const char *port);
int relay_upstream_response(int upstream_fd, int client_fd);
int send_simple_response(
    int client_fd,
    int status_code,
    const char *reason,
    const char *message
);

#endif
