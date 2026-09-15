#!/usr/bin/env python3
"""serve.py — the page's web server, with Range.

`python3 -m http.server` does not answer a Range request. It sends 200 and the
whole file. The page asks for `Range: bytes=<offset>-` on every poll so that a
run of one hour costs one small request a second, and source.js handles a
server that ignores it by throwing away the bytes it has already seen. That
works, and it means each poll carries the WHOLE mission log again: about 2 MB
a second after two minutes of Lab 1, for every student watching.

So this serves the same directory and answers Range:

  206 with the bytes asked for, and Content-Range
  204 when the offset is exactly the end: nothing new since the last poll.
      This is the usual answer while a run is quiet, and it must not look
      like a new run
  416 when the offset is PAST the end, which is what a restarted run looks
      like: the file is shorter than the page's offset, and source.js starts
      again from 0

Everything else is http.server's: it is the same directory listing, the same
media types, and the same one-file-per-request behaviour. ThreadingHTTPServer
because a poll must not wait behind another browser's request.

  usage: serve.py --directory DIR [--port 8080] [--bind 127.0.0.1]
"""

import argparse
import http.server
import os
import socketserver
import sys


class RangeHandler(http.server.SimpleHTTPRequestHandler):
    """SimpleHTTPRequestHandler, plus a single "bytes=start-[end]" range."""

    def send_head(self):
        rng = self.headers.get("Range")
        if not rng:
            return super().send_head()

        path = self.translate_path(self.path)
        if os.path.isdir(path):
            return super().send_head()

        start, end = parse_range(rng)
        if start is None:
            return super().send_head()          # a form we do not serve

        try:
            f = open(path, "rb")
        except OSError:
            self.send_error(404, "File not found")
            return None

        try:
            size = os.fstat(f.fileno()).st_size
            # Exactly at the end: nothing new since the last poll. This is the
            # common case between two writes, and it must not look like a new
            # run. 204 says "no content" and the page keeps its offset.
            if start == size:
                f.close()
                self.send_response(204)
                self.send_header("Content-Range", "bytes */%d" % size)
                self.end_headers()
                return None
            # Past the end: the file is shorter than the offset the page
            # holds, so a new run wrote it from the start. 416 tells the page
            # to begin again, and Content-Range says how long it is now.
            if start > size:
                f.close()
                self.send_response(416)
                self.send_header("Content-Range", "bytes */%d" % size)
                self.send_header("Content-Length", "0")
                self.end_headers()
                return None

            last = size - 1 if end is None or end >= size else end
            length = last - start + 1
            f.seek(start)

            self.send_response(206)
            self.send_header("Content-type", self.guess_type(path))
            self.send_header("Content-Range", "bytes %d-%d/%d" % (start, last, size))
            self.send_header("Content-Length", str(length))
            self.send_header("Accept-Ranges", "bytes")
            self.end_headers()
        except Exception:
            f.close()
            raise

        return LimitedFile(f, length)

    def end_headers(self):
        # A live page polls the same names; a cached answer would freeze it.
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, fmt, *args):
        pass                                     # one line per poll is noise


class LimitedFile:
    """The part of f that copyfile() may send. It closes f with itself."""

    def __init__(self, f, length):
        self.f = f
        self.left = length

    def read(self, n=-1):
        if self.left <= 0:
            return b""
        if n is None or n < 0 or n > self.left:
            n = self.left
        data = self.f.read(n)
        self.left -= len(data)
        return data

    def close(self):
        self.f.close()


def parse_range(value):
    """"bytes=100-" or "bytes=100-199" -> (100, None) or (100, 199).

    Anything else, including a suffix range ("bytes=-500") and a list of
    ranges, gives (None, None): the caller then serves the whole file, which
    is always a correct answer."""
    if not value.startswith("bytes="):
        return None, None
    spec = value[len("bytes="):].strip()
    if "," in spec or spec.startswith("-"):
        return None, None
    first, _, last = spec.partition("-")
    if not first.isdigit():
        return None, None
    return int(first), (int(last) if last.isdigit() else None)


class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def main():
    ap = argparse.ArgumentParser(description="serve the SENTINEL page, with Range")
    ap.add_argument("--directory", required=True)
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--bind", default="127.0.0.1")
    args = ap.parse_args()

    if not os.path.isdir(args.directory):
        sys.exit("serve: %s is not a directory" % args.directory)

    def handler(*a, **kw):
        return RangeHandler(*a, directory=args.directory, **kw)

    with Server((args.bind, args.port), handler) as httpd:
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()
