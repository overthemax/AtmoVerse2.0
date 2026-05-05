#include "ProgmemIcons.h"

// Definizione delle icone in PROGMEM (memoria Flash)
// Queste occuperanno meno spazio rispetto a tenerle in RAM

// Versioni più compatte delle icone SVG (ottimizzate per dimensioni ridotte)
const char SVG_EDIT_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><path fill='#7a604a' d='M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25z'/></svg>";
const char SVG_INFO_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><path fill='#7a604a' d='M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm1 15h-2v-6h2v6zm0-8h-2V7h2v2z'/></svg>";
const char SVG_SETTINGS_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><path fill='#7a604a' d='M19.43 12.98c.04-.32.07-.64.07-.98s-.03-.66-.07-.98l2.11-1.65c.19-.15.24-.42.12-.64l-2-3.46c-.12-.22-.39-.3-.61-.22l-2.49 1c-.52-.4-1.08-.73-1.69-.98l-.38-2.65C14.46 2.18 14.25 2 14 2h-4c-.25 0-.46.18-.49.42l-.38 2.65c-.61.25-1.17.59-1.69.98l-2.49-1c-.23-.09-.49 0-.61.22l-2 3.46c-.13.22-.07.49.12.64l2.11 1.65c-.04.32-.07.65-.07.98s.03.66.07.98l-2.11 1.65c-.19.15-.24.42-.12.64l2 3.46c.12.22.39.3.61.22l2.49-1c.52.4 1.08.73 1.69.98l.38 2.65c.03.24.24.42.49.42h4c.25 0 .46-.18.49-.42l.38-2.65c.61-.25 1.17-.59 1.69-.98l2.49 1c.23.09.49 0 .61-.22l2-3.46c.12-.22.07-.49-.12-.64l-2.11-1.65zM12 15.5c-1.93 0-3.5-1.57-3.5-3.5s1.57-3.5 3.5-3.5 3.5 1.57 3.5 3.5-1.57 3.5-3.5 3.5z'/></svg>";

// Icona meteo nuvola, versione compatta
const char SVG_CLOUD_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><path fill='#7a604a' d='M19.35 10.04C18.67 6.59 15.64 4 12 4 9.11 4 6.61 5.64 5.36 8.04 2.35 8.36 0 10.9 0 14c0 3.31 2.69 6 6 6h13c2.76 0 5-2.24 5-5 0-2.64-2.05-4.78-4.65-4.96z'/></svg>";

// Icone meteo per condizioni specifiche - versione ottimizzata
const char SVG_THUNDER_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><path fill='#7a604a' d='M12,11h3l-2,4h2l-3.75,7L12,17H9.5L12,11z'/></svg>";

const char SVG_RAIN_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><path fill='#7a604a' d='M12,14.5c0.3,0.5,1,1.7,1,2.5c0,0.8-0.7,1.5-1.5,1.5S10,17.8,10,17c0-0.8,0.7-2,1-2.5 M12,12l-0.5,0.7c-0.8,1.2-1.5,2.3-1.5,3.3c0,2.2,1.8,4,4,4s4-1.8,4-4c0-1-0.7-2.1-1.5-3.3L16,12h-4z'/></svg>";

const char SVG_SNOW_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><path fill='#7a604a' d='M13,12.6l1.4,0.9l-0.5,1.4l1.4-0.2l0.6,1.5l0.6-1.5l1.4,0.2l-0.5-1.4l1.4-0.9l-1.5-0.1l0.2-1.5l-1.2,0.9 l-1.2-0.9l0.2,1.5L13,12.6z M8,9.6l1.4,0.9l-0.5,1.4l1.4-0.2l0.6,1.5l0.6-1.5l1.4,0.2l-0.5-1.4l1.4-0.9l-1.5-0.1l0.2-1.5L11.1,9 l-1.2-0.9l0.2,1.5L8,9.6z M13,16.6l1.4,0.9l-0.5,1.4l1.4-0.2l0.6,1.5l0.6-1.5l1.4,0.2l-0.5-1.4l1.4-0.9l-1.5-0.1l0.2-1.5 l-1.2,0.9l-1.2-0.9l0.2,1.5L13,16.6z'/></svg>";

const char SVG_MIST_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><path fill='#7a604a' d='M3,15h10c0.6,0,1,0.4,1,1s-0.4,1-1,1H3c-0.6,0-1-0.4-1-1S2.4,15,3,15z M16,15h5c0.6,0,1,0.4,1,1s-0.4,1-1,1h-5 c-0.6,0-1-0.4-1-1S15.4,15,16,15z M3,19h2c0.6,0,1,0.4,1,1s-0.4,1-1,1H3c-0.6,0-1-0.4-1-1S2.4,19,3,19z M8,19h13c0.6,0,1,0.4,1,1 s-0.4,1-1,1H8c-0.6,0-1-0.4-1-1S7.4,19,8,19z'/></svg>";

const char SVG_SUN_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><circle fill='#7a604a' cx='12' cy='12' r='5'/><circle fill='none' stroke='#7a604a' cx='12' cy='12' r='10' stroke-width='1' stroke-dasharray='1.5,2'/><line x1='12' y1='2' x2='12' y2='4' stroke='#7a604a' stroke-width='1'/><line x1='12' y1='20' x2='12' y2='22' stroke='#7a604a' stroke-width='1'/><line x1='22' y1='12' x2='20' y2='12' stroke='#7a604a' stroke-width='1'/><line x1='4' y1='12' x2='2' y2='12' stroke='#7a604a' stroke-width='1'/></svg>";

const char SVG_UNKNOWN_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><circle fill='none' stroke='#7a604a' cx='12' cy='12' r='10' stroke-width='1.5'/><rect x='11' y='15' width='2' height='2' fill='#7a604a'/><rect x='11' y='7' width='2' height='6' fill='#7a604a'/></svg>";

// Icona per le citazioni - versione ottimizzata
const char SVG_QUOTE_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><path fill='#7a604a' d='M9,7L7,11h3v6H4v-6l2-4H9 M17,7l-2,4h3v6h-6v-6l2-4H17z'/></svg>";

// Icone meteo compatte - ottimizzate
const char SVG_TEMP_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><path fill='#7a604a' d='M13,15V6c0-0.6-0.4-1-1-1s-1,0.4-1,1v9c-1.2,0.4-2,1.5-2,2.8c0,1.7,1.3,3,3,3s3-1.3,3-3C15,16.5,14.2,15.4,13,15z'/></svg>";
const char SVG_HUMIDITY_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><path fill='#7a604a' d='M12,4c-2,3-3,5.2-3,7c0,1.7,1.3,3,3,3s3-1.3,3-3C15,9.2,14,7,12,4z'/></svg>";
const char SVG_PRESSURE_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><circle fill='none' stroke='#7a604a' cx='12' cy='12' r='9' stroke-width='2'/><path fill='#7a604a' d='M11,7h2v6h-2V7z M11,15h2v2h-2V15z'/></svg>";
const char SVG_LOCATION_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><path fill='#7a604a' d='M12,3C9,3,6.5,5.5,6.5,8.5C6.5,12,12,19,12,19s5.5-7,5.5-10.5C17.5,5.5,15,3,12,3z'/></svg>";
const char SVG_TIMEZONE_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><circle fill='none' stroke='#7a604a' cx='12' cy='12' r='10' stroke-width='2'/><path stroke='#7a604a' fill='none' d='M12,6v6l4,3'/></svg>";
const char SVG_UPDATE_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><circle fill='none' stroke='#7a604a' cx='12' cy='12' r='10' stroke-width='2'/><path fill='#7a604a' stroke='none' d='M8,12l4,4l4-4 M12,8v8'/></svg>";
const char SVG_REFRESH_ICON[] PROGMEM = "<svg viewBox='0 0 24 24'><path fill='none' stroke='#7a604a' d='M17.5,8C16.3,6.5,14.3,5.5,12,5.5c-3.6,0-6.5,2.9-6.5,6.5s2.9,6.5,6.5,6.5c2.8,0,5.2-1.8,6.1-4.2' stroke-width='1.5'/><polyline fill='none' stroke='#7a604a' points='16,4.5 19.5,8 16,11.5' stroke-width='1.5'/></svg>";

// Funzione helper per recuperare le stringhe dalla memoria Flash
String getProgmemString(const char* progmemString) {
  return String(FPSTR(progmemString));
}
