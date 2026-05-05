#pragma once

// Header per gestire i buffer condivisi del display
// Riduciamo l'utilizzo di DRAM centralizzando i buffer più utilizzati

// Espone le dimensioni per consentire sizeof() fuori da questo TU
#if defined(MINIMIZE_BUFFERS)
#  define SHARED_DISPLAY_BUFFER_SIZE   80
#  define SECONDARY_DISPLAY_BUFFER_SIZE 24
#else
#  define SHARED_DISPLAY_BUFFER_SIZE   100
#  define SECONDARY_DISPLAY_BUFFER_SIZE 32
#endif

// Buffer condiviso per le stringhe temporanee
extern char sharedDisplayBuffer[SHARED_DISPLAY_BUFFER_SIZE];

// Buffer secondario per casi in cui sono necessari due buffer temporanei simultanei
extern char secondaryDisplayBuffer[SECONDARY_DISPLAY_BUFFER_SIZE];
