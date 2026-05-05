#ifndef WEBMINIMAL_H
#define WEBMINIMAL_H

#include <Arduino.h>
#include "WeatherUtils.h"

// Genera la pagina principale con stile minimalista
String generateMinimalMainPage();

// Genera una versione minimalista della pagina delle citazioni
String generateMinimalQuotesPage();

// Genera una pagina informativa con dettagli tecnici del sistema
String generateMinimalInfoPage();

#endif // WEBMINIMAL_H
