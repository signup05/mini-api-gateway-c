FROM debian:bookworm-slim AS builder

WORKDIR /app

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential \
    && rm -rf /var/lib/apt/lists/*

COPY Makefile ./
COPY include ./include
COPY src ./src

RUN make

FROM debian:bookworm-slim

WORKDIR /app

RUN apt-get update \
    && apt-get install -y --no-install-recommends curl \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/mini-api-gateway-c /app/mini-api-gateway-c
COPY routes.conf /app/routes.conf

EXPOSE 8080

HEALTHCHECK --interval=10s --timeout=3s --start-period=5s --retries=3 \
    CMD curl -fsS http://127.0.0.1:8080/users/health >/dev/null || exit 1

CMD ["/app/mini-api-gateway-c", "8080"]
