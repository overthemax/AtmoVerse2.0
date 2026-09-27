#!/usr/bin/env python3
"""Generates manifest.json for the AtmoVerse automatic update.

The manifest describes the firmware and the SD files of a release: the
device downloads it, compares the SHA-256 hashes and downloads only what
changed (see Updater.cpp).

Usage (run by the release GitHub Action):
    python tools/make_manifest.py --version 2.1.0 --tag v2.1.0 \
        --repo overthemax/AtmoVerse2.0 --firmware build/AtmoVerse_2.0.ino.bin \
        --sd sd_files --out manifest.json
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

# User files (editable from the web editors): downloaded only if missing,
# never overwritten
KEEP_IF_PRESENT = {"/quotes.json"}

# SD folders and files managed by the updates.
# icons_bmp/ (35 MB, icon sources) is not used by the firmware and is left out.
MANAGED = ["www", "icons", "quotes.json"]

# The device builds the URL as base + path, without encoding
SAFE_PATH = re.compile(r"^/[A-Za-z0-9._/-]+$")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def collect_files(sd_root: Path):
    files = []
    for entry in MANAGED:
        target = sd_root / entry
        candidates = [target] if target.is_file() else sorted(p for p in target.rglob("*") if p.is_file())
        for p in candidates:
            rel = "/" + p.relative_to(sd_root).as_posix()
            if any(part.startswith(".") for part in p.relative_to(sd_root).parts):
                continue
            if not SAFE_PATH.match(rel):
                sys.exit(f"File name not allowed (only letters, digits, . _ - /): {rel}")
            files.append({
                "path": rel,
                "size": p.stat().st_size,
                "sha256": sha256(p),
                "keep": rel in KEEP_IF_PRESENT,
            })
    return files


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--version", required=True, help="version x.y.z")
    ap.add_argument("--tag", required=True, help="git tag of the release, e.g. v2.1.0")
    ap.add_argument("--repo", required=True, help="owner/repository on GitHub")
    ap.add_argument("--firmware", required=True, type=Path, help="firmware .bin file")
    ap.add_argument("--sd", required=True, type=Path, help="folder with the SD files")
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()

    if not re.match(r"^\d+\.\d+\.\d+$", args.version):
        sys.exit(f"Invalid version: {args.version} (x.y.z needed)")

    files = collect_files(args.sd)
    manifest = {
        "version": args.version,
        "firmware": {
            "url": f"https://github.com/{args.repo}/releases/download/{args.tag}/firmware.bin",
            "size": args.firmware.stat().st_size,
            "sha256": sha256(args.firmware),
        },
        "files_base_url": f"https://raw.githubusercontent.com/{args.repo}/{args.tag}/sd_files",
        "files": files,
        # Fingerprint of the file list: if it does not change between two releases,
        # the device does not check the SD card again (see syncKey in Updater.cpp)
        "files_digest": hashlib.sha256("\n".join(
            f"{f['path']}|{f['sha256']}|{int(f['keep'])}" for f in files).encode()).hexdigest(),
    }
    # Compact: the device keeps it in RAM during the check
    args.out.write_text(json.dumps(manifest, separators=(",", ":")) + "\n", encoding="utf-8")
    print(f"manifest: version {args.version}, firmware {manifest['firmware']['size']} bytes, "
          f"{len(manifest['files'])} SD files")


if __name__ == "__main__":
    main()
