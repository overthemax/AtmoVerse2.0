#ifndef SD_FILE_MANAGER_H
#define SD_FILE_MANAGER_H

#include <Arduino.h>
#include <SD.h>
#include <FS.h>

// Funzioni per gestire i file sulla SD card
bool initSDFileStructure();
bool fileExists(const char* path);
String readFileFromSD(const char* path);
bool writeFileToSD(const char* path, const char* content);
void createDirectoryIfNotExists(const char* path);
void saveIconsToSD();

#endif // SD_FILE_MANAGER_H
