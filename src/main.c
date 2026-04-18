#include "proxy.h"

#include <stdio.h>

int main(int argc, char *argv[]) {
    const char *port = DEFAULT_GATEWAY_PORT;

    if (argc >= 2) {
        port = argv[1];
    }

    printf("starting mini-api-gateway-c on port %s\n", port);
    return run_gateway_server(port);
}
