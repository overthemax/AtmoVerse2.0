#include "QuotesUtils.h"
// Utilizza il sistema avanzato di gestione citazioni basato su meteo/tempo
#include "QuotesManager.h"

// Ottiene una citazione casuale dalla categoria specificata
String getRandomQuote(String category) {
  // Ignoriamo la lettura diretta del file e utilizziamo il sistema centralizzato
  // basato su meteo e momento della giornata
  Quote q = getQuoteForDisplay();

  // Se il chiamante ha passato una categoria non vuota, prova ad usarla come
  // categoria esplicita, mantenendo però la stessa logica di fallback
  if (category.length() > 0) {
    Quote overrideQuote;
    if (loadRandomQuote(category, overrideQuote)) {
      q = overrideQuote;
    }
  }

  // Gestione fallback se il testo è vuoto
  if (q.text.length() == 0) {
    q.text = "Ogni giorno è una nuova opportunità.";
  }
  if (q.author.length() == 0) {
    q.author = "Anonimo";
  }

  // Restituisce una stringa unica "testo - autore" come atteso dal vecchio codice
  String result = "\"" + q.text + "\" - " + q.author;
  return result;
}

