#!/usr/bin/env python3
"""Converts a CSV of time quotes into the files read by AtmoVerse.

CSV format (| separator), one quote per line:
    HH:MM|phrase naming the time|text|work|author

Output: a "clock" folder with one file per hour (00.txt ... 23.txt).
Each line of the files:
    MM|text|Author, Work
The firmware reads only the current hour's file and picks at random among
the quotes of the current minute (see loadClockQuote in QuotesManager.cpp).

Copy the generated folder to the root of the SD card (/clock).
Never put the files in the repository: the quotes may be copyrighted and
stay on the SD card only.

Usage:
    python tools/make_clock_quotes.py quotes.csv  C:/path/to/SD
"""
import sys
from collections import defaultdict
from pathlib import Path


def clean(text: str) -> str:
    text = text.strip()
    # Some quotes in the CSV are wrapped in quotes, with "" for inner ones
    if len(text) >= 2 and text[0] == '"' and text[-1] == '"':
        text = text[1:-1]
    text = text.replace('""', '"').strip()
    # The output separator and line breaks cannot appear in the text
    return " ".join(text.replace("|", "/").split())


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dest = Path(sys.argv[1]), Path(sys.argv[2]) / "clock"
    dest.mkdir(parents=True, exist_ok=True)

    per_hour = defaultdict(list)
    skipped = 0
    for line in src.read_text(encoding="utf-8").splitlines():
        parts = line.split("|")
        if len(parts) != 5:
            skipped += 1
            continue
        hhmm, _phrase, text, work, author = parts
        try:
            hh, mm = (int(x) for x in hhmm.strip().split(":"))
        except ValueError:
            skipped += 1
            continue
        text, work, author = clean(text), clean(work), clean(author)
        if not text or not (0 <= hh < 24 and 0 <= mm < 60):
            skipped += 1
            continue
        signature = f"{author}, {work}" if work else author
        per_hour[hh].append(f"{mm:02d}|{text}|{signature}")

    total = 0
    for hh in range(24):
        lines = per_hour.get(hh, [])
        (dest / f"{hh:02d}.txt").write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8", newline="\n")
        total += len(lines)

    minutes = {(h, l[:2]) for h, ls in per_hour.items() for l in ls}
    print(f"{total} quotes in {dest} ({len(minutes)} of 1440 minutes covered, {skipped} lines skipped)")


if __name__ == "__main__":
    main()
