#!/usr/bin/env python3
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import argparse
import os
import ssl


class CrossOriginIsolatedHandler(SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "cross-origin")
        super().end_headers()


def main():
    parser = argparse.ArgumentParser(description="Serve a threaded Emscripten build with COOP/COEP headers.")
    parser.add_argument("directory", nargs="?", default=None, help="Directory to serve. Defaults to build-web-pthreads.")
    parser.add_argument("--path", dest="path", help="Directory to serve. Overrides the positional directory.")
    parser.add_argument("--host", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument("--cert", help="Optional TLS certificate file for HTTPS.")
    parser.add_argument("--key", help="Optional TLS private key file for HTTPS.")
    args = parser.parse_args()

    serve_path = args.path or args.directory or "build-web-pthreads"
    os.chdir(serve_path)
    server = ThreadingHTTPServer((args.host, args.port), CrossOriginIsolatedHandler)
    scheme = "http"
    if args.cert or args.key:
        if not args.cert or not args.key:
            raise SystemExit("--cert and --key must be provided together.")
        context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        context.load_cert_chain(certfile=args.cert, keyfile=args.key)
        server.socket = context.wrap_socket(server.socket, server_side=True)
        scheme = "https"

    print(f"Serving {os.getcwd()} at {scheme}://{args.host}:{args.port}/")
    print("COOP/COEP headers enabled for SharedArrayBuffer/Emscripten pthreads.")
    server.serve_forever()


if __name__ == "__main__":
    main()
