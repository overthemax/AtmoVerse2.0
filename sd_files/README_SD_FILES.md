# SD card files

The firmware keeps this folder in sync by itself: at every release the
automatic update downloads the files that changed (see `Updater.cpp` and
`tools/make_manifest.py`). You only need to copy it by hand on a card that
has never been online.

```
SD card
├── conf.json        Settings (created by the device, never overwritten by updates)
├── quotes.json      Weather and scheduled quotes (downloaded only if missing)
├── icons/           Weather icons, 1-bit BMP (Weather Icons by Erik Flowers)
├── www/             Web interface: index, settings, quotes editor, diagnostics
└── clock/           Optional: literary clock quotes, one file per hour
                     (made with tools/make_clock_quotes.py, never in git)
```

Files you add yourself are never deleted: an update only writes the files
listed in the release manifest.
