#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <SPI.h>

// Definisci i pin SPI per il display e-ink
// Aggiornati in base ai pin dell'adattatore DESPI-C02 (busy,res,d/c,cs,sck,sdi,gnd,3,3v)
#define EPD_BUSY    4    // GPIO04 - busy
#define EPD_RST     16   // GPIO16 - res (reset)
#define EPD_DC      17   // GPIO17 - d/c (data/command)
#define EPD_CS      5    // GPIO05 - cs (chip select)
#define EPD_SCK     18   // GPIO18 - sck (clock)
#define EPD_MOSI    23   // GPIO23 - sdi (data in/MOSI)

// Usa il modello corretto per GDEW0583T8 (5.83 pollici)
GxEPD2_BW<GxEPD2_583_T8, GxEPD2_583_T8::HEIGHT> display(GxEPD2_583_T8(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)); // 5.83" GDEW0583T8

void hardwareReset() {
  Serial.println("Eseguo reset hardware...");
  // Reset hardware del display
  pinMode(EPD_RST, OUTPUT);
  digitalWrite(EPD_RST, LOW);
  delay(50);  // Aumentato il tempo di reset a 50ms
  digitalWrite(EPD_RST, HIGH);
  delay(200);
  Serial.println("Reset hardware completato");
}

void testConnessioni() {
  Serial.println("Test connessioni pin...");
  // Test pin BUSY
  pinMode(EPD_BUSY, INPUT);
  Serial.print("BUSY pin stato: ");
  Serial.println(digitalRead(EPD_BUSY));
  
  // Test pin RST
  pinMode(EPD_RST, OUTPUT);
  digitalWrite(EPD_RST, HIGH);
  Serial.println("RST pin impostato HIGH");
  delay(100);
  digitalWrite(EPD_RST, LOW);
  Serial.println("RST pin impostato LOW");
  delay(100);
  digitalWrite(EPD_RST, HIGH);
  Serial.println("RST pin reimpostato HIGH");
  
  // Test pin DC
  pinMode(EPD_DC, OUTPUT);
  digitalWrite(EPD_DC, HIGH);
  Serial.println("DC pin impostato HIGH");
  delay(100);
  digitalWrite(EPD_DC, LOW);
  Serial.println("DC pin impostato LOW");
  delay(100);
  digitalWrite(EPD_DC, HIGH);
  
  // Test pin CS
  pinMode(EPD_CS, OUTPUT);
  digitalWrite(EPD_CS, HIGH);
  Serial.println("CS pin impostato HIGH");
  delay(100);
  digitalWrite(EPD_CS, LOW);
  Serial.println("CS pin impostato LOW");
  delay(100);
  digitalWrite(EPD_CS, HIGH);
  
  Serial.println("Test connessioni completato");
}

void setup() {
  // Inizializza la comunicazione seriale per il debug
  Serial.begin(115200);
  delay(2000); // Aumentato per assicurarsi che il monitor seriale si apra
  Serial.println("\n\n----- Test display e-ink GDEW0583T8 -----");
  
  // Test delle connessioni dei pin
  testConnessioni();
  
  // Reset hardware prima dell'inizializzazione
  hardwareReset();
  
  // Configura esplicitamente i pin SPI
  Serial.println("Configurazione SPI...");
  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  Serial.println("SPI configurato");
  
  // Inizializza il display con più opzioni di debug
  Serial.println("Inizializzazione display...");
  
  // Versione pulita per l'inizializzazione
  hardwareReset();
  display.init(0); // Senza output di debug seriale
  
  Serial.println("Display inizializzato con successo");
  
  // Impostazioni appropriate per il GDEW0583T8
  display.setRotation(0); // Modificato a 1 (90 gradi) per sistemare il display capovolto
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  
  // Pulisci il display
  Serial.println("Pulizia display...");
  display.clearScreen();
  Serial.println("Display pulito");

  // Scrivi del testo sul display
  Serial.println("Scrittura testo...");
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    
    // Posizionamento testo adattato alla nuova rotazione
    int textX = 150;
    int startY = 100;
    int lineHeight = 50;
    
    display.setCursor(textX, startY);
    display.println("Test Display E-Ink");
    
    display.setCursor(textX, startY + lineHeight);
    display.println("GDEW0583T8 (5.83\")");
    
    display.setCursor(textX, startY + lineHeight*2);
    display.println("Collegato con DESPI-C02");
    
    // Aggiunto un bordo per vedere i limiti del display
    display.drawRect(10, 10, display.width()-20, display.height()-20, GxEPD_BLACK);
    
  } while (display.nextPage());
  
  Serial.println("Testo scritto sul display");
  Serial.println("Setup completato");
  
  // Indicazione di completamento
  for (int i = 0; i < 5; i++) {
    digitalWrite(2, HIGH); // LED integrato
    delay(100);
    digitalWrite(2, LOW);
    delay(100);
  }
}

void loop() {
  // Nessuna azione nel loop
  delay(5000);
  Serial.println("Display attivo? (verifica visivamente)");
}
