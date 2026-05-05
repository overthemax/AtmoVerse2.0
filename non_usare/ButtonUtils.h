#ifndef BUTTON_UTILS_H
#define BUTTON_UTILS_H

#include <Arduino.h>

/**
 * @brief Verifica se il pulsante di reset è premuto
 * 
 * Controlla lo stato del pulsante di reset e avvia la procedura
 * di reset della configurazione se il pulsante è premuto a lungo.
 */
void initButtons();
void checkResetButton();

#endif // BUTTON_UTILS_H
