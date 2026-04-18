#!/usr/bin/env python3

from http.server import BaseHTTPRequestHandler, HTTPServer
import json
import sys


class MockHandler(BaseHTTPRequestHandler):
    service_name = "mock-service"

    def do_GET(self):
        payload = {
            "service": self.service_name,
            "method": "GET",
            "path": self.path,
            "message": f"response from {self.service_name}",
        }
        body = json.dumps(payload).encode("utf-8")

        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        return


def main():
    if len(sys.argv) != 3:
        print("usage: mock_service.py <service-name> <port>", file=sys.stderr)
        sys.exit(1)

    service_name = sys.argv[1]
    port = int(sys.argv[2])

    MockHandler.service_name = service_name
    server = HTTPServer(("0.0.0.0", port), MockHandler)
    print(f"starting {service_name} on port {port}")
    server.serve_forever()


if __name__ == "__main__":
    main()
