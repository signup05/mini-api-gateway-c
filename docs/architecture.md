# Architecture Notes

## Goal

The goal of this project is to turn a small C HTTP proxy foundation into a lightweight reverse proxy gateway that is still simple enough to study and explain.

## Request Flow

1. `src/main.c`
   - reads the gateway port from the CLI
   - starts the server
2. `src/proxy.c`
   - creates the listening socket
   - accepts incoming clients
   - spawns one detached worker thread per client
   - parses the request and resolves a route by path
   - connects to the selected upstream service
   - relays the upstream response
3. `src/route.c`
   - defines fixed path-prefix routes
   - returns the matching upstream target for a request path
4. `src/http.c`
   - reads the inbound request
   - parses method, target, version, and selected headers
   - rebuilds the upstream request for reverse proxy forwarding
   - handles the raw relay loop

## Design Choices

### Reverse Proxy Instead Of Forward Proxy

The original proxy idea lets the client identify the destination host. This project changes that behavior so the gateway chooses the upstream service from the request path.

### Thread-Per-Connection

This model is simple and easy to explain. It is not the most scalable design, but it keeps concurrency visible and understandable for learning.

### Static Routing Table

Routes are hard-coded on purpose. This keeps the first version focused on request flow instead of configuration parsing.

### Minimal Header Rewriting

The gateway removes proxy-specific or connection-specific headers and adds simple forwarding headers such as `X-Forwarded-For`.

## Current Limitations

- no TLS termination
- no load balancing
- no retries or circuit breaker behavior
- limited request body handling
- no dynamic config reload
- no authentication or authorization layer

These limitations are intentional. They keep the project aligned with learning goals and portfolio clarity.
