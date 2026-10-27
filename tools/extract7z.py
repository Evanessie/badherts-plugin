"""Extract a 7-Zip self-extracting archive without running the .exe.

The SFX is a PE stub with a 7z payload appended; we locate the 7z signature,
slice the payload out to a temp file and let py7zr do the actual work.
"""
import os
import sys
import tempfile

import py7zr

SIG = b"7z\xbc\xaf\x27\x1c"


def main(src, dest):
    with open(src, "rb") as f:
        data = f.read()

    offset = data.find(SIG)
    while offset != -1 and offset < 64 * 1024 * 1024:
        # sanity: a real 7z signature is followed by version bytes 00 04
        if data[offset + 6:offset + 8] == b"\x00\x04":
            break
        offset = data.find(SIG, offset + 1)

    if offset <= 0:
        raise SystemExit("no 7z payload found in %s" % src)

    print("payload at offset %d (%.1f MB)" % (offset, (len(data) - offset) / 1e6))

    fd, tmp = tempfile.mkstemp(suffix=".7z")
    os.close(fd)
    try:
        with open(tmp, "wb") as f:
            f.write(data[offset:])
        os.makedirs(dest, exist_ok=True)
        with py7zr.SevenZipFile(tmp, mode="r") as z:
            z.extractall(path=dest)
    finally:
        try:
            os.remove(tmp)
        except OSError:
            pass

    print("extracted ->", dest)
    for name in sorted(os.listdir(dest))[:20]:
        print("   ", name)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
