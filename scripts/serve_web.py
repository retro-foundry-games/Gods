#!/usr/bin/env python3
"""Serve the browser build with the headers a page that runs the game on a thread needs (cross-origin isolation, for SharedArrayBuffer).

    python3 serve_web.py [DIR [PORT]]      (default: the folder this script is in, 8080)

`nix build .#web` puts this script next to gods.html, gods.js and gods.wasm in `result`, so `python3 result/serve_web.py` serves the page; DIR serves another folder.
"""
import functools
import http.server
import sys
from pathlib import Path


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, ".wasm": "application/wasm", ".js": "text/javascript"}

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


def main() -> None:
    folder = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 8080
    if not (folder / "gods.html").exists():
        sys.exit(f"{folder}/gods.html is not there: serve the folder `nix build .#web` makes (`result`): python3 result/serve_web.py")
    handler = functools.partial(Handler, directory=str(folder))
    print(f"http://localhost:{port}/gods.html  ({folder})")
    http.server.ThreadingHTTPServer(("127.0.0.1", port), handler).serve_forever()


if __name__ == "__main__":
    main()
