#!/usr/bin/env python3
"""Tiny static server for the Service Worker feasibility spike.

Adds Cross-Origin-Opener-Policy/Cross-Origin-Embedder-Policy to every response so the
test page is cross-origin-isolated the same way the real guit3.html is (that isolation
is what makes question 2 in README.md meaningful - a page that ISN'T isolated wouldn't
enforce CORP on synthesized responses at all). Stdlib only, no dependencies, so it runs
with the "python3" already confirmed present rather than needing node/npm.
"""
import http.server
import functools
import os
import sys

PORT = 8791
ROOT = os.path.dirname(os.path.abspath(__file__))
# Serve from the parent of this folder so the URL path is /webui-sw-spike/... ,
# matching the scope the worker registers with in index.html.
SERVE_ROOT = os.path.dirname(ROOT)


class COIHandler(http.server.SimpleHTTPRequestHandler):
    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, fmt, *args):
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))


def main():
    handler = functools.partial(COIHandler, directory=SERVE_ROOT)
    with http.server.ThreadingHTTPServer(("127.0.0.1", PORT), handler) as httpd:
        print(f"Serving {SERVE_ROOT} at http://127.0.0.1:{PORT}/webui-sw-spike/")
        httpd.serve_forever()


if __name__ == "__main__":
    main()
