# mini-api-gateway-c

`mini-api-gateway-c` is a lightweight reverse proxy gateway prototype built in C for learning and portfolio use.

## Goal

This project is meant to show:

- basic C socket programming
- HTTP request parsing and relay flow
- the difference between a forward proxy and a reverse proxy
- simple path-based routing to upstream services
- system-level thinking that connects well to backend, infrastructure, and Go/AWS learning

It is intentionally small and explicit. The goal is to understand the request flow and architecture, not to build a production-grade API gateway.

## Features

- listens on a configurable gateway port
- accepts concurrent clients with one thread per connection
- parses the HTTP request line and selected headers
- routes requests by path prefix
- forwards requests to fixed upstream services
- rewrites selected headers for reverse proxy behavior
- relays the upstream response back to the client
- returns simple gateway error responses such as `404` and `502`

## Example Routes

- `/users` -> `127.0.0.1:9001`
- `/orders` -> `127.0.0.1:9002`

## Project Structure

```text
mini-api-gateway-c/
├── Makefile
├── README.md
├── docs/
│   └── architecture.md
├── include/
│   ├── http.h
│   ├── proxy.h
│   └── route.h
└── src/
    ├── http.c
    ├── main.c
    ├── proxy.c
    └── route.c
```

## Build

```bash
make
```

## Run

```bash
./mini-api-gateway-c
./mini-api-gateway-c 8081
```

## Docker

Build the gateway image only:

```bash
docker build -t mini-api-gateway-c:dev .
docker run --rm -p 8080:8080 mini-api-gateway-c:dev
```

For end-to-end local testing, run the gateway with mock upstream services through Docker Compose:

```bash
docker compose up --build
```

Then test:

```bash
curl http://127.0.0.1:8080/users/1
curl http://127.0.0.1:8080/orders/42
curl http://127.0.0.1:8080/unknown
```

Stop the stack:

```bash
docker compose down
```

## Test Idea

Run simple upstream services on `9001` and `9002`, then send requests through the gateway.

```bash
curl http://127.0.0.1:8080/users/1
curl http://127.0.0.1:8080/orders/42
curl http://127.0.0.1:8080/unknown
```

## Current Limitations

- plain HTTP only
- no TLS termination
- no authentication or rate limiting
- limited request body support
- no load balancing or service discovery
- thread-per-connection is simple, not highly scalable

## Portfolio Framing

This project is a small reverse proxy gateway prototype that demonstrates low-level network programming and request routing in C. It is intended to support a broader backend and infrastructure learning path rather than present C as the main specialization.
