/**
 * @file DisplayTask.cpp
 * @brief Task del display sul core 0 (vedi DisplayTask.h)
 */

#include "DisplayTask.h"
#include "Hardware.h"
#include "Screens.h"
#include <SD.h>

// Core 0: lo stack WiFi ha priorità 23 e interrompe il disegno quando serve;
// il task passa gran parte del tempo in attesa del pin BUSY del pannello.
static const BaseType_t DISPLAY_TASK_CORE = 0;
static const UBaseType_t DISPLAY_TASK_PRIORITY = 1;
static const uint32_t DISPLAY_TASK_STACK = 10 * 1024;

static TaskHandle_t displayTaskHandle = nullptr;
static SemaphoreHandle_t pendingMutex = nullptr;

// Richiesta in attesa (scritta dal loop, letta dal task) e copia di lavoro
// del task. Statiche e non sullo stack: contengono l'icona (2 KB).
static ScreenModel pendingModel;
static ScreenModel renderModel;
static bool hasPending = false;
static volatile bool rendering = false;

static void renderScreen(const ScreenModel& m) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    switch (m.kind) {
      case SCREEN_MAIN:    drawMainScreen(m); break;
      case SCREEN_SETUP:   drawSetupScreen(m); break;
      case SCREEN_UPDATE:  drawUpdateScreen(m); break;
      case SCREEN_MESSAGE: drawMessageScreen(m); break;
    }
  } while (display.nextPage());
}

static void displayTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    for (;;) {
      xSemaphoreTake(pendingMutex, portMAX_DELAY);
      bool work = hasPending;
      if (work) {
        renderModel = pendingModel;
        hasPending = false;
        rendering = true;
      }
      xSemaphoreGive(pendingMutex);
      if (!work) break;

      unsigned long start = millis();
      renderScreen(renderModel);
      rendering = false;
      Serial.printf("[DISPLAY] Schermata %d disegnata in %lu ms (core %d)\n",
                    renderModel.kind, millis() - start, xPortGetCoreID());
    }
  }
}

void startDisplayTask() {
  if (displayTaskHandle) return;
  pendingMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(displayTask, "display", DISPLAY_TASK_STACK, nullptr,
                          DISPLAY_TASK_PRIORITY, &displayTaskHandle, DISPLAY_TASK_CORE);
}

void showScreen(const ScreenModel& model) {
  if (!displayTaskHandle) startDisplayTask();
  xSemaphoreTake(pendingMutex, portMAX_DELAY);
  pendingModel = model;
  hasPending = true;
  xSemaphoreGive(pendingMutex);
  xTaskNotifyGive(displayTaskHandle);
}

bool waitDisplayIdle(uint32_t timeoutMs) {
  unsigned long start = millis();
  while (millis() - start < timeoutMs) {
    xSemaphoreTake(pendingMutex, portMAX_DELAY);
    bool idle = !hasPending && !rendering;
    xSemaphoreGive(pendingMutex);
    if (idle) return true;
    delay(50);
  }
  return false;
}

// BMP monocromatico (1 bit, non compresso) -> bitmap in RAM, bit a 1 = nero
bool loadIconBitmap(const char* path, ScreenModel& model) {
  model.iconWidth = model.iconHeight = 0;
  File f = SD.open(path, FILE_READ);
  if (!f) return false;

  uint8_t header[62];
  bool ok = f.read(header, sizeof(header)) == sizeof(header) && header[0] == 'B' && header[1] == 'M';
  uint32_t dataOffset = ok ? header[10] | header[11] << 8 | header[12] << 16 | (uint32_t)header[13] << 24 : 0;
  int32_t width = ok ? (int32_t)(header[18] | header[19] << 8 | header[20] << 16 | (uint32_t)header[21] << 24) : 0;
  int32_t height = ok ? (int32_t)(header[22] | header[23] << 8 | header[24] << 16 | (uint32_t)header[25] << 24) : 0;
  uint16_t bpp = ok ? header[28] | header[29] << 8 : 0;
  uint32_t compression = ok ? header[30] | header[31] << 8 : 1;

  bool topDown = height < 0;
  if (topDown) height = -height;
  if (!ok || bpp != 1 || compression != 0 || width <= 0 || height <= 0 ||
      width > ICON_MAX_SIZE || height > ICON_MAX_SIZE) {
    f.close();
    return false;
  }

  // Colore 0 della tavolozza scuro -> un bit a 0 nel file significa nero
  bool zeroIsBlack = (header[54] + header[55] + header[56]) < 384;

  int rowSize = ((width + 31) / 32) * 4;
  int outRow = (width + 7) / 8;
  uint8_t row[ICON_MAX_SIZE / 8 + 4];
  memset(model.icon, 0, sizeof(model.icon));

  for (int r = 0; r < height; r++) {
    if (!f.seek(dataOffset + (uint32_t)r * rowSize) || f.read(row, rowSize) != rowSize) {
      f.close();
      return false;
    }
    int y = topDown ? r : height - 1 - r;
    for (int b = 0; b < outRow; b++) {
      model.icon[y * outRow + b] = zeroIsBlack ? (uint8_t)~row[b] : row[b];
    }
    // Bit oltre la larghezza nell'ultimo byte: bianchi
    if (width % 8) model.icon[y * outRow + outRow - 1] &= (uint8_t)(0xFF << (8 - width % 8));
  }
  f.close();
  model.iconWidth = width;
  model.iconHeight = height;
  return true;
}
