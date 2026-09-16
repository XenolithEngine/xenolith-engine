#!/usr/bin/env python3
# Dev server that sets COOP/COEP so SharedArrayBuffer (wasm threads) is available.
import http.server, socketserver, sys
class H(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cross-Origin-Resource-Policy", "cross-origin")
        super().end_headers()
    def log_message(self, *a): pass
port = int(sys.argv[1]) if len(sys.argv) > 1 else 8760
with socketserver.ThreadingTCPServer(("127.0.0.1", port), H) as s:
    print(f"COOP/COEP server on http://127.0.0.1:{port}"); s.serve_forever()
