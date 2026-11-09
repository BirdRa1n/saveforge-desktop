#!/usr/bin/env python3
"""Regenerates the app icon files from src/app/icons/saveforge.svg.

Builds the small Qt renderer in this folder, renders the SVG at every size the
platforms need, and packs the results:

    src/app/icons/saveforge.ico    Windows executable and installer (16-256 px)
    src/app/icons/saveforge.icns   macOS bundle (16-1024 px, Retina variants)
    src/app/icons/saveforge.png    window icon and Linux desktop entry (256 px)

Usage (Qt with the Svg module, CMake and a compiler on PATH):

    python tools/icons/generate.py --qt <qt-prefix, e.g. C:/Qt/6.12.0/mingw_64>
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "src/app/icons/saveforge.svg"
OUT = ROOT / "src/app/icons"

ICO_SIZES = [16, 20, 24, 32, 40, 48, 64, 128, 256]
# ICNS element types that hold PNG data, by pixel size.
ICNS_TYPES = [
    (b"icp4", 16), (b"icp5", 32), (b"icp6", 64), (b"ic07", 128), (b"ic08", 256),
    (b"ic09", 512), (b"ic10", 1024), (b"ic11", 32), (b"ic12", 64), (b"ic13", 256), (b"ic14", 512),
]


def run(cmd):
    subprocess.run(cmd, check=True)


def build_renderer(qt_prefix, work):
    build = work / "build"
    run(["cmake", "-S", str(Path(__file__).parent), "-B", str(build), "-G", "Ninja",
         "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_PREFIX_PATH={qt_prefix}"])
    run(["cmake", "--build", str(build)])
    exe = next(p for p in build.iterdir() if p.stem == "render" and p.is_file() and p.suffix in ("", ".exe"))
    return exe


def write_ico(pngs, target):
    # ICONDIR + one ICONDIRENTRY per image; entries hold PNG data (Vista+).
    header = struct.pack("<HHH", 0, 1, len(pngs))
    offset = 6 + 16 * len(pngs)
    entries, blobs = b"", b""
    for size, data in pngs:
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        blobs += data
        offset += len(data)
    target.write_bytes(header + entries + blobs)


def write_icns(images, target):
    body = b"".join(kind + struct.pack(">I", len(data) + 8) + data for kind, data in images)
    target.write_bytes(b"icns" + struct.pack(">I", len(body) + 8) + body)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--qt", required=True, help="Qt installation prefix (with the Svg module)")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        renderer = build_renderer(args.qt, work)
        sizes = sorted({s for s in ICO_SIZES} | {s for _, s in ICNS_TYPES})
        pngs = work / "png"
        pngs.mkdir()
        # The renderer needs the Qt libraries of the prefix it was built with.
        env_path = str(Path(args.qt) / "bin")
        env = {**os.environ, "PATH": env_path + os.pathsep + os.environ.get("PATH", "")}
        subprocess.run([str(renderer), str(SOURCE), str(pngs), *map(str, sizes)], check=True, env=env)

        def png(size):
            return (pngs / f"icon-{size}.png").read_bytes()

        write_ico([(s, png(s)) for s in ICO_SIZES], OUT / "saveforge.ico")
        write_icns([(kind, png(size)) for kind, size in ICNS_TYPES], OUT / "saveforge.icns")
        shutil.copyfile(pngs / "icon-256.png", OUT / "saveforge.png")
    print(f"wrote saveforge.ico, saveforge.icns and saveforge.png to {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
