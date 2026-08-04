#!/usr/bin/env python3
"""Serve the local dashboard without logging OAuth query parameters."""

from __future__ import annotations

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parents[1]


class RedactingHandler(SimpleHTTPRequestHandler):
    def log_request(self, code="-", size="-"):
        path = urlsplit(self.path).path
        self.log_message(
            '"%s %s %s" %s %s',
            self.command,
            path,
            self.request_version,
            str(code),
            str(size),
        )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()
    handler = partial(RedactingHandler, directory=str(ROOT / "build/dashboard/current"))
    server = ThreadingHTTPServer(("", args.port), handler)
    print(f"Serving redacted dashboard logs on http://localhost:{args.port}")
    server.serve_forever()


if __name__ == "__main__":
    main()
