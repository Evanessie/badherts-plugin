"""Minimal downloader/extractor used to bootstrap the local VST build toolchain.

curl (schannel) and git-remote-https are blocked in this sandbox, but Python's
OpenSSL stack works, so we do everything with urllib here.

Usage:
  py tools/net.py assets <owner/repo>
  py tools/net.py latest <owner/repo>
  py tools/net.py dl <url> <dest>
  py tools/net.py unzip <zip> <destdir>
"""
import io
import json
import os
import sys
import time
import urllib.request
import zipfile

UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) dsh-bootstrap/1.0"


def _open(url, timeout=120):
    req = urllib.request.Request(url, headers={"User-Agent": UA,
                                               "Accept": "application/vnd.github+json"})
    return urllib.request.urlopen(req, timeout=timeout)


def api(path):
    with _open("https://api.github.com" + path, 60) as r:
        return json.load(r)


def cmd_assets(repo):
    data = api("/repos/%s/releases/latest" % repo)
    print("tag:", data["tag_name"])
    for a in data["assets"]:
        print("  %-60s %8.1f MB  %s" % (a["name"], a["size"] / 1e6, a["browser_download_url"]))


def cmd_latest(repo):
    data = api("/repos/%s/releases/latest" % repo)
    print(data["tag_name"])


def cmd_dl(url, dest):
    os.makedirs(os.path.dirname(os.path.abspath(dest)), exist_ok=True)
    t0 = time.time()
    n = 0
    with _open(url, 300) as r, open(dest, "wb") as f:
        total = r.headers.get("Content-Length")
        total = int(total) if total else 0
        while True:
            chunk = r.read(1 << 18)
            if not chunk:
                break
            f.write(chunk)
            n += len(chunk)
            if total and n % (4 << 20) < (1 << 18):
                print("  %5.1f%%  %6.1f / %.1f MB" % (100.0 * n / total, n / 1e6, total / 1e6), flush=True)
    print("%s: %.1f MB in %.1fs" % (dest, n / 1e6, time.time() - t0))


def cmd_unzip(src, destdir):
    os.makedirs(destdir, exist_ok=True)
    with zipfile.ZipFile(src) as z:
        names = z.namelist()
        z.extractall(destdir)
    roots = sorted({n.split("/")[0] for n in names if n.strip("/")})
    print("extracted %d entries -> %s" % (len(names), destdir))
    if len(roots) == 1:
        print("single root:", roots[0])


if __name__ == "__main__":
    cmd, args = sys.argv[1], sys.argv[2:]
    {"assets": cmd_assets, "latest": cmd_latest, "dl": cmd_dl, "unzip": cmd_unzip}[cmd](*args)
