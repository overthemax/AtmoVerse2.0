# Third-party notices

AtmoVerse's own source code is released under the [MIT license](LICENSE).
It uses the third-party software and assets listed below, each under its own license.

## Firmware binaries

The firmware published in the GitHub releases (`firmware.bin`) is built with the
libraries below. Because it links **GxEPD2**, which is licensed under the
**GNU GPL v3**, the firmware binary as a whole is distributed under the terms of
the GPL v3. The complete corresponding source code is this repository at the
release tag, plus the library versions listed here.

## Libraries (linked into the firmware, not included in this repository)

| Library | Version | License | Copyright |
|---|---|---|---|
| [GxEPD2](https://github.com/ZinggJM/GxEPD2) | 1.6.9 | GPL-3.0 | Jean-Marc Zingg |
| [Adafruit GFX Library](https://github.com/adafruit/Adafruit-GFX-Library) | 1.12.6 | BSD-2-Clause | Adafruit Industries |
| [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO) | 1.17.4 | MIT | Adafruit Industries |
| [Adafruit INA219](https://github.com/adafruit/Adafruit_INA219) | 1.2.3 | BSD | Adafruit Industries |
| [RTClib](https://github.com/adafruit/RTClib) | 2.1.4 | MIT | Adafruit Industries, JeeLabs |
| [ArduinoJson](https://github.com/bblanchon/ArduinoJson) | 7.4.3 | MIT | Benoît Blanchon |
| [U8g2_for_Adafruit_GFX](https://github.com/olikraus/U8g2_for_Adafruit_GFX) | 1.8.0 | BSD-2-Clause | olikraus |
| [Arduino core for ESP32](https://github.com/espressif/arduino-esp32) | 3.3.8 | LGPL-2.1 | Espressif Systems |
| [ESP-IDF](https://github.com/espressif/esp-idf) (incl. mbedTLS, esp_http_client, esp_qrcode) | bundled with the core | Apache-2.0 (esp_qrcode uses Project Nayuki's QR Code generator, MIT) | Espressif Systems and contributors |

## Fonts (compiled into the firmware from U8g2)

| Font | Used for | License |
|---|---|---|
| FreeUniversal (`u8g2_font_fur*`) | time and temperature | SIL Open Font License 1.1 — FreeUniversal © Stephen Wilson 2009, based on Sil Sophia © SIL International 1994–2008 |
| Lucida Sans (`u8g2_font_lu*`) | text, labels, quotes | X11 bitmap fonts by Bigelow & Holmes: © Bigelow & Holmes 1985, 1986, redistributed under the X Window System font license. Lucida is a registered trademark of Bigelow & Holmes |

## Icons (included in this repository)

`sd_files/icons/`, `sd_files/icons_bmp/` and `svg/` are converted from
[Weather Icons](https://erikflowers.github.io/weather-icons/) by Erik Flowers,
licensed under the [SIL Open Font License 1.1](LICENSES/OFL-1.1.txt).

## Data and services

- Weather data: [OpenWeather](https://openweathermap.org), used through the
  user's own API key and under OpenWeather's terms of service (data licensed
  CC BY-SA 4.0). Attribution: *Weather data provided by OpenWeather*.
- Updates: GitHub Releases and the GitHub REST API.

## Quotes

Quotes are short excerpts attributed to their authors and works. The literary
clock files (`/orari`) are not part of this repository or of the updates: they
stay on the user's own SD card.

## Tools (not distributed with the firmware)

- The case is modelled with [build123d](https://github.com/gumyr/build123d)
  (Apache-2.0) and the [Amagine3D](https://github.com/amagine-ai/Amagine3D)
  skill (Apache-2.0).
- The case design is original; its soft, rounded look was inspired by
  literary clocks such as [Author Clock](https://www.authorandco.com/). No
  part of their design files, logos or trademarks is used.
