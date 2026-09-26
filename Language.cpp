#include "Language.h"
#include "Config.h"

bool uiItalian() {
  return config.language[0] == '\0' || strcmp(config.language, "it") == 0;
}
