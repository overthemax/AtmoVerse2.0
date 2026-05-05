#include "DisplayBuffers.h"

// Implementazione dei buffer condivisi del display
// Questi buffer sono condivisi tra tutte le funzioni di visualizzazione
// per ridurre drasticamente l'utilizzo di stack e memoria DRAM

// Buffer principale condiviso per le stringhe temporanee
#if defined(MINIMIZE_BUFFERS)
  char sharedDisplayBuffer[80];  // Ridotto per ottimizzare memoria
#else
  char sharedDisplayBuffer[100]; // Dimensione standard
#endif

// Buffer secondario più piccolo per casi in cui sono necessari due buffer contemporaneamente
#if defined(MINIMIZE_BUFFERS)
  char secondaryDisplayBuffer[24];  // Ridotto per ottimizzare memoria
#else
  char secondaryDisplayBuffer[32];  // Dimensione standard
#endif
