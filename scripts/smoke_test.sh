#!/usr/bin/env bash

set -euo pipefail

base_url="${1:-http://127.0.0.1:8080}"

assert_contains() {
    local haystack="$1"
    local needle="$2"
    local message="$3"

    if [[ "$haystack" != *"$needle"* ]]; then
        echo "smoke test failed: $message" >&2
        echo "response was: $haystack" >&2
        exit 1
    fi
}

users_response="$(curl -fsS "$base_url/users/1")"
assert_contains "$users_response" '"service": "users-service"' "users route should proxy to users-service"
assert_contains "$users_response" '"path": "/users/1"' "users route should preserve path"

orders_response="$(curl -fsS "$base_url/orders/42")"
assert_contains "$orders_response" '"service": "orders-service"' "orders route should proxy to orders-service"
assert_contains "$orders_response" '"path": "/orders/42"' "orders route should preserve path"

unknown_status="$(curl -s -o /tmp/mini-api-gateway-unknown.out -w "%{http_code}" "$base_url/unknown")"
if [[ "$unknown_status" != "404" ]]; then
    echo "smoke test failed: unknown route should return 404, got $unknown_status" >&2
    cat /tmp/mini-api-gateway-unknown.out >&2
    exit 1
fi

echo "smoke tests passed"
