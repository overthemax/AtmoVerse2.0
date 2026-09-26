/**
 * @file EcoPower.h
 * @brief Risparmio energetico a batteria
 *
 * Col caricatore collegato tutto resta acceso come sempre. A batteria:
 * - il WiFi si accende solo quando serve (meteo e ora, controllo
 *   aggiornamenti) e si spegne subito dopo;
 * - tra un aggiornamento del display e il successivo la scheda dorme
 *   (light sleep: RAM e stato conservati) fino allo scatto del minuto;
 * - toccando la vite in alto a destra (retro) la scheda si sveglia e tiene
 *   WiFi e pagina web accesi per 10 minuti, prolungati a ogni richiesta.
 */
#ifndef ECO_POWER_H
#define ECO_POWER_H

#include <Arduino.h>

// Da chiamare in setup() dopo battery.begin(): calibra il tasto a sfioramento
void ecoBegin();

// Da chiamare quando il caricatore viene collegato o staccato: ricalibra il tocco
void ecoPowerChanged();

// true a batteria (non in carica), con una rete configurata e fuori dalla modalità AP
bool ecoActive();

// Finestra della pagina web aperta da un tocco
bool ecoWebWindowOpen();
void ecoOpenWebWindow();

// Chiamata dal web server a ogni richiesta: prolunga la finestra
void ecoNoteWebActivity();

// Accende e collega il WiFi se non lo è (con attesa tra i tentativi falliti)
bool ecoEnsureWiFi();

// Spegne il WiFi se nessuno lo sta usando
void ecoWiFiOffIfIdle();

// Dorme fino allo scatto del minuto successivo (al massimo maxMs), solo col
// WiFi spento e il display fermo. Un tocco sveglia e apre la pagina web.
void ecoSleep(unsigned long maxMs);

#endif // ECO_POWER_H
