#!/usr/bin/env python3
"""Converte un CSV di citazioni "a orario" nei file letti da AtmoVerse.

Formato del CSV (separatore |), una citazione per riga:
    HH:MM|espressione dell'orario|testo|opera|autore

Uscita: una cartella "orari" con un file per ora (00.txt ... 23.txt).
Ogni riga dei file:
    MM|testo|Autore, Opera
Il firmware legge solo il file dell'ora corrente e sceglie a caso tra le
citazioni del minuto attuale (vedi loadClockQuote in QuotesManager.cpp).

I file generati vanno copiati nella radice della SD (cartella /orari).
Non vanno messi nel repository: le citazioni possono essere protette da
diritto d'autore e restano solo sulla SD.

Uso:
    python tools/prepara_citazioni_orarie.py quotes.csv  C:/percorso/SD
"""
import sys
from collections import defaultdict
from pathlib import Path


def clean(text: str) -> str:
    text = text.strip()
    # Nel CSV alcune citazioni sono racchiuse tra virgolette con "" per le interne
    if len(text) >= 2 and text[0] == '"' and text[-1] == '"':
        text = text[1:-1]
    text = text.replace('""', '"').strip()
    # Il separatore dei file di uscita e gli a capo non possono comparire nel testo
    return " ".join(text.replace("|", "/").split())


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dest = Path(sys.argv[1]), Path(sys.argv[2]) / "orari"
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
    print(f"{total} citazioni in {dest} ({len(minutes)} minuti su 1440 coperti, {skipped} righe scartate)")


if __name__ == "__main__":
    main()
