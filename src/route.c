#include "route.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static route_t ROUTES[MAX_ROUTES] = {
    { "/users", "127.0.0.1", "9001", "users-service" },
    { "/orders", "127.0.0.1", "9002", "orders-service" },
};
static size_t route_count = 2;
static int initialized = 0;
static char active_config_path[256] = "";

static const char *env_or_default(const char *name, const char *fallback) {
    const char *value = getenv(name);

    if (value == NULL || value[0] == '\0') {
        return fallback;
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

static void load_routes_from_environment(void) {
    (void)copy_limited(
        ROUTES[0].upstream_host,
        sizeof(ROUTES[0].upstream_host),
        env_or_default("USERS_SERVICE_HOST", "127.0.0.1")
    );
    (void)copy_limited(
        ROUTES[0].upstream_port,
        sizeof(ROUTES[0].upstream_port),
        env_or_default("USERS_SERVICE_PORT", "9001")
    );
    (void)copy_limited(
        ROUTES[1].upstream_host,
        sizeof(ROUTES[1].upstream_host),
        env_or_default("ORDERS_SERVICE_HOST", "127.0.0.1")
    );
    (void)copy_limited(
        ROUTES[1].upstream_port,
        sizeof(ROUTES[1].upstream_port),
        env_or_default("ORDERS_SERVICE_PORT", "9002")
    );
}

const char *get_routes_config_path(void) {
    if (active_config_path[0] == '\0') {
        const char *path = getenv("ROUTES_CONFIG");

        if (path == NULL || path[0] == '\0') {
            active_config_path[0] = '\0';
        } else {
            (void)copy_limited(active_config_path, sizeof(active_config_path), path);
        }
    }

    return active_config_path;
}

int load_routes_from_file(const char *config_path) {
    FILE *file;
    char line[1024];
    size_t loaded = 0;

    if (config_path == NULL || config_path[0] == '\0') {
        return -1;
    }

    file = fopen(config_path, "r");
    if (file == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        route_t route;
        char *newline = strchr(line, '\n');

        if (newline != NULL) {
            *newline = '\0';
        }

        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        memset(&route, 0, sizeof(route));
        if (sscanf(
                line,
                "%2047s %255s %7s %63s",
                route.path_prefix,
                route.upstream_host,
                route.upstream_port,
                route.service_name
            ) != 4) {
            fclose(file);
            return -1;
        }

        if (loaded >= MAX_ROUTES) {
            fclose(file);
            return -1;
        }

        ROUTES[loaded++] = route;
    }

    fclose(file);
    if (loaded == 0) {
        return -1;
    }

    route_count = loaded;
    return 0;
}

static void initialize_routes(void) {
    const char *config_path;

    if (initialized) {
        return;
    }

    load_routes_from_environment();
    config_path = get_routes_config_path();
    if (config_path[0] == '\0') {
        printf("using built-in route defaults\n");
        route_count = 2;
    } else if (load_routes_from_file(config_path) != 0) {
        fprintf(stderr, "using built-in route defaults (config load failed: %s)\n", config_path);
        route_count = 2;
    } else {
        printf("loaded %zu routes from %s\n", route_count, config_path);
    }
    initialized = 1;
}

static int path_has_prefix(const char *path, const char *prefix) {
    size_t prefix_len;

    if (path == NULL || prefix == NULL) {
        return 0;
    }

    prefix_len = strlen(prefix);
    if (strncmp(path, prefix, prefix_len) != 0) {
        return 0;
    }

    return path[prefix_len] == '\0' ||
           path[prefix_len] == '/' ||
           prefix[prefix_len - 1] == '/';
}

const route_t *find_route_for_path(const char *path) {
    size_t i;

    initialize_routes();

    for (i = 0; i < route_count; i++) {
        if (path_has_prefix(path, ROUTES[i].path_prefix)) {
            return &ROUTES[i];
        }
    }

    return NULL;
}
