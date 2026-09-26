#!/usr/bin/env python3
"""Genera manifest.json per l'aggiornamento automatico di AtmoVerse.

Il manifest descrive il firmware e i file della SD di una release: il
dispositivo lo scarica, confronta gli SHA-256 e scarica solo ciò che è cambiato
(vedi Updater.cpp).

Uso (lo esegue la GitHub Action di rilascio):
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

# File dell'utente (modificabili dagli editor web): scaricati solo se mancano,
# mai sovrascritti
KEEP_IF_PRESENT = {"/quotes.json", "/layout.json"}

# Cartelle e file della SD gestiti dagli aggiornamenti.
# icons_bmp/ (35 MB, sorgenti delle icone) non è usata dal firmware ed è esclusa.
MANAGED = ["www", "icons", "quotes.json", "layout.json"]

# Il dispositivo compone l'URL come base + percorso, senza codifica
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
                sys.exit(f"Nome file non ammesso (solo lettere, cifre, . _ - /): {rel}")
            files.append({
                "path": rel,
                "size": p.stat().st_size,
                "sha256": sha256(p),
                "keep": rel in KEEP_IF_PRESENT,
            })
    return files


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--version", required=True, help="versione x.y.z")
    ap.add_argument("--tag", required=True, help="tag git della release, es. v2.1.0")
    ap.add_argument("--repo", required=True, help="proprietario/repository su GitHub")
    ap.add_argument("--firmware", required=True, type=Path, help="file .bin del firmware")
    ap.add_argument("--sd", required=True, type=Path, help="cartella con i file della SD")
    ap.add_argument("--out", required=True, type=Path)
    args = ap.parse_args()

    if not re.match(r"^\d+\.\d+\.\d+$", args.version):
        sys.exit(f"Versione non valida: {args.version} (serve x.y.z)")

    manifest = {
        "version": args.version,
        "firmware": {
            "url": f"https://github.com/{args.repo}/releases/download/{args.tag}/firmware.bin",
            "size": args.firmware.stat().st_size,
            "sha256": sha256(args.firmware),
        },
        "files_base_url": f"https://raw.githubusercontent.com/{args.repo}/{args.tag}/sd_files",
        "files": collect_files(args.sd),
    }
    # Compatto: il dispositivo lo tiene in RAM durante il controllo
    args.out.write_text(json.dumps(manifest, separators=(",", ":")) + "\n", encoding="utf-8")
    print(f"manifest: versione {args.version}, firmware {manifest['firmware']['size']} byte, "
          f"{len(manifest['files'])} file della SD")


if __name__ == "__main__":
    main()
