/**
 * @file Updater.h
 * @brief Aggiornamento automatico di firmware e file della SD da GitHub Releases
 *
 * L'ultima release pubblica contiene manifest.json con versione, URL e SHA-256
 * del firmware e dei file della SD. Il dispositivo scarica solo ciò che è
 * cambiato, verifica ogni file e applica tutto insieme:
 * - i file della SD vengono preparati in /upd e spostati al posto giusto solo
 *   quando tutto è stato scaricato e verificato (anche dopo un'interruzione);
 * - il firmware va nella seconda area del flash; se la nuova versione non
 *   supera i primi 60 secondi di funzionamento, il bootloader torna a quella
 *   precedente e la versione difettosa non viene più riscaricata.
 */
#ifndef UPDATER_H
#define UPDATER_H

#include <Arduino.h>

enum UpdateState : uint8_t {
  UPDATE_IDLE,
  UPDATE_DOWNLOADING,   // Il display mostra la schermata di aggiornamento
};

extern volatile UpdateState updateState;

// Da chiamare in setup() dopo il montaggio della SD: rileva un eventuale
// rollback del firmware e completa gli aggiornamenti della SD rimasti in sospeso
void initUpdater();

// Controlla GitHub e installa gli aggiornamenti. Solo dal task di rete, con WiFi connesso.
// Se viene installato un nuovo firmware il dispositivo si riavvia.
// Restituisce false se il controllo non è riuscito (da ripetere a breve).
bool checkForUpdates();

// Conferma che il firmware in esecuzione funziona (annulla il rollback automatico)
void markFirmwareHealthy();

// Esito dell'ultimo controllo, mostrato nella pagina Info
String getUpdateStatusText();

// Problema che richiede un intervento (es. SD piena), mostrato nel piè di
// pagina del display finché un controllo successivo non va a buon fine.
// Vuoto se non ci sono problemi.
String getUpdateNotice();

// GET HTTPS con certificato del server verificato (bundle di certificati
// radice di ESP-IDF) e redirect seguiti. Usata anche per il meteo.
// Restituisce il codice HTTP (200 = ok) oppure -1 se la connessione fallisce.
int httpsGet(const String& url, String& body, size_t maxLen);

#endif // UPDATER_H
