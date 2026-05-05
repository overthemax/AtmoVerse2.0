#include <SPIFFS.h>

bool firstLoop = true; // Flag per eseguire operazioni SPIFFS solo una volta

void setup() {
    Serial.begin(115200);
    Serial.println("Inizio setup...");

    // **FORMATTA SPIFFS SE IL MONTAGGIO INIZIALE FALLISCE - SOLO PER LA PRIMA ESECUZIONE**
    if(!SPIFFS.begin()){
        Serial.println("Errore nel montaggio di SPIFFS, tentativo di formattazione...");
        if(SPIFFS.format()){
            Serial.println("SPIFFS formattato con successo");
            if(!SPIFFS.begin()){ // Riprova a montare SPIFFS dopo la formattazione
                Serial.println("Errore nel montaggio di SPIFFS anche dopo la formattazione!");
                while(true); // Blocca se ancora non si monta
            } else {
                Serial.println("SPIFFS montato con successo DOPO la formattazione.");
            }
        } else {
            Serial.println("Formattazione di SPIFFS fallita!");
            while(true); // Blocca se la formattazione fallisce
        }
    } else {
        Serial.println("SPIFFS montato con successo (senza formattazione necessaria).");
    }

    Serial.println("Setup completato.");
}

void loop() {
    if (firstLoop) {
        Serial.println("Esecuzione operazioni SPIFFS al primo loop...");
        // Scrivi i file nella flash interna
        writeFileSPIFFS("/index.html", "<h1>Benvenuto!</h1><p>Questa è la pagina index.</p>");
        writeFileSPIFFS("/conf.txt", "parametro1=valore1\nparametro2=valore2");
        writeFileSPIFFS("/quotes.txt", "La vita è bella.\nSii felice.\nSorridi sempre.");

        // Leggi i file dalla flash interna e stampali sulla seriale
        readFileSPIFFS("/index.html");
        readFileSPIFFS("/conf.txt");
        readFileSPIFFS("/quotes.txt");

        Serial.println("Operazioni SPIFFS completate.");
        firstLoop = false; // Imposta il flag a false per non rieseguire nel loop successivo
    }
    // Altre operazioni del loop (se necessarie)
    // ...
}

void writeFileSPIFFS(const char *path, const char *message) {
    Serial.printf("Scrittura file SPIFFS: %s\n", path);

    File file = SPIFFS.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("Impossibile aprire il file per la scrittura in SPIFFS");
        return;
    }
    if (file.print(message)) {
        Serial.println("File scritto con successo in SPIFFS");
    } else {
        Serial.println("Scrittura fallita in SPIFFS");
    }
    file.close();
}

void readFileSPIFFS(const char *path) {
    Serial.printf("Lettura file SPIFFS: %s\n", path);

    File file = SPIFFS.open(path);
    if (!file) {
        Serial.println("Impossibile aprire il file per la lettura da SPIFFS");
        return;
    }

    Serial.print("Lettura dal file SPIFFS: ");
    while (file.available()) {
        Serial.write(file.read());
    }
    Serial.println();
    file.close();
}
