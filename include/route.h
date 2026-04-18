#ifndef MINI_API_GATEWAY_ROUTE_H
#define MINI_API_GATEWAY_ROUTE_H

#include "proxy.h"

#define MAX_ROUTES 32
#define MAX_SERVICE_NAME_LEN 64

typedef struct {
    char path_prefix[MAX_PATH_LEN];
    char upstream_host[MAX_HOST_LEN];
    char upstream_port[MAX_PORT_LEN];
    char service_name[MAX_SERVICE_NAME_LEN];
} route_t;

const route_t *find_route_for_path(const char *path);
int load_routes_from_file(const char *config_path);
const char *get_routes_config_path(void);

#endif
