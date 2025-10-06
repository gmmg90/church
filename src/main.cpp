#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <time.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <RTClib.h>
#include <Wire.h>

// Include dei nostri file
#include "include/config.h"
#include "include/bell_controller.h"

// Pin I2C di default per ESP32 (T-Display): SDA=21, SCL=22, sovrascrivibili da config.h
#ifndef I2C_SDA_PIN
#define I2C_SDA_PIN 21
#endif
#ifndef I2C_SCL_PIN
#define I2C_SCL_PIN 22
#endif


// === VARIABILI GLOBALI ===
// Sistema Status
SystemStatus systemStatus;

// Bell Controller (dichiarato in bell_controller.cpp)
extern BellController bellController;

// Variabili per ora manuale se RTC non presente
DateTime manualTime;
bool manualTimeValid = false;

// === CONFIGURAZIONE SISTEMA ===
// (NTP gestito dal sistema via time.h)

// Modalità AP di backup
bool apMode = false;
const char* AP_SSID = "ChurchBells-Config";
const char* AP_PASSWORD = "campanile123";

// Variabili per gestione fuso orario Italia
bool isDST = true; // Settembre = ora legale
int utcOffsetHours = 2; // UTC+2 durante ora legale
int timeOffset = 7200; // Ora legale (UTC+2)
int currentTimezoneOffset = 7200; // Offset corrente del fuso orario

// Impostazioni SNTP/NTP
static const char* TZ_ITALY = "CET-1CEST,M3.5.0/2,M10.5.0/3"; // Italia con ora legale
static const char* NTP1 = "pool.ntp.org";
static const char* NTP2 = "time.nist.gov";
static const char* NTP3 = "time.google.com";

// Web Server
AsyncWebServer server(WEB_SERVER_PORT);

// Display
TFT_eSPI tft = TFT_eSPI();

// RTC (opzionale - se non connesso funziona comunque con NTP)
RTC_DS3231 rtc;

// Variabili di sistema
bool bellsEnabled = false;
bool testMode = false;
bool schedulerActive = false;

// Versione firmware
static const char* FIRMWARE_VERSION = "v2.2";

// Pulsante funerale - polarità (GPIO27 con pullup -> premuto = LOW)
static bool FUNERAL_PRESSED_LOW = true; // Pulsante con pullup: LOW = premuto

// Timing variables
unsigned long lastUpdate = 0;
unsigned long lastNTPCheck = 0;
unsigned long lastScheduleCheck = 0;
int lastCheckedMinute = -1;

// Variabili monitoraggio temperatura ESP32
float currentESP32Temperature = 0.0;
unsigned long lastTemperatureCheck = 0;
bool temperatureWarningActive = false;
bool thermalProtectionActive = false;

// Schedules in-memory (arrays + counters)
static WeeklySchedule weeklyArr[MAX_WEEKLY_SCHEDULES];
static int weeklyCount = 0;
static SpecialEvent specialArr[MAX_SPECIAL_EVENTS];
static int specialCount = 0;

// FS paths
static const char* WEEKLY_FS = "/weekly.json";
static const char* SPECIAL_FS = "/events.json";
static const char* MELODIES_FS = "/melodies.json";
 

// === DICHIARAZIONI DI FUNZIONE ===
void updateDisplay();
bool loadMelodiesFromFS();
bool saveAllMelodiesToFS();
void scanI2CDevices();
void initSNTP(bool waitForSync);
bool loadSchedulesFromFS();
bool saveSchedulesToFS();
void checkAndRunSchedules();
bool getLocalTm(struct tm &out);
// Forward declarations for functions used before their definitions
void connectWiFi();
void setupWebServer();
void processSerialCommands();
void startApConfig();

// === FUNZIONI TEMPERATURA ESP32 ===
float getESP32Temperature();
void checkTemperatureThresholds();
String getTemperatureStatus();

// === FUNZIONI DI UTILITY ===

// Calcola automaticamente se siamo in ora legale (DST) per l'Italia
bool isItalianDST(int month, int day, int weekday) {
  // L'ora legale in Italia va dall'ultima domenica di marzo all'ultima domenica di ottobre
  
  // Prima di marzo o dopo ottobre = ora solare
  if (month < 3 || month > 10) return false;
  
  // Aprile - settembre = sempre ora legale
  if (month > 3 && month < 10) return true;
  
  // In marzo e ottobre dipende dalla settimana
  // Calcola l'ultima domenica del mese
  int lastSunday = day;
  int currentWeekday = weekday; // 0=domenica, 1=lunedì, ...
  
  // Trova l'ultima domenica
  while (currentWeekday != 0) {
    lastSunday--;
    currentWeekday = (currentWeekday + 6) % 7; // Vai al giorno precedente
  }
  
  // Se siamo prima dell'ultima domenica, usa il comportamento del mese precedente
  if (day < lastSunday) {
    return (month == 3) ? false : true;
  }
  
  // Se siamo dopo l'ultima domenica, usa il comportamento del mese successivo
  return (month == 3) ? true : false;
}

void updateTimezone() {
  // Aggiornamento timezone
  // Ottieni la data/ora corrente dal sistema
  time_t rawtime = time(nullptr);
  struct tm * timeinfo = localtime(&rawtime);
  
  // Determina automaticamente se siamo in ora legale
  isDST = isItalianDST(timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_wday);
  
  // Aggiorna l'offset
  currentTimezoneOffset = isDST ? 7200 : 3600; // UTC+2 (legale) o UTC+1 (solare)
  utcOffsetHours = isDST ? 2 : 1;
  
  // Il client NTP non è più usato, offset applicato via TZ
  
  Serial.print("Fuso orario aggiornato: UTC+");
  Serial.print(utcOffsetHours);
  Serial.println(isDST ? " (Ora Legale)" : " (Ora Solare)");
}

void initSNTP(bool waitForSync) {
  // Inizializzazione SNTP
  configTzTime(TZ_ITALY, NTP1, NTP2, NTP3);
  if (!waitForSync) {
    Serial.println("SNTP configurato (senza attesa sync)");
    return;
  }
  Serial.println("Sincronizzazione SNTP in corso...");
  struct tm timeinfo;
  const uint32_t start = millis();
  while (!getLocalTime(&timeinfo, 1000)) { // attesa max 15s con log progressivo
    Serial.print('.');
    if (millis() - start > 15000) {
      Serial.println("\n✗ SNTP: timeout di sincronizzazione");
      systemStatus.ntpSynced = false;
      return;
    }
  }
  Serial.println("\n✓ SNTP sincronizzato");
  systemStatus.ntpSynced = true;
  updateTimezone();
  if (systemStatus.rtcConnected) {
    // Aggiorna RTC con l'ora locale attuale
    DateTime now(
      timeinfo.tm_year + 1900,
      timeinfo.tm_mon + 1,
      timeinfo.tm_mday,
      timeinfo.tm_hour,
      timeinfo.tm_min,
      timeinfo.tm_sec
    );
    rtc.adjust(now);
    Serial.println("RTC aggiornato da SNTP");
  }
}

bool getLocalTm(struct tm &out) {
  // Prova prima NTP/sistema
  time_t now = time(nullptr);
  if (now > 1000000) { // Se il tempo è ragionevole (non epoca 1970)
    struct tm* ti = localtime(&now);
    if (ti) {
      out = *ti;
      return true;
    }
  }
  
  // Se NTP non funziona, prova RTC se disponibile
  if (systemStatus.rtcConnected) {
    DateTime rtcNow = rtc.now();
    if (rtcNow.year() > 2020) { // Verifica che l'anno sia ragionevole
      out.tm_year = rtcNow.year() - 1900;
      out.tm_mon = rtcNow.month() - 1;
      out.tm_mday = rtcNow.day();
      out.tm_hour = rtcNow.hour();
      out.tm_min = rtcNow.minute();
      out.tm_sec = rtcNow.second();
      // DS3231 dayOfTheWeek(): controlla il valore effettivo
      int rtcDay = rtcNow.dayOfTheWeek();
      out.tm_wday = rtcDay; // Per ora mantieni il valore originale
      out.tm_yday = 0;
      out.tm_isdst = 0;
      return true;
    }
  }
  
  // Ultimo fallback: ora fittizia per evitare crash
  memset(&out, 0, sizeof(out));
  out.tm_year = 125; // 2025
  out.tm_mon = 8;    // settembre (0-based)
  out.tm_mday = 22;
  out.tm_hour = 12;
  out.tm_min = 0;
  out.tm_sec = 0;
  out.tm_wday = 0;   // domenica
  return true; // Restituisco sempre true per evitare errori continui
}

bool saveSchedulesToFS() {
  DynamicJsonDocument doc(16384);
  JsonArray w = doc.createNestedArray("weekly");
  for (int i=0;i<weeklyCount;i++){
    JsonObject o = w.createNestedObject();
    o["id"]=weeklyArr[i].id; o["name"]=weeklyArr[i].name; o["dayOfWeek"]=weeklyArr[i].dayOfWeek; o["hour"]=weeklyArr[i].hour; o["minute"]=weeklyArr[i].minute; o["melodyIndex"]=weeklyArr[i].melodyIndex; o["isActive"]=weeklyArr[i].isActive;
  }
  JsonArray s = doc.createNestedArray("special");
  for (int i=0;i<specialCount;i++){
    JsonObject o = s.createNestedObject();
    o["id"]=specialArr[i].id; o["name"]=specialArr[i].name; o["type"]=specialArr[i].type; o["year"]=specialArr[i].year; o["month"]=specialArr[i].month; o["day"]=specialArr[i].day; o["hour"]=specialArr[i].hour; o["minute"]=specialArr[i].minute; o["melodyIndex"]=specialArr[i].melodyIndex; o["isActive"]=specialArr[i].isActive; o["isRecurring"]=specialArr[i].isRecurring;
  }
  fs::File f = SPIFFS.open(WEEKLY_FS, "w");
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

bool loadSchedulesFromFS() {
  weeklyCount = 0; specialCount = 0;
  if (!SPIFFS.exists(WEEKLY_FS)) return true; // nothing to load
  fs::File f = SPIFFS.open(WEEKLY_FS, "r");
  if (!f) return false;
  DynamicJsonDocument doc(16384);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  if (doc.containsKey("weekly")){
    JsonArray w = doc["weekly"].as<JsonArray>();
    for (JsonObject o : w){
      if (weeklyCount>=MAX_WEEKLY_SCHEDULES) break;
      WeeklySchedule &it = weeklyArr[weeklyCount++];
      strlcpy(it.name, (o["name"] | ""), sizeof(it.name));
      it.id = o["id"] | weeklyCount;
      it.dayOfWeek = (DayOfWeek)(int)(o["dayOfWeek"] | 0);
      it.hour = o["hour"] | 0; it.minute = o["minute"] | 0;
      it.melodyIndex = o["melodyIndex"] | 0; it.isActive = o["isActive"] | true;
    }
  }
  if (doc.containsKey("special")){
    JsonArray s = doc["special"].as<JsonArray>();
    for (JsonObject o : s){
      if (specialCount>=MAX_SPECIAL_EVENTS) break;
      SpecialEvent &it = specialArr[specialCount++];
      strlcpy(it.name, (o["name"] | ""), sizeof(it.name));
      it.id = o["id"] | specialCount;
      it.type = (EventType)(int)(o["type"] | 5);
      it.year = o["year"] | 0; it.month = o["month"] | 0; it.day = o["day"] | 0;
      it.hour = o["hour"] | 0; it.minute = o["minute"] | 0;
      it.melodyIndex = o["melodyIndex"] | 0; it.isActive = o["isActive"] | true; it.isRecurring = o["isRecurring"] | false;
    }
  }
  return true;
}

// === IMPLEMENTAZIONE FUNZIONI TEMPERATURA ESP32 ===
float getESP32Temperature() {
  // Semplice stima tramite sensore interno (non molto accurato su ESP32)
  // Su alcune revisioni il sensore non è accessibile; restituiamo l'ultima lettura se fallisce
  float tempC = temperatureRead(); // ritorna in °C
  if (isnan(tempC)) {
    return systemStatus.esp32Temperature;
  }
  // Aggiorna soglie
  if (tempC >= TEMP_SHUTDOWN_THRESHOLD) {
    systemStatus.thermalProtection = true;
    systemStatus.temperatureWarning = true;
    bellController.emergencyStop();
  } else if (tempC >= TEMP_CRITICAL_THRESHOLD) {
    systemStatus.thermalProtection = true;
    systemStatus.temperatureWarning = true;
  } else if (tempC >= TEMP_WARNING_THRESHOLD) {
    systemStatus.thermalProtection = false;
    systemStatus.temperatureWarning = true;
  } else {
    systemStatus.thermalProtection = false;
    systemStatus.temperatureWarning = false;
  }
  return tempC;
}

void checkTemperatureThresholds() {
  float t = getESP32Temperature();
  systemStatus.esp32Temperature = t;
  // Azioni di protezione già gestite in getESP32Temperature()
}

String getTemperatureStatus() {
  if (systemStatus.thermalProtection) return String("CRITICA");
  if (systemStatus.temperatureWarning) return String("ELEVATA");
  return String("OK");
}

// === MELODIE: SALVATAGGIO/CARICAMENTO SU FS ===
bool saveAllMelodiesToFS() {
  DynamicJsonDocument doc(32768);
  JsonArray arr = doc.createNestedArray("melodies");
  for (int i = 0; i < 10; i++) {
    if (!melodies[i].isActive) continue;
    JsonObject m = arr.createNestedObject();
    m["id"] = i;
    m["name"] = melodies[i].name;
    m["noteCount"] = melodies[i].noteCount;
    JsonArray ns = m.createNestedArray("notes");
    for (int j = 0; j < melodies[i].noteCount; j++) {
      JsonObject n = ns.createNestedObject();
      n["bellNumber"] = melodies[i].notes[j].bellNumber;
      n["duration"] = melodies[i].notes[j].duration;
      n["delay"] = melodies[i].notes[j].delay;
    }
  }
  fs::File f = SPIFFS.open(MELODIES_FS, "w");
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

bool loadMelodiesFromFS() {
  if (!SPIFFS.exists(MELODIES_FS)) return true; // niente da caricare
  fs::File f = SPIFFS.open(MELODIES_FS, "r");
  if (!f) return false;
  DynamicJsonDocument doc(32768);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  if (!doc.containsKey("melodies")) return true;
  // Azzerare tutte
  for (int i = 0; i < 10; i++) { melodies[i].isActive = false; melodies[i].noteCount = 0; melodies[i].name[0] = '\0'; }
  for (JsonObject m : doc["melodies"].as<JsonArray>()) {
    int id = m["id"] | -1;
    if (id < 0 || id >= 10) continue;
    const char* name = m["name"] | "Senza nome";
    strncpy(melodies[id].name, name, sizeof(melodies[id].name)-1);
    melodies[id].name[sizeof(melodies[id].name)-1] = '\0';
    JsonArray ns = m["notes"].as<JsonArray>();
    uint8_t count = 0;
    if (ns) {
      for (JsonObject n : ns) {
        uint8_t b = n["bellNumber"] | 1;
        uint16_t d = n["duration"] | 300;
        uint16_t dl = n["delay"] | 800;
        if (b < 1 || b > 2) continue;
        melodies[id].notes[count++] = { b, d, dl };
        if (count >= MAX_MELODY_STEPS) break;
      }
    }
    melodies[id].noteCount = count;
    melodies[id].isActive = (count > 0);
  }
  return true;
}

// === SCAN I2C ===
void scanI2CDevices() {
  byte count = 0;
  for (byte address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    byte error = Wire.endTransmission();
    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      count++;
    }
  }
  Serial.printf("I2C scan complete, found %d device(s)\n", count);
}

// === ARDUINO SETUP/LOOP ===
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Avvio Campane Chiesa ===");

  // Inizializza SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS mount failed");
  }

  // Display
  tft.init();
  tft.setRotation(1); // orizzontale (T-Display)

  // I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  // Stato iniziale
  memset(&systemStatus, 0, sizeof(systemStatus));
  systemStatus.bellsEnabled = AUTO_ENABLE_BELLS_ON_STARTUP;

  // RTC
  if (rtc.begin()) {
    systemStatus.rtcConnected = true;
  } else {
    systemStatus.rtcConnected = false;
  }

  // Bell controller
  bellController.begin();

  // Pulsante fisico per avvio "FUNERALE" (GPIO27 - pulsante esterno)
  pinMode(FUNERAL_BUTTON_PIN, INPUT_PULLUP); // Usa pullup interno

  // Pulsante fisico per avvio "CHIAMATA MESSA" (GPIO32 - pulsante esterno)
  pinMode(MASS_BUTTON_PIN, INPUT_PULLUP); // Usa pullup interno

  // Pulsante di configurazione (GPIO0 - boot button)
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);

  // Carica melodie e schedules da FS
  loadMelodiesFromFS();
  loadSchedulesFromFS();

  // Assicura che le due melodie predefinite esistano
  if (bellController.getMelodyNoteCount(0) == 0) {
    bellController.loadDefaultMelodies();
  }
  if (bellController.getMelodyNoteCount(1) == 0) {
    bellController.loadDefaultMelodies();
  }

  // WiFi e SNTP
  connectWiFi();

  // Web server
  setupWebServer();
  server.begin();
  Serial.println("Web server avviato");

  // Prima schermata
  updateDisplay();
}

void loop() {
  // Aggiorna controller campane (non bloccante)
  bellController.update();

  // Aggiorna temperatura periodicamente
  if (millis() - lastTemperatureCheck >= TEMP_CHECK_INTERVAL) {
    lastTemperatureCheck = millis();
    checkTemperatureThresholds();
  }

  // Aggiornamento display
  if (millis() - lastUpdate >= DISPLAY_UPDATE_INTERVAL) {
    lastUpdate = millis();
    updateDisplay();
  }

  // Controllo programmazioni
  if (millis() - lastScheduleCheck >= SCHEDULE_CHECK_INTERVAL) {
    lastScheduleCheck = millis();
    checkAndRunSchedules();
  }

  // Comandi seriale
  processSerialCommands();

  // Gestione pulsante fisico FUNERALE (debounce + cooldown)
  {
    static uint32_t lastPressMs = 0;
    static bool lastState = true; // pull-up: true=rilasciato
    static uint32_t lastDebugMs = 0;
    
    bool raw = digitalRead(FUNERAL_BUTTON_PIN);
    bool state = raw; // true=alto, false=basso
    
    // Debug semplice ogni 3 secondi
    if (millis() - lastDebugMs > 3000) {
      lastDebugMs = millis();
      Serial.printf("[FUNERAL] GPIO27 = %d\n", raw);
    }
    if (lastState != state) {
      lastState = state;
      Serial.printf("[BUTTON] GPIO27 cambiato: %d -> %d\n", !state, state);
      bool isPressed = FUNERAL_PRESSED_LOW ? (!state) : state;
      Serial.printf("[BUTTON] isPressedLogic: %s (FUNERAL_PRESSED_LOW=%s)\n", 
                   isPressed ? "PREMUTO" : "rilasciato", FUNERAL_PRESSED_LOW ? "true" : "false");
      if (isPressed) { // pressione
        uint32_t now = millis();
        if (now - lastPressMs > 800) { // debounce
          lastPressMs = now;
          // Avvia FUNERALE solo se non sta già suonando qualcosa
          if (!bellController.isPlayingMelody()) {
            Serial.println("FUNERALE: pulsante premuto");
            // Assicura che la melodia 0 esista
            if (bellController.getMelodyNoteCount(0) == 0) {
              bellController.loadDefaultMelodies();
            }
            // Esegui in modalità test per bypassare campane disabilitate (se lo fossero)
            bellController.enableTestMode(true);
            bellController.playMelody(0); // 0 = FUNERALE
          }
        }
      }
    }
    // Fallback: se non ci sono stati cambiamenti per >10s, prova a invertire logica
    static uint32_t lastChangeTime = millis();
    static int lastRawValue = raw;
    if (lastRawValue != raw) {
      lastChangeTime = millis();
      lastRawValue = raw;
    }
    
    if (millis() - lastChangeTime > 10000) {
      Serial.printf("[BUTTON] Nessun cambiamento per 10s, inverto polarità: %s -> %s\n",
                   FUNERAL_PRESSED_LOW ? "LOW" : "HIGH", !FUNERAL_PRESSED_LOW ? "LOW" : "HIGH");
      FUNERAL_PRESSED_LOW = !FUNERAL_PRESSED_LOW;
      lastChangeTime = millis();
    }
  }

  // === PULSANTE CHIAMATA MESSA (GPIO32) ===
  {
    static uint32_t lastMassPressMs = 0;
    static bool lastMassState = true; // pull-up: true=rilasciato
    static uint32_t lastMassDebugMs = 0;
    
    bool massRaw = digitalRead(MASS_BUTTON_PIN);
    bool massState = massRaw; // true=alto, false=basso
    
    // Debug semplice ogni 3 secondi
    if (millis() - lastMassDebugMs > 3000) {
      lastMassDebugMs = millis();
      Serial.printf("[MASS] GPIO32 = %d\n", massRaw);
    }
    
    if (lastMassState != massState) {
      lastMassState = massState;
      Serial.printf("[BUTTON] GPIO32 cambiato: %d -> %d\n", !massState, massState);
      bool isPressed = !massState; // GPIO32 con INPUT_PULLUP: LOW = premuto
      Serial.printf("[BUTTON] isPressedLogic: %s (GPIO32)\n", 
                   isPressed ? "PREMUTO" : "rilasciato");
      if (isPressed) { // pressione
        uint32_t now = millis();
        if (now - lastMassPressMs > 800) { // debounce
          lastMassPressMs = now;
          // Avvia CHIAMATA MESSA solo se non sta già suonando qualcosa
          if (!bellController.isPlayingMelody()) {
            Serial.println("CHIAMATA MESSA: pulsante premuto (GPIO32)");
            // Assicura che la melodia 1 esista
            if (bellController.getMelodyNoteCount(1) == 0) {
              bellController.loadDefaultMelodies();
            }
            // Esegui in modalità test per bypassare campane disabilitate
            bellController.enableTestMode(true);
            bellController.playMelody(1); // 1 = CHIAMATA MESSA
          }
        }
      }
    }
  }

  // Gestione pulsante CONFIG (GPIO35):
  // - breve pressione (<2s): toggle campane abilitate
  // - lunga pressione (>=2s): attiva AP di configurazione
  {
  static bool lastCfgState = false; // GPIO0 boot button
    static uint32_t pressStart = 0;
    static bool longHandled = false;
  // GPIO0 (boot button) è tipicamente ACTIVE-LOW con pullup (premuto = LOW)
  bool pressed = (digitalRead(CONFIG_BUTTON_PIN) == LOW);
    uint32_t now = millis();

    // Edge di pressione
    if (pressed && !lastCfgState) {
      pressStart = now; // inizio pressione
      longHandled = false; // reset gestione long-press
    }
    // Long press: una sola volta per pressione
    if (pressed && !longHandled && (now - pressStart >= 2000)) {
      startApConfig();
      longHandled = true; // non ripetere finché non rilascia
    }
    // Rilascio: se non c'è stata long-press, gestisci come breve
    if (!pressed && lastCfgState) {
      uint32_t held = now - pressStart;
      if (!longHandled && held < 2000) {
        bellController.setEnabled(!bellController.isEnabled());
      }
    }
    lastCfgState = pressed;
  }

  // Coopera con stack async
  delay(0);
  yield();
}

// Avvia Access Point di configurazione in runtime
void startApConfig() {
  if (apMode) return;
  Serial.println("[NET] Attivo Access Point di configurazione...");
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP);
  apMode = true;
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  Serial.print("AP IP: "); Serial.println(ip);
  // Feedback a display
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(4, 4);
  tft.println("CONFIG AP ON");
  tft.setTextSize(1);
  tft.setCursor(4, 28);
  tft.print("SSID: "); tft.println(AP_SSID);
  tft.print("PWD : "); tft.println(AP_PASSWORD);
  tft.print("IP  : "); tft.println(ip.toString());
}

// Funzioni di debug per la programmazione
void debugScheduleCheck() {
    struct tm ti;
    if (!getLocalTm(ti)) {
        // Impossibile ottenere ora locale
        return;
    }
    
    Serial.printf("\n=== DEBUG PROGRAMMAZIONE (ogni minuto) ===\n");
    Serial.printf("Ora attuale: %02d:%02d:%02d - Giorno: %d (0=Dom)\n", 
                  ti.tm_hour, ti.tm_min, ti.tm_sec, ti.tm_wday);
    Serial.printf("Data: %02d/%02d/%04d\n", ti.tm_mday, ti.tm_mon+1, ti.tm_year+1900);
    
    Serial.printf("Stato sistema:\n");
    Serial.printf("  - Campane abilitate: %s\n", systemStatus.bellsEnabled ? "SÌ" : "NO");
    Serial.printf("  - Modalità test: %s\n", systemStatus.testMode ? "SÌ" : "NO");
    Serial.printf("  - RTC connesso: %s\n", systemStatus.rtcConnected ? "SÌ" : "NO");
    Serial.printf("  - NTP sincronizzato: %s\n", systemStatus.ntpSynced ? "SÌ" : "NO");
    
    Serial.printf("\n--- Programmazioni Settimanali Attive (%d totali) ---\n", weeklyCount);
    bool foundActiveWeekly = false;
    for (int i = 0; i < weeklyCount; i++) {
        const WeeklySchedule &e = weeklyArr[i];
        if (e.isActive) {
            foundActiveWeekly = true;
            Serial.printf("[%d] %s: Giorno %d, %02d:%02d, Melodia %d\n",
                         i, e.name, e.dayOfWeek, e.hour, e.minute, e.melodyIndex);
            
            // Verifica se dovrebbe suonare ora
            if ((int)e.dayOfWeek == ti.tm_wday && e.hour == ti.tm_hour && e.minute == ti.tm_min) {
                Serial.printf("    *** DOVREBBE SUONARE ADESSO! ***\n");
                
                // Verifica melodia
                int noteCount = bellController.getMelodyNoteCount(e.melodyIndex);
                if (noteCount > 0) {
                    Serial.printf("    Melodia %d valida (%d note): %s\n", 
                                 e.melodyIndex, noteCount, bellController.getMelodyName(e.melodyIndex));
                } else {
                    Serial.printf("    *** ERRORE: Melodia %d non valida o vuota! ***\n", e.melodyIndex);
                }
            }
        }
    }
    if (!foundActiveWeekly) {
        Serial.println("  Nessuna programmazione settimanale attiva");
    }
    
    Serial.printf("\n--- Eventi Speciali Attivi (%d totali) ---\n", specialCount);
    bool foundActiveSpecial = false;
    for (int i = 0; i < specialCount; i++) {
        const SpecialEvent &e = specialArr[i];
        if (e.isActive) {
            foundActiveSpecial = true;
            Serial.printf("[%d] %s: %02d/%02d/%04d %02d:%02d, Ricorrente: %s\n",
                         i, e.name, e.day, e.month, e.year, e.hour, e.minute,
                         e.isRecurring ? "SÌ" : "NO");
            
            // Verifica se dovrebbe suonare oggi
            bool dateMatch = (e.day == ti.tm_mday && e.month == (ti.tm_mon + 1));
            if (e.isRecurring || e.year == (ti.tm_year + 1900)) {
                if (dateMatch && e.hour == ti.tm_hour && e.minute == ti.tm_min) {
                    Serial.printf("    *** DOVREBBE SUONARE ADESSO! ***\n");
                }
            }
        }
    }
    if (!foundActiveSpecial) {
        Serial.println("  Nessun evento speciale attivo");
    }
    
    Serial.printf("\n--- Melodie Disponibili ---\n");
    bool foundMelodies = false;
    for (int i = 0; i < 10; i++) {
        int noteCount = bellController.getMelodyNoteCount(i);
        if (noteCount > 0) {
            foundMelodies = true;
            Serial.printf("[%d] %s (%d note)\n", i, bellController.getMelodyName(i), noteCount);
        }
    }
    if (!foundMelodies) {
        Serial.println("  Nessuna melodia disponibile!");
    }
    
    Serial.println("=======================================\n");
}

// Funzioni di test per debug programmazione
void testScheduleNow() {
    Serial.println("\n=== TEST PROGRAMMAZIONE IMMEDIATO ===");
    
    struct tm ti;
    if (!getLocalTm(ti)) {
        Serial.println("ERRORE: impossibile ottenere ora locale per test");
        return;
    }
    
    // Crea una programmazione test per il minuto successivo
    int nextMinute = (ti.tm_min + 1) % 60;
    int nextHour = ti.tm_hour;
    if (nextMinute == 0) {
        nextHour = (nextHour + 1) % 24;
    }
    
    WeeklySchedule testSchedule;
    testSchedule.id = 99;
    strcpy(testSchedule.name, "TEST_AUTO");
    testSchedule.dayOfWeek = (DayOfWeek)ti.tm_wday;
    testSchedule.hour = nextHour;
    testSchedule.minute = nextMinute;
    testSchedule.melodyIndex = 0; // Prima melodia disponibile
    testSchedule.isActive = true;
    
    // Trova la prima melodia disponibile
    for (int i = 0; i < 10; i++) {
        if (bellController.getMelodyNoteCount(i) > 0) {
            testSchedule.melodyIndex = i;
            break;
        }
    }
    
    // Sostituisci temporaneamente una programmazione
    WeeklySchedule backup;
    bool hasBackup = false;
    int backupIndex = -1;
    
    if (weeklyCount < MAX_WEEKLY_SCHEDULES) {
        // Aggiungi nuovo slot
        backupIndex = weeklyCount;
        weeklyArr[weeklyCount++] = testSchedule;
    } else {
        // Sostituisci l'ultimo
        backupIndex = weeklyCount - 1;
        backup = weeklyArr[backupIndex];
        hasBackup = true;
        weeklyArr[backupIndex] = testSchedule;
    }
    
    Serial.printf("Test programmato per: %02d:%02d del giorno %d\n", 
                  testSchedule.hour, testSchedule.minute, testSchedule.dayOfWeek);
    Serial.printf("Ora attuale: %02d:%02d del giorno %d\n", 
                  ti.tm_hour, ti.tm_min, ti.tm_wday);
    Serial.printf("Melodia di test: %d (%s)\n", 
                  testSchedule.melodyIndex, bellController.getMelodyName(testSchedule.melodyIndex));
    Serial.println("Aspetto 90 secondi per il trigger...");
    
    // Forza abilitazione campane per test
    bool originalEnabled = systemStatus.bellsEnabled;
    systemStatus.bellsEnabled = true;
    
    // Aspetta e controlla
    unsigned long startTime = millis();
    while (millis() - startTime < 90000) { // 90 secondi
        checkAndRunSchedules();
        delay(1000);
        
        // Mostra countdown ogni 10 secondi
        if ((millis() - startTime) % 10000 < 1000) {
            Serial.printf("Test in corso... %d secondi rimanenti\n", 
                         (90000 - (millis() - startTime)) / 1000);
        }
    }
    
    // Ripristina stato originale
    systemStatus.bellsEnabled = originalEnabled;
    
    if (hasBackup) {
        weeklyArr[backupIndex] = backup;
    } else {
        weeklyCount--;
    }
    
    Serial.println("=== FINE TEST AUTOMATICO ===\n");
}

void processSerialCommands() {
    if (!Serial.available()) return;
    
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toLowerCase();
    
    Serial.printf("\n>>> Comando ricevuto: '%s' <<<\n", command.c_str());
    
    if (command == "help" || command == "?") {
        Serial.println("\n=== COMANDI DISPONIBILI ===");
        Serial.println("help / ?                - Mostra questo menu");
        Serial.println("debug                   - Debug stato programmazioni");
        Serial.println("test_schedule           - Test automatico programmazione");
        Serial.println("play_melody_X           - Suona melodia X (0-9)");
        Serial.println("stop_melody             - Ferma melodia in corso");
        Serial.println("test_relay_X            - Test relè X (1-2)");
        Serial.println("toggle_bells            - Abilita/disabilita campane");
        Serial.println("enable_test_mode        - Abilita modalità test");
        Serial.println("disable_test_mode       - Disabilita modalità test");
        Serial.println("status                  - Mostra stato sistema");
        Serial.println("temp                    - Mostra temperatura ESP32");
        Serial.println("list_melodies           - Lista melodie disponibili");
        Serial.println("list_schedules          - Lista programmazioni");
        Serial.println("force_schedule_check    - Forza controllo programmazioni");
        Serial.println("time                    - Mostra ora corrente");
        Serial.println("enable_bells            - Abilita campane");
        Serial.println("disable_bells           - Disabilita campane");
        Serial.println("i2c / scan              - Scansione dispositivi I2C");
        Serial.println("=========================\n");
        
    } else if (command == "debug") {
        debugScheduleCheck();
        
    } else if (command == "test_schedule") {
        testScheduleNow();
        
    } else if (command.startsWith("play_melody_")) {
        int melodyId = command.substring(12).toInt();
        if (melodyId >= 0 && melodyId < 10) {
            Serial.printf("Riproduzione melodia %d...\n", melodyId);
            bellController.playMelody(melodyId);
        } else {
            Serial.printf("ID melodia non valido: %d (range: 0-9)\n", melodyId);
        }
        
    } else if (command == "stop_melody") {
        Serial.println("Fermando melodia...");
        bellController.stopMelody();
        
    } else if (command.startsWith("test_relay_")) {
        int relayId = command.substring(11).toInt();
        if (relayId >= 1 && relayId <= 2) {
            Serial.printf("Test relè %d per 500ms...\n", relayId);
            bool wasTestMode = testMode;
            testMode = true;
            bellController.ringBell(relayId, 500);
            testMode = wasTestMode;
        } else {
            Serial.printf("ID relè non valido: %d (range: 1-2)\n", relayId);
        }
        
    } else if (command == "toggle_bells") {
        systemStatus.bellsEnabled = !systemStatus.bellsEnabled;
        bellController.setEnabled(systemStatus.bellsEnabled);
        Serial.printf("Campane: %s\n", systemStatus.bellsEnabled ? "ABILITATE" : "DISABILITATE");
        
    } else if (command == "enable_test_mode") {
        testMode = true;
        bellController.enableTestMode(true);
        Serial.println("Modalità test: ABILITATA");
        
    } else if (command == "disable_test_mode") {
        testMode = false;
        bellController.enableTestMode(false);
        Serial.println("Modalità test: DISABILITATA");
        
    } else if (command == "status") {
        Serial.println("\n=== STATO SISTEMA ===");
        Serial.printf("WiFi: %s\n", systemStatus.wifiConnected ? "Connesso" : "Disconnesso");
        Serial.printf("RTC: %s\n", systemStatus.rtcConnected ? "Connesso" : "Disconnesso");
        Serial.printf("NTP: %s\n", systemStatus.ntpSynced ? "Sincronizzato" : "Non sincronizzato");
        Serial.printf("Campane: %s\n", systemStatus.bellsEnabled ? "Abilitate" : "Disabilitate");
        Serial.printf("Test Mode: %s\n", testMode ? "Attivo" : "Disattivo");
        Serial.printf("Melodia in riproduzione: %s\n", bellController.isPlayingMelody() ? "Sì" : "No");
        Serial.printf("Suonate totali: %d\n", systemStatus.totalBellRings);
        Serial.printf("Programmazioni settimanali: %d\n", weeklyCount);
        Serial.printf("Eventi speciali: %d\n", specialCount);
        Serial.printf("Temperatura ESP32: %.1f°C [%s]\n", systemStatus.esp32Temperature, 
                     systemStatus.thermalProtection ? "CRITICA" : 
                     systemStatus.temperatureWarning ? "ELEVATA" : "OK");
        Serial.println("====================\n");
        
    } else if (command == "temp") {
        float temp = getESP32Temperature();
        Serial.printf("🌡️ Temperatura ESP32: %.1f°C\n", temp);
        Serial.printf("Soglie: Avviso=%.1f°C, Critica=%.1f°C, Spegnimento=%.1f°C\n", 
                     TEMP_WARNING_THRESHOLD, TEMP_CRITICAL_THRESHOLD, TEMP_SHUTDOWN_THRESHOLD);
        Serial.printf("Stato: %s\n", getTemperatureStatus().c_str());
        
    } else if (command == "list_melodies") {
        Serial.println("\n=== MELODIE DISPONIBILI ===");
        bool found = false;
        for (int i = 0; i < 10; i++) {
            int noteCount = bellController.getMelodyNoteCount(i);
            if (noteCount > 0) {
                found = true;
                Serial.printf("[%d] %s (%d note)\n", i, bellController.getMelodyName(i), noteCount);
            }
        }
        if (!found) {
            Serial.println("Nessuna melodia disponibile");
        }
        Serial.println("==========================\n");
        
    } else if (command == "list_schedules") {
        Serial.println("\n=== PROGRAMMAZIONI ===");
        Serial.printf("Settimanali (%d):\n", weeklyCount);
        for (int i = 0; i < weeklyCount; i++) {
            if (weeklyArr[i].isActive) {
                Serial.printf("  [%d] %s: Giorno %d, %02d:%02d, Melodia %d\n",
                             i, weeklyArr[i].name, weeklyArr[i].dayOfWeek,
                             weeklyArr[i].hour, weeklyArr[i].minute, weeklyArr[i].melodyIndex);
            }
        }
        Serial.printf("Speciali (%d):\n", specialCount);
        for (int i = 0; i < specialCount; i++) {
            if (specialArr[i].isActive) {
                Serial.printf("  [%d] %s: %02d/%02d/%04d %02d:%02d, Ricorrente: %s\n",
                             i, specialArr[i].name, specialArr[i].day, specialArr[i].month,
                             specialArr[i].year, specialArr[i].hour, specialArr[i].minute,
                             specialArr[i].isRecurring ? "Sì" : "No");
            }
        }
        Serial.println("===================\n");
        
    } else if (command == "force_schedule_check") {
        Serial.println("Forzando controllo programmazioni...");
        lastCheckedMinute = -1; // Reset per forzare il controllo
        checkAndRunSchedules();
        
    } else if (command == "time") {
        struct tm ti;
        if (getLocalTm(ti)) {
            Serial.printf("Ora locale: %02d:%02d:%02d del %02d/%02d/%04d (giorno %d)\n",
                         ti.tm_hour, ti.tm_min, ti.tm_sec,
                         ti.tm_mday, ti.tm_mon+1, ti.tm_year+1900, ti.tm_wday);
        } else {
            Serial.println("Impossibile ottenere ora locale");
        }
        
    } else if (command == "enable_bells") {
        systemStatus.bellsEnabled = true;
        bellsEnabled = true;
        bellController.setEnabled(true);
        Serial.println("🔔 CAMPANE ABILITATE manualmente");
        
    } else if (command == "disable_bells") {
        systemStatus.bellsEnabled = false;
        bellsEnabled = false;
        bellController.setEnabled(false);
        Serial.println("🔇 CAMPANE DISABILITATE manualmente");
        Serial.println("⚠️ ATTENZIONE: Le campane rimarranno disabilitate fino al prossimo riavvio");
        Serial.println("💡 Per riabilitare: 'enable_bells' o riavvio sistema");
        
    } else if (command == "i2c" || command == "scan") {
        Serial.println("=== SCANSIONE DISPOSITIVI I2C ===");
        scanI2CDevices();
        
    } else {
        Serial.printf("Comando sconosciuto: '%s'. Digita 'help' per l'elenco comandi.\n", command.c_str());
    }
    
    Serial.println(">>> Pronto per nuovo comando <<<\n");
}

void checkAndRunSchedules(){
  struct tm ti; 
  if (!getLocalTm(ti)) {
    Serial.println("[SCHED] ERRORE: impossibile ottenere ora locale");
    return;
  }
  
  // Debug dettagliato ogni cambio di minuto
  static int lastDebugMinute = -1;
  if (DEBUG_SCHEDULING && ti.tm_min != lastDebugMinute) {
    lastDebugMinute = ti.tm_min;
    debugScheduleCheck();
  }
  
  // Check once per minute - ma con controllo sui secondi per evitare doppi trigger
  if (ti.tm_min == lastCheckedMinute) return;
  lastCheckedMinute = ti.tm_min;
  
  Serial.printf("[SCHED] Controllo programmazioni per %02d:%02d del giorno %d\n", 
               ti.tm_hour, ti.tm_min, ti.tm_wday);
  
  int dow = ti.tm_wday; // 0=dom
  bool scheduleFound = false;
  
  // Controllo programmazioni settimanali
  for (int i=0;i<weeklyCount;i++){
    const WeeklySchedule &e = weeklyArr[i];
    if (!e.isActive) continue;
    
    Serial.printf("[SCHED] Controllo weekly[%d]: %s - Giorno %d vs %d, Ora %02d:%02d vs %02d:%02d\n",
                 i, e.name, e.dayOfWeek, dow, e.hour, e.minute, ti.tm_hour, ti.tm_min);
    
    if ((int)e.dayOfWeek == dow && e.hour == ti.tm_hour && e.minute == ti.tm_min){
      scheduleFound = true;
      Serial.printf("[SCHED] *** MATCH TROVATO: %s ***\n", e.name);
      
      // Verifica melodia
      int noteCount = bellController.getMelodyNoteCount(e.melodyIndex);
      if (noteCount <= 0) {
        Serial.printf("[SCHED] ERRORE: Melodia %d non valida (noteCount=%d)\n", e.melodyIndex, noteCount);
        continue;
      }
      
      // Verifica stato campane
      if (!systemStatus.bellsEnabled) {
        Serial.printf("[SCHED] AVVISO: Campane disabilitate, saltando '%s'\n", e.name);
        continue;
      }
      
      Serial.printf("[SCHED] ESECUZIONE: '%s' -> melodia %d (%s, %d note)\n", 
                   e.name, e.melodyIndex, bellController.getMelodyName(e.melodyIndex), noteCount);
      
      bellController.playMelody(e.melodyIndex);
      
      // Aggiorna statistiche
      systemStatus.totalBellRings++;
      systemStatus.lastBellTime = millis();
      systemStatus.activeMelody = e.melodyIndex;
      
      return; // Esci dopo aver trovato ed eseguito un evento
    }
  }
  
  // Controllo eventi speciali
  for (int i=0;i<specialCount;i++){
    SpecialEvent &e = specialArr[i];
    if (!e.isActive) continue;
    
    bool dateMatch = (e.day == ti.tm_mday && e.month == (ti.tm_mon + 1));
    bool yearMatch = e.isRecurring || (e.year == (ti.tm_year + 1900));
    
    Serial.printf("[SCHED] Controllo special[%d]: %s - Data %02d/%02d/%04d vs %02d/%02d/%04d, Ora %02d:%02d vs %02d:%02d\n",
                 i, e.name, e.day, e.month, e.year, ti.tm_mday, ti.tm_mon+1, ti.tm_year+1900,
                 e.hour, e.minute, ti.tm_hour, ti.tm_min);
    
    if (dateMatch && yearMatch && e.hour == ti.tm_hour && e.minute == ti.tm_min){
      scheduleFound = true;
      Serial.printf("[SCHED] *** MATCH EVENTO SPECIALE: %s ***\n", e.name);
      
      int noteCount = bellController.getMelodyNoteCount(e.melodyIndex);
      if (noteCount <= 0) {
        Serial.printf("[SCHED] ERRORE: Melodia %d non valida per evento speciale\n", e.melodyIndex);
        continue;
      }
      
      if (!systemStatus.bellsEnabled) {
        Serial.printf("[SCHED] AVVISO: Campane disabilitate, saltando evento '%s'\n", e.name);
        continue;
      }
      
      Serial.printf("[SCHED] ESECUZIONE EVENTO: '%s' -> melodia %d\n", e.name, e.melodyIndex);
      
      bellController.playMelody(e.melodyIndex);
      systemStatus.totalBellRings++;
      systemStatus.lastBellTime = millis();
      
      if (!e.isRecurring) {
        e.isActive = false; // one-shot consumed
        Serial.printf("[SCHED] Evento non ricorrente '%s' completato e disattivato\n", e.name);
        saveSchedulesToFS(); // Salva il cambiamento
      }
      
      return;
    }
  }
  
  if (!scheduleFound && DEBUG_SCHEDULING) {
    Serial.printf("[SCHED] Nessuna programmazione trovata per %02d:%02d del giorno %d\n", 
                 ti.tm_hour, ti.tm_min, ti.tm_wday);
  }
}

void connectWiFi() {
  Serial.println("[DEBUG] Chiamata: connectWiFi()");
  Serial.println("=== INIZIO CONNESSIONE WiFi ===");
  
  // Riduci potenza WiFi per evitare surriscaldamento
  WiFi.setTxPower(WIFI_POWER_11dBm); // Riduce da 20dBm a 11dBm
  // Disabilita power save per evitare latenze che possono far scattare il WDT
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  Serial.println("📡 Potenza WiFi impostata a 11dBm per ridurre riscaldamento");
  
  // Carica le credenziali salvate
  if (SPIFFS.exists("/wifi_config.json")) {
    fs::File file = SPIFFS.open("/wifi_config.json", "r");
    if (file) {
      DynamicJsonDocument doc(512);
      deserializeJson(doc, file);
      
      String savedSSID = doc["ssid"];
      String savedPassword = doc["password"];
      
      if (savedSSID.length() > 0 && savedPassword.length() > 0) {
        Serial.print("SSID: ");
        Serial.println(savedSSID);
        
        // Avvia connessione STA
        WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
        Serial.println("Tentativo connessione...");
        
        int attempts = 0;
        while (WiFi.status() != WL_CONNECTED && attempts < 20) { // Timeout più breve e non bloccante
          for (int i = 0; i < 10; i++) { // 10 x 100ms = 1s
            delay(100);
            yield();
            // Opportunistic feed per Async stack
            delay(0);
          }
          Serial.print(".");
          attempts++;
        }
        
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("");
          Serial.println("✓ CONNESSIONE WiFi RIUSCITA!");
          Serial.print("✓ Indirizzo IP: ");
          Serial.println(WiFi.localIP());
          systemStatus.wifiConnected = true;
          apMode = false;
          
          // Riduci ulteriormente la potenza una volta connesso
          WiFi.setTxPower(WIFI_POWER_8_5dBm); // Ancora più bassa per uso continuo
          Serial.println("📡 Potenza WiFi ridotta a 8.5dBm per prevenire surriscaldamento");
          
          // Inizializza SNTP con fuso orario Italia e attendi sync
          initSNTP(true);
          
          Serial.println("=== FINE CONNESSIONE WiFi ===");
          return;
        }
      }
      file.close();
    }
  }
  
  Serial.println("");
  Serial.println("✗ CONNESSIONE WiFi FALLITA!");
  Serial.println("=== FINE CONNESSIONE WiFi ===");
  
  systemStatus.wifiConnected = false;
  apMode = true;
  
  // Modalità AP di backup
  Serial.println("⚠️ Connessione WiFi fallita, attivazione modalità AP...");
  Serial.println("🔄 Avvio modalità Access Point di backup...");
  
  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  
  Serial.print("✓ Access Point attivo: ");
  Serial.println(AP_SSID);
  Serial.print("✓ Password: ");
  Serial.println(AP_PASSWORD);
  Serial.print("✓ IP AP: ");
  Serial.println(WiFi.softAPIP());
}

void setupWebServer() {
  Serial.println("[DEBUG] Chiamata: setupWebServer()");
  Serial.println("Configurazione Web Server...");
  
  // Nota: registrare PRIMA le API e SOLO ALLA FINE lo static handler.
  // Se lo static handler viene registrato per primo su "/", intercetta anche /api/* e risponde 404,
  // impedendo alle API di funzionare.
  
  // API per ottenere lo stato del sistema
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request){
    static uint32_t lastMs = 0;
    uint32_t now = millis();
    if (now - lastMs < 150) { // troppo frequente: alleggeriamo
      request->send(429, "text/plain", "Too Many Requests");
      return;
    }
    lastMs = now;
    Serial.println("📡 Richiesta ricevuta: /api/status");
    DynamicJsonDocument doc(1024);
    doc["wifiConnected"] = systemStatus.wifiConnected;
    doc["ntpSynced"] = systemStatus.ntpSynced;
    doc["rtcConnected"] = systemStatus.rtcConnected;
    doc["bellsEnabled"] = systemStatus.bellsEnabled;
    doc["testMode"] = testMode;
    doc["schedulerActive"] = schedulerActive;
    doc["apMode"] = apMode;
    doc["timezoneOffset"] = currentTimezoneOffset;
    doc["isDST"] = isDST;
    doc["firmwareVersion"] = FIRMWARE_VERSION;
    // Uptime (ms dall'avvio) e orario di avvio (epoch se disponibile)
    doc["uptimeMs"] = (uint32_t)millis();
    uint32_t bootEpoch = 0;
    if (systemStatus.ntpSynced) {
      time_t now = time(nullptr);
      if (now > 0) bootEpoch = (uint32_t)now - (millis() / 1000);
    } else if (systemStatus.rtcConnected) {
      DateTime now = rtc.now();
      bootEpoch = (uint32_t)now.unixtime() - (millis() / 1000);
    }
    if (bootEpoch > 0) doc["bootEpoch"] = bootEpoch;
    // Aggiungi campi utili alla UI
    doc["totalBellRings"] = systemStatus.totalBellRings;
    doc["lastBellTime"] = systemStatus.lastBellTime;
        
    // Informazioni temperatura ESP32
    doc["esp32Temperature"] = systemStatus.esp32Temperature;
    doc["temperatureWarning"] = systemStatus.temperatureWarning;
    doc["thermalProtection"] = systemStatus.thermalProtection;
    doc["temperatureStatus"] = getTemperatureStatus();
        
    // Descrizione fuso orario
    String tzDesc = String("Italia UTC+") + String(utcOffsetHours) + (isDST ? " (Ora Legale)" : " (Ora Solare)");
    doc["timezoneDescription"] = tzDesc;
        
    if (systemStatus.wifiConnected) {
      String ip = WiFi.localIP().toString();
      doc["ipAddress"] = ip;
      doc["wifiIP"] = ip;
    } else {
      String apIp = WiFi.softAPIP().toString();
      doc["ipAddress"] = apIp;
      doc["apIP"] = apIp;
    }
        
  String response;
  serializeJson(doc, response);
  AsyncWebServerResponse* r = request->beginResponse(200, "application/json", response);
  r->addHeader("Connection", "close");
  request->send(r);
        
    Serial.println("✓ Risposta inviata: status JSON");
  });
  
  // API per ottenere le melodie
  server.on("/api/melodies", HTTP_GET, [](AsyncWebServerRequest *request){
    DynamicJsonDocument doc(2048);
    JsonArray melodies = doc.createNestedArray("melodies");
        
    for (int i = 0; i < 10; i++) {
      yield();
      if (bellController.getMelodyNoteCount(i) > 0) {
        JsonObject melody = melodies.createNestedObject();
        String melodyName = bellController.getMelodyName(i);
        melody["id"] = i;
        melody["name"] = melodyName;
        melody["noteCount"] = bellController.getMelodyNoteCount(i);
        melody["duration"] = bellController.getMelodyDuration(i);
        melody["isActive"] = true;
      }
    }
        
  String response;
  serializeJson(doc, response);
  AsyncWebServerResponse* r = request->beginResponse(200, "application/json", response);
  r->addHeader("Connection", "close");
  request->send(r);
        
    Serial.println("✓ Risposta inviata: melodie JSON");
  });

  // API: dettagli melodia singola
  server.on("/api/melody", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->hasParam("index")) { request->send(400, "application/json", "{\"success\":false,\"message\":\"index mancante\"}"); return; }
    int idx = request->getParam("index")->value().toInt();
    if (idx < 0 || idx >= 10) { request->send(400, "application/json", "{\"success\":false,\"message\":\"index non valido\"}"); return; }
    int count = bellController.getMelodyNoteCount(idx);
    if (count <= 0) { request->send(404, "application/json", "{\"success\":false,\"message\":\"melodia vuota\"}"); return; }
    DynamicJsonDocument doc(4096);
    doc["index"] = idx;
    doc["name"] = bellController.getMelodyName(idx);
    doc["noteCount"] = count;
    JsonArray ns = doc.createNestedArray("notes");
    const BellNote* notes = bellController.getMelodyNotes(idx);
    for (int i=0;i<count;i++){
      JsonObject n = ns.createNestedObject();
      n["bellNumber"] = notes[i].bellNumber;
      n["duration"] = notes[i].duration;
      n["delay"] = notes[i].delay;
    }
  String resp; serializeJson(doc, resp);
  AsyncWebServerResponse* r = request->beginResponse(200, "application/json", resp);
  r->addHeader("Connection", "close");
  request->send(r);
  });

  // API orario corrente per UI
  server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request){
    DynamicJsonDocument doc(256);
    char bufTime[20];
    char bufDate[20];
    if (systemStatus.rtcConnected) {
      DateTime now = rtc.now();
      snprintf(bufTime, sizeof(bufTime), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
      snprintf(bufDate, sizeof(bufDate), "%02d/%02d/%04d", now.day(), now.month(), now.year());
    } else if (manualTimeValid) {
      snprintf(bufTime, sizeof(bufTime), "%02d:%02d:%02d", manualTime.hour(), manualTime.minute(), manualTime.second());
      snprintf(bufDate, sizeof(bufDate), "%02d/%02d/%04d", manualTime.day(), manualTime.month(), manualTime.year());
    } else if (systemStatus.ntpSynced) {
      time_t now = time(nullptr);
      struct tm * timeinfo = localtime(&now);
      snprintf(bufTime, sizeof(bufTime), "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
      snprintf(bufDate, sizeof(bufDate), "%02d/%02d/%04d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900);
    } else {
      // fallback a --:--:--
      strlcpy(bufTime, "--:--:--", sizeof(bufTime));
      strlcpy(bufDate, "--/--/----", sizeof(bufDate));
    }
    doc["time"] = String(bufTime);
    doc["date"] = String(bufDate);
    String resp; serializeJson(doc, resp);
    request->send(200, "application/json", resp);
  });
  
  // API per ottenere programmazioni settimanali
  server.on("/api/weekly-schedules", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/weekly-schedules (GET)");
    DynamicJsonDocument doc(16384);
    JsonArray arr = doc.createNestedArray("schedules");
    for (int i=0;i<weeklyCount;i++){
      if ((i & 3) == 0) { yield(); }
      const WeeklySchedule &e = weeklyArr[i];
      JsonObject o = arr.createNestedObject();
      o["id"]=e.id; o["name"]=e.name; o["dayOfWeek"]=e.dayOfWeek; o["hour"]=e.hour; o["minute"]=e.minute; o["melodyIndex"]=e.melodyIndex; o["isActive"]=e.isActive;
    }
  String response; serializeJson(doc, response);
  AsyncWebServerResponse* r = request->beginResponse(200, "application/json", response);
  r->addHeader("Connection", "close");
  request->send(r);
  });
  server.on("/api/weekly-schedules", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    Serial.println("📡 Richiesta ricevuta: /api/weekly-schedules (POST)");
    // Accumula i chunk del body
    if (index == 0) {
      request->_tempObject = new String();
      ((String*)request->_tempObject)->reserve(total);
    }
    String* body = (String*)request->_tempObject;
    body->concat((const char*)data, len);
    if (index + len < total) { yield(); return; }
    // Fine: parse JSON
  DynamicJsonDocument doc(24576);
    DeserializationError err = deserializeJson(doc, *body);
    delete body; request->_tempObject = nullptr;
    if (err) { request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON non valido\"}"); return; }
    if (!doc.containsKey("schedules") || !doc["schedules"].is<JsonArray>()){
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Campo schedules mancante\"}"); return;
    }
    weeklyCount = 0;
    for (JsonObject o : doc["schedules"].as<JsonArray>()){
      if (weeklyCount>=MAX_WEEKLY_SCHEDULES) break;
      WeeklySchedule &e = weeklyArr[weeklyCount++];
      strlcpy(e.name, (o["name"] | ""), sizeof(e.name));
      e.id = o["id"] | weeklyCount; e.dayOfWeek = (DayOfWeek)(int)(o["dayOfWeek"] | 0);
      e.hour = o["hour"] | 0; e.minute = o["minute"] | 0; e.melodyIndex = o["melodyIndex"] | 0; e.isActive = o["isActive"] | true;
      if ((weeklyCount & 3) == 0) { yield(); }
    }
    saveSchedulesToFS();
    request->send(200, "application/json", "{\"success\":true}");
  });
  
  // API per ottenere eventi speciali
  server.on("/api/special-events", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/special-events (GET)");
    DynamicJsonDocument doc(8192);
    JsonArray arr = doc.createNestedArray("events");
    for (int i=0;i<specialCount;i++){
      if ((i & 3) == 0) { yield(); }
      const SpecialEvent &e = specialArr[i];
      JsonObject o = arr.createNestedObject();
      o["id"]=e.id; o["name"]=e.name; o["type"]=e.type; o["year"]=e.year; o["month"]=e.month; o["day"]=e.day; o["hour"]=e.hour; o["minute"]=e.minute; o["melodyIndex"]=e.melodyIndex; o["isActive"]=e.isActive; o["isRecurring"]=e.isRecurring;
    }
  String response; serializeJson(doc, response);
  AsyncWebServerResponse* r = request->beginResponse(200, "application/json", response);
  r->addHeader("Connection", "close");
  request->send(r);
  });
  server.on("/api/special-events", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    Serial.println("📡 Richiesta ricevuta: /api/special-events (POST)");
    if (index == 0) {
      request->_tempObject = new String();
      ((String*)request->_tempObject)->reserve(total);
    }
    String* body = (String*)request->_tempObject;
    body->concat((const char*)data, len);
    if (index + len < total) { yield(); return; }
    DynamicJsonDocument doc(12288);
    DeserializationError err = deserializeJson(doc, *body);
    delete body; request->_tempObject = nullptr;
    if (err) { request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON non valido\"}"); return; }
    if (!doc.containsKey("events") || !doc["events"].is<JsonArray>()){
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Campo events mancante\"}"); return;
    }
    specialCount = 0;
    for (JsonObject o : doc["events"].as<JsonArray>()){
      if (specialCount>=MAX_SPECIAL_EVENTS) break;
      SpecialEvent &e = specialArr[specialCount++];
      strlcpy(e.name, (o["name"] | ""), sizeof(e.name));
      e.id = o["id"] | specialCount; e.type = (EventType)(int)(o["type"] | 5);
      e.year = o["year"] | 0; e.month = o["month"] | 0; e.day = o["day"] | 0;
      e.hour = o["hour"] | 0; e.minute = o["minute"] | 0; e.melodyIndex = o["melodyIndex"] | 0; e.isActive = o["isActive"] | true; e.isRecurring = o["isRecurring"] | false;
      if ((specialCount & 3) == 0) { yield(); }
    }
    saveSchedulesToFS();
    request->send(200, "application/json", "{\"success\":true}");
  });

  // API: forza resync SNTP
  server.on("/api/ntp-resync", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/ntp-resync");
    if (!systemStatus.wifiConnected) {
      request->send(503, "application/json", "{\"success\":false,\"message\":\"WiFi non connesso\"}");
      return;
    }
    initSNTP(true);
    String resp = String("{\"success\":") + (systemStatus.ntpSynced?"true":"false") + ",\"ntpSynced\":" + (systemStatus.ntpSynced?"true":"false") + "}";
    request->send(200, "application/json", resp);
  });

  // API: backup completo (melodie + schedules). WiFi escluso per sicurezza.
  server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncResponseStream* s = request->beginResponseStream("application/json");
    // Header
    s->print('{');
    s->print("\"firmwareVersion\":\""); s->print(FIRMWARE_VERSION); s->print("\",");
    // timestamp
    time_t now = time(nullptr); struct tm *ti = localtime(&now);
    char ts[32]; if (ti) snprintf(ts, sizeof(ts), "%04d-%02d-%02dT%02d:%02d:%02d", ti->tm_year+1900, ti->tm_mon+1, ti->tm_mday, ti->tm_hour, ti->tm_min, ti->tm_sec); else strlcpy(ts, "unknown", sizeof(ts));
    s->print("\"timestamp\":\""); s->print(ts); s->print("\",");

    // Melodies
    s->print("\"melodies\":[");
    bool firstMel = true;
    for (int i=0;i<10;i++){
      if ((i & 1) == 0) { yield(); }
      int cnt = bellController.getMelodyNoteCount(i);
      if (cnt <= 0) continue;
      if (!firstMel) s->print(','); firstMel = false;
      s->print('{');
      s->print("\"id\":"); s->print(i); s->print(',');
      s->print("\"name\":\""); s->print(bellController.getMelodyName(i)); s->print("\",");
      s->print("\"notes\":[");
      bool firstNote = true;
      const BellNote* notes = bellController.getMelodyNotes(i);
      for (int j=0;j<cnt;j++){
        if ((j & 3) == 0) { yield(); }
        if (!firstNote) s->print(','); firstNote = false;
        s->print('{');
        s->print("\"bellNumber\":"); s->print(notes[j].bellNumber); s->print(',');
        s->print("\"duration\":"); s->print(notes[j].duration); s->print(',');
        s->print("\"delay\":"); s->print(notes[j].delay);
        s->print('}');
      }
      s->print("]}");
    }
    s->print(']');

    // Weekly
    s->print(','); s->print("\"weekly\":[");
    for (int i=0;i<weeklyCount;i++){
      if ((i & 3) == 0) { yield(); }
      if (i>0) s->print(',');
      const WeeklySchedule &e = weeklyArr[i];
      s->print('{');
      s->print("\"id\":"); s->print(e.id); s->print(',');
      s->print("\"name\":\""); s->print(e.name); s->print("\",");
      s->print("\"dayOfWeek\":"); s->print(e.dayOfWeek); s->print(',');
      s->print("\"hour\":"); s->print(e.hour); s->print(',');
      s->print("\"minute\":"); s->print(e.minute); s->print(',');
      s->print("\"melodyIndex\":"); s->print(e.melodyIndex); s->print(',');
      s->print("\"isActive\":"); s->print(e.isActive ? "true" : "false");
      s->print('}');
    }
    s->print(']');

    // Special
    s->print(','); s->print("\"special\":[");
    for (int i=0;i<specialCount;i++){
      if ((i & 3) == 0) { yield(); }
      if (i>0) s->print(',');
      const SpecialEvent &e = specialArr[i];
      s->print('{');
      s->print("\"id\":"); s->print(e.id); s->print(',');
      s->print("\"name\":\""); s->print(e.name); s->print("\",");
      s->print("\"type\":"); s->print(e.type); s->print(',');
      s->print("\"year\":"); s->print(e.year); s->print(',');
      s->print("\"month\":"); s->print(e.month); s->print(',');
      s->print("\"day\":"); s->print(e.day); s->print(',');
      s->print("\"hour\":"); s->print(e.hour); s->print(',');
      s->print("\"minute\":"); s->print(e.minute); s->print(',');
      s->print("\"melodyIndex\":"); s->print(e.melodyIndex); s->print(',');
      s->print("\"isActive\":"); s->print(e.isActive ? "true" : "false"); s->print(',');
      s->print("\"isRecurring\":"); s->print(e.isRecurring ? "true" : "false");
      s->print('}');
    }
    s->print(']');

    // Chiudi array principale
    s->print('}');
    request->send(s);
  });

  // API: restore completo (melodie + schedules). WiFi escluso.
  server.on("/api/restore", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    Serial.println("📡 Richiesta ricevuta: /api/restore");
    if (index == 0) {
      request->_tempObject = new String();
      ((String*)request->_tempObject)->reserve(total);
    }
    String* body = (String*)request->_tempObject;
    body->concat((const char*)data, len);
    if (index + len < total) { yield(); return; }
    DynamicJsonDocument doc(32768);
    DeserializationError err = deserializeJson(doc, *body);
    delete body; request->_tempObject = nullptr;
    if (err) { request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON non valido\"}"); return; }
    // Import melodie (opzionale)
    int importedMel = 0;
    if (doc.containsKey("melodies") && doc["melodies"].is<JsonArray>()){
      // Svuota
      for (int i=0;i<10;i++) { bellController.deleteMelody(i); if ((i & 1)==0) yield(); }
      for (JsonObject m : doc["melodies"].as<JsonArray>()){
        yield();
        String name = m["name"] | "Senza nome";
        JsonArray ns = m["notes"].as<JsonArray>();
        if (!ns) continue;
        BellNote temp[MAX_MELODY_STEPS]; uint8_t count=0;
        for (JsonObject n : ns){
          if ((count & 3)==0) yield();
          int b = n["bellNumber"] | 1; int d = n["duration"] | 300; int dl = n["delay"] | 800;
          if (b<1 || b>2) continue; temp[count++] = { (uint8_t)b, (uint16_t)d, (uint16_t)dl }; if (count>=MAX_MELODY_STEPS) break;
        }
        if (count>0) { if (bellController.addMelody(name.c_str(), temp, count)) importedMel++; }
      }
      saveAllMelodiesToFS();
    }
    // Import schedules (opzionale)
    int importedWeekly = 0;
    if (doc.containsKey("weekly") && doc["weekly"].is<JsonArray>()){
      weeklyCount = 0;
      for (JsonObject o : doc["weekly"].as<JsonArray>()){
        if ((weeklyCount & 3)==0) yield();
        if (weeklyCount>=MAX_WEEKLY_SCHEDULES) break;
        WeeklySchedule &e = weeklyArr[weeklyCount++];
        strlcpy(e.name, (o["name"] | ""), sizeof(e.name));
        e.id = o["id"] | weeklyCount; e.dayOfWeek = (DayOfWeek)(int)(o["dayOfWeek"] | 0);
        e.hour = o["hour"] | 0; e.minute = o["minute"] | 0; e.melodyIndex = o["melodyIndex"] | 0; e.isActive = o["isActive"] | true;
        importedWeekly++;
      }
    }
    int importedSpecial = 0;
    if (doc.containsKey("special") && doc["special"].is<JsonArray>()){
      specialCount = 0;
      for (JsonObject o : doc["special"].as<JsonArray>()){
        if ((specialCount & 3)==0) yield();
        if (specialCount>=MAX_SPECIAL_EVENTS) break;
        SpecialEvent &e = specialArr[specialCount++];
        strlcpy(e.name, (o["name"] | ""), sizeof(e.name));
        e.id = o["id"] | specialCount; e.type = (EventType)(int)(o["type"] | 5);
        e.year = o["year"] | 0; e.month = o["month"] | 0; e.day = o["day"] | 0;
        e.hour = o["hour"] | 0; e.minute = o["minute"] | 0; e.melodyIndex = o["melodyIndex"] | 0; e.isActive = o["isActive"] | true; e.isRecurring = o["isRecurring"] | false;
        importedSpecial++;
      }
    }
    saveSchedulesToFS();
    // Assicura preset se mancanti (slot 0/1)
    if (bellController.getMelodyNoteCount(0) == 0 || bellController.getMelodyNoteCount(1) == 0) {
      bellController.loadDefaultMelodies();
    }
    // Risposta dettagliata
    {
      DynamicJsonDocument resp(256);
      resp["success"] = true;
      JsonObject imp = resp.createNestedObject("imported");
      imp["melodies"] = importedMel;
      imp["weekly"] = importedWeekly;
      imp["special"] = importedSpecial;
      String s; serializeJson(resp, s);
      AsyncWebServerResponse* r = request->beginResponse(200, "application/json", s);
      r->addHeader("Connection", "close");
      request->send(r);
    }
  });

  

  // API diagnostica: scansione I2C (mostra indirizzi trovati)
  server.on("/api/i2c-scan", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/i2c-scan");
    DynamicJsonDocument doc(512);
    JsonArray arr = doc.createNestedArray("devices");
    uint8_t count = 0;
    for (uint8_t address = 1; address < 127; address++) {
      Wire.beginTransmission(address);
      uint8_t error = Wire.endTransmission();
      if (error == 0) {
        JsonObject d = arr.createNestedObject();
        d["address"] = address;
        d["hex"] = String("0x") + String(address, HEX);
        count++;
      }
    }
    doc["count"] = count;
    String resp; serializeJson(doc, resp);
    request->send(200, "application/json", resp);
    Serial.printf("✓ I2C scan: %d dispositivi\n", count);
  });
  
  // API per test melodia
  server.on("/api/test-melody", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    Serial.println("📡 Richiesta ricevuta: /api/test-melody");
    if (index == 0) {
      request->_tempObject = new String();
      ((String*)request->_tempObject)->reserve(total);
    }
    String* body = (String*)request->_tempObject;
    body->concat((const char*)data, len);
    if (index + len < total) { yield(); return; }
    DynamicJsonDocument doc(12288);
    DeserializationError derr = deserializeJson(doc, *body);
    delete body; request->_tempObject = nullptr;
    if (derr) { request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON non valido\"}"); return; }
    
    // Test per ID oppure per sequenza di note ad-hoc
  if (doc.containsKey("notes") && doc["notes"].is<JsonArray>()) {
      JsonArray ns = doc["notes"].as<JsonArray>();
      const size_t maxSteps = min((size_t)MAX_MELODY_STEPS, ns.size());
      BellNote tempNotes[MAX_MELODY_STEPS];
      uint8_t count = 0;
      for (size_t i = 0; i < maxSteps; i++) {
        JsonObject n = ns[i];
        int b = n["bellNumber"] | 1;
        int d = n["duration"] | 300;
        int dl = n["delay"] | 800;
        if (b < 1 || b > 2) continue;
        tempNotes[count++] = { (uint8_t)b, (uint16_t)d, (uint16_t)dl };
      }
      if (count == 0) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Nessuna nota valida\"}");
        return;
      }
      // Trova primo slot libero; se non c'è, libera l'ultimo (9)
      int freeIdx = -1;
      for (int i = 0; i < 10; i++) {
        if (bellController.getMelodyNoteCount(i) == 0) { freeIdx = i; break; }
      }
      if (freeIdx < 0) { bellController.deleteMelody(9); freeIdx = 9; }
      bool ok = bellController.addMelody("Test", tempNotes, count);
      if (!ok) { request->send(500, "application/json", "{\"success\":false,\"message\":\"Impossibile aggiungere melodia\"}"); return; }
      // Se l'algoritmo di add ha usato un altro slot (primo libero), individua l'indice effettivo
      int playIdx = freeIdx;
      for (int i = 0; i < 10; i++) {
        if (bellController.getMelodyNoteCount(i) == count && String(bellController.getMelodyName(i)) == String("Test")) { playIdx = i; break; }
      }
      bellController.playMelody(playIdx);
      request->send(200, "application/json", String("{\"success\":true,\"message\":\"Test melodia ad-hoc avviato\",\"index\":") + playIdx + "}");
      Serial.printf("✓ Test melodia ad-hoc con %d note (slot %d)\n", count, playIdx);
    } else {
      int melodyId = doc["melodyId"] | -1;
      if (melodyId >= 0 && melodyId < 10 && bellController.getMelodyNoteCount(melodyId) > 0) {
        bellController.playMelody(melodyId);
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Melodia in riproduzione\"}");
        Serial.printf("✓ Riproduzione melodia ID: %d\n", melodyId);
      } else {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"ID melodia non valido\"}");
        Serial.println("❌ ID melodia non valido");
      }
    }
  });

  // API salva melodia
  server.on("/api/save-melody", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    Serial.println("📡 Richiesta ricevuta: /api/save-melody");
    if (index == 0) {
      request->_tempObject = new String();
      ((String*)request->_tempObject)->reserve(total);
    }
    String* body = (String*)request->_tempObject;
    body->concat((const char*)data, len);
    if (index + len < total) { yield(); return; }
    DynamicJsonDocument doc(12288);
    DeserializationError err = deserializeJson(doc, *body);
    delete body; request->_tempObject = nullptr;
    if (err) {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON non valido\"}");
      return;
    }
    String name = doc["name"] | "Senza nome";
    JsonArray ns = doc["notes"].as<JsonArray>();
    if (!ns || ns.size() == 0) {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Note mancanti\"}");
      return;
    }
    BellNote temp[MAX_MELODY_STEPS];
    uint8_t count = 0;
    for (JsonObject n : ns) {
      int b = n["bellNumber"] | 1;
      int d = n["duration"] | 300;
      int dl = n["delay"] | 800;
      if (b < 1 || b > 2) continue;
      temp[count++] = { (uint8_t)b, (uint16_t)d, (uint16_t)dl };
      if (count >= MAX_MELODY_STEPS) break;
    }
    if (count == 0) {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Nessuna nota valida\"}");
      return;
    }
    bool ok = bellController.addMelody(name.c_str(), temp, count);
    if (!ok) {
      request->send(500, "application/json", "{\"success\":false,\"message\":\"Nessuno slot libero\"}");
      return;
    }
    // Trova indice assegnato
    int assigned = -1;
    for (int i=0;i<10;i++){
      if (bellController.getMelodyNoteCount(i) == count && String(bellController.getMelodyName(i)) == name) { assigned = i; break; }
    }
    saveAllMelodiesToFS();
    String resp = String("{\"success\":true,\"index\":") + (assigned>=0? String(assigned): String(-1)) + "}";
    request->send(200, "application/json", resp);
  });

  // API aggiorna melodia esistente
  server.on("/api/update-melody", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    Serial.println("📡 Richiesta ricevuta: /api/update-melody");
    if (index == 0) {
      request->_tempObject = new String();
      ((String*)request->_tempObject)->reserve(total);
    }
    String* body = (String*)request->_tempObject;
    body->concat((const char*)data, len);
    if (index + len < total) { yield(); return; }
    DynamicJsonDocument doc(12288);
    DeserializationError err = deserializeJson(doc, *body);
    delete body; request->_tempObject = nullptr;
    if (err) { request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON non valido\"}"); return; }
    int idx = doc["index"] | -1;
    if (idx < 0 || idx >= 10) { request->send(400, "application/json", "{\"success\":false,\"message\":\"index non valido\"}"); return; }
    String name = doc["name"] | "Senza nome";
    JsonArray ns = doc["notes"].as<JsonArray>();
    if (!ns || ns.size()==0){ request->send(400, "application/json", "{\"success\":false,\"message\":\"Note mancanti\"}"); return; }
    BellNote temp[MAX_MELODY_STEPS];
    uint8_t count = 0;
    for (JsonObject n : ns){
      int b = n["bellNumber"] | 1;
      int d = n["duration"] | 300;
      int dl = n["delay"] | 800;
      if (b<1 || b>2) continue;
      temp[count++] = { (uint8_t)b, (uint16_t)d, (uint16_t)dl };
      if (count>=MAX_MELODY_STEPS) break;
    }
    if (count==0){ request->send(400, "application/json", "{\"success\":false,\"message\":\"Nessuna nota valida\"}"); return; }
    bool ok = bellController.updateMelody((uint8_t)idx, name.c_str(), temp, count);
    if (!ok){ request->send(500, "application/json", "{\"success\":false,\"message\":\"Aggiornamento fallito\"}"); return; }
    saveAllMelodiesToFS();
    request->send(200, "application/json", "{\"success\":true}");
  });

  // API elimina melodia
  server.on("/api/delete-melody", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    if (index == 0) {
      request->_tempObject = new String();
      ((String*)request->_tempObject)->reserve(total);
    }
    String* body = (String*)request->_tempObject;
    body->concat((const char*)data, len);
    if (index + len < total) { return; }
  DynamicJsonDocument doc(256);
  DeserializationError parseErr = deserializeJson(doc, *body);
    delete body; request->_tempObject = nullptr;
  if (parseErr) { request->send(400, "application/json", "{\"success\":false}"); return; }
    int idx = doc["index"] | -1;
    if (idx < 0 || idx >= 10) { request->send(400, "application/json", "{\"success\":false}"); return; }
    bool ok = bellController.deleteMelody(idx);
    if (ok) saveAllMelodiesToFS();
    request->send(200, "application/json", String("{\"success\":") + (ok?"true":"false") + "}");
  });

  // API per fermare la melodia in riproduzione (POST e GET)
  server.on("/api/stop-melody", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/stop-melody (POST)");
    bellController.stopMelody();
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Melodia fermata\"}");
  });
  server.on("/api/stop-melody", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/stop-melody (GET)");
    bellController.stopMelody();
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Melodia fermata\"}");
  });
  
  // API per aggiornamento orario manuale
  server.on("/api/set-time", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
  [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    Serial.println("📡 Richiesta ricevuta: /api/set-time");
    if (index == 0) {
      request->_tempObject = new String();
      ((String*)request->_tempObject)->reserve(total);
    }
    String* body = (String*)request->_tempObject;
    body->concat((const char*)data, len);
    if (index + len < total) { return; }
    DynamicJsonDocument doc(512);
    deserializeJson(doc, *body);
    delete body; request->_tempObject = nullptr;
    String dateTime = doc["dateTime"];
    if (dateTime.length() > 0) {
      // Parsing formato ISO 8601: "2025-09-12T15:30:00"
      int year, month, day, hour, minute, second;
      if (sscanf(dateTime.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) == 6) {
        Serial.printf("[DEBUG] Imposto orario: %04d-%02d-%02d %02d:%02d:%02d\n", year, month, day, hour, minute, second);
        // Aggiorna RTC se presente, altrimenti imposta orario manuale
        if (systemStatus.rtcConnected) {
          rtc.adjust(DateTime(year, month, day, hour, minute, second));
          Serial.println("[DEBUG] RTC aggiornato");
        } else {
          manualTime = DateTime(year, month, day, hour, minute, second);
          manualTimeValid = true;
          Serial.println("[DEBUG] Orario manuale impostato (RTC assente)");
        }
        // Aggiorna variabili di sistema (simula sync NTP)
        systemStatus.ntpSynced = true;
        // Forza refresh display
        updateDisplay();
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Orario aggiornato\"}");
        Serial.print("✓ Richiesta aggiornamento orario: ");
        Serial.println(dateTime);
      } else {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Formato data/ora non valido\"}");
        Serial.println("❌ Formato data/ora non valido (parsing fallito)");
      }
    } else {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"Formato data/ora non valido\"}");
      Serial.println("❌ Formato data/ora non valido (campo mancante)");
    }
  });
  
  // API per configurazione WiFi
  server.on("/api/configure-wifi", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/configure-wifi");
  }, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    // Accumula body
    if (index == 0) {
      request->_tempObject = new String();
      ((String*)request->_tempObject)->reserve(total);
    }
    String* body = (String*)request->_tempObject;
    body->concat((const char*)data, len);
    if (index + len < total) { return; }
    String jsonString = *body;
    delete body; request->_tempObject = nullptr;
    Serial.print("JSON ricevuto: ");
    Serial.println(jsonString);
    
    // Parse semplice senza ArduinoJson per evitare problemi
    int ssidStart = jsonString.indexOf("\"ssid\":\"") + 8;
    int ssidEnd = jsonString.indexOf("\"", ssidStart);
    String newSSID = jsonString.substring(ssidStart, ssidEnd);
    
    int passwordStart = jsonString.indexOf("\"password\":\"") + 12;
    int passwordEnd = jsonString.indexOf("\"", passwordStart);
    String newPassword = jsonString.substring(passwordStart, passwordEnd);
    
    Serial.print("Nuovo SSID: ");
    Serial.println(newSSID);
    Serial.print("Nuova Password: [NASCOSTA] (");
    Serial.print(newPassword.length());
    Serial.println(" caratteri)");
    
    if (newSSID.length() > 0 && newPassword.length() > 0) {
      // Salva le credenziali su SPIFFS
      DynamicJsonDocument doc(512);
      doc["ssid"] = newSSID;
      doc["password"] = newPassword;
      
      fs::File file = SPIFFS.open("/wifi_config.json", "w");
      if (file) {
        serializeJson(doc, file);
        file.close();
        Serial.println("✓ Credenziali WiFi salvate");
      }
      
      request->send(200, "application/json", "{\"success\":true,\"message\":\"WiFi configurato, riavvio...\"}");
      Serial.println("✓ Configurazione WiFi accettata, riavvio tra 2 secondi...");
      
      // Programma riavvio
      delay(2000);
      ESP.restart();
    } else {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"SSID e password richiesti\"}");
      Serial.println("❌ SSID o password mancanti");
    }
  });

  // API per STOP di emergenza
  server.on("/api/emergency-stop", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/emergency-stop");
    bellController.emergencyStop();
    request->send(200, "application/json", "{\"success\":true,\"message\":\"STOP di emergenza attivato\"}");
  });

  // API per toggle modalità test
  server.on("/api/toggle-test-mode", HTTP_POST, [](AsyncWebServerRequest *request){
    testMode = !testMode;
    bellController.enableTestMode(testMode);
    String resp = String("{\"success\":true,\"testMode\":") + (testMode?"true":"false") + "}";
    request->send(200, "application/json", resp);
    Serial.printf("Modalità test -> %s\n", testMode?"ON":"OFF");
  });

  // API per test singolo relè: /api/test-relay?relay=1&duration=500
  server.on("/api/test-relay", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/test-relay");
    int relay = 1;
    int duration = 500;
    if (request->hasParam("relay")) {
      relay = request->getParam("relay")->value().toInt();
    }
    if (request->hasParam("duration")) {
      duration = request->getParam("duration")->value().toInt();
    }

    if (relay < 1 || relay > 2) {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"relay must be 1 or 2\"}");
      Serial.println("❌ test-relay: parametro relay non valido");
      return;
    }

    // Attiva temporaneamente modalità test per forzare il suono anche se disabilitate
    bellController.enableTestMode(true);
    bellController.ringBell((uint8_t)relay, (uint16_t)duration);
    bellController.enableTestMode(false);

    String resp = "{\"success\":true,\"relay\":" + String(relay) + ",\"duration\":" + String(duration) + "}";
    request->send(200, "application/json", resp);
    Serial.printf("✓ test-relay eseguito: relay=%d duration=%d\n", relay, duration);
  });
  
  // API per abilitare/disabilitare campane
  server.on("/api/toggle-bells", HTTP_GET, [](AsyncWebServerRequest *request){
    bool enabled = false;
    if (request->hasParam("enabled")) {
      String v = request->getParam("enabled")->value();
      enabled = (v == "1" || v == "true" || v == "on");
    } else {
      enabled = !systemStatus.bellsEnabled; // toggle if no param
    }
    Serial.printf("[GET] toggle-bells -> %d\n", enabled);
    bellController.setEnabled(enabled);
    systemStatus.bellsEnabled = enabled;
    bellsEnabled = enabled;
    updateDisplay();
    request->send(200, "application/json", String("{\"success\":true,\"enabled\":" ) + (enabled?"true":"false") + "}");
  });
  server.on("/api/toggle-bells", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    Serial.println("📡 Richiesta ricevuta: /api/toggle-bells (POST)");
    if (index == 0) {
      request->_tempObject = new String();
      ((String*)request->_tempObject)->reserve(total);
    }
    String* body = (String*)request->_tempObject;
    if (len > 0) body->concat((const char*)data, len);
    if (index + len < total) { return; }
    bool enabled = false;
    if (body->length() > 0) {
      DynamicJsonDocument doc(256);
      DeserializationError err = deserializeJson(doc, *body);
      if (!err && doc.containsKey("enabled")) {
        enabled = doc["enabled"];
      } else {
        Serial.println("⚠️ toggle-bells POST: body mancante/invalid, uso toggle");
        enabled = !systemStatus.bellsEnabled;
      }
    } else {
      enabled = !systemStatus.bellsEnabled;
    }
    delete body; request->_tempObject = nullptr;
    Serial.printf("Toggle bells -> %d\n", enabled ? 1 : 0);
    bellController.setEnabled(enabled);
    systemStatus.bellsEnabled = enabled;
    bellsEnabled = enabled;
    updateDisplay();
    request->send(200, "application/json", String("{\"success\":true,\"enabled\":") + (enabled?"true":"false") + "}");
  });

  // API diagnostica: lettura stato pin relè e LED
  server.on("/api/relay-status", HTTP_GET, [](AsyncWebServerRequest *request){
    static uint32_t lastMs = 0;
    uint32_t now = millis();
    if (now - lastMs < 150) { // troppo frequente: alleggeriamo
      request->send(429, "text/plain", "Too Many Requests");
      return;
    }
    lastMs = now;
    Serial.println("📡 Richiesta ricevuta: /api/relay-status");
    DynamicJsonDocument doc(256);
    doc["relay_active_level"] = "LOW (active when LOW)";
    doc["enabled"] = systemStatus.bellsEnabled;
    int r1 = -1, r2 = -1, led = -1;
    r1 = digitalRead(RELAY1_PIN);
    r2 = digitalRead(RELAY2_PIN);
    led = digitalRead(STATUS_LED_PIN);
    doc["relay1_raw"] = r1;
    doc["relay2_raw"] = r2;
    doc["statusLed_raw"] = led;
    String resp;
    serializeJson(doc, resp);
    request->send(200, "application/json", resp);
  });

  // API diagnostica: impostare direttamente il pin del relè (/api/set-relay?relay=1&value=0)
  server.on("/api/set-relay", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/set-relay");
    int relay = 1;
    int value = -1;
    if (request->hasParam("relay")) relay = request->getParam("relay")->value().toInt();
    if (request->hasParam("value")) value = request->getParam("value")->value().toInt();

    if (relay < 1 || relay > 2) {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"relay must be 1 or 2\"}");
      return;
    }
    if (value != 0 && value != 1) {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"value must be 0 or 1\"}");
      return;
    }

    int pin = (relay == 1) ? RELAY1_PIN : RELAY2_PIN;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value ? HIGH : LOW);
    String resp = "{\"success\":true,\"relay\":" + String(relay) + ",\"value\":" + String(value) + "}";
    request->send(200, "application/json", resp);
    Serial.printf("Set relay %d -> %d\n", relay, value);
  });
  // Serve i file statici da SPIFFS (no-cache per evitare UI vecchie)
  // Registrato alla fine per non ombreggiare le API.
  // Protezione Basic Auth per UI statica
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(ADMIN_USER, ADMIN_PASSWORD)) {
      return request->requestAuthentication();
    }
    request->send(SPIFFS, "/index.html", String(), false);
  });
  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!request->authenticate(ADMIN_USER, ADMIN_PASSWORD)) {
      return request->requestAuthentication();
    }
    request->send(SPIFFS, "/index.html", String(), false);
  });
  // Per le altre risorse statiche sotto / (css/js/png/etc.) proteggile con un handler generico
  server.onNotFound([](AsyncWebServerRequest *request){
    // Consenti sempre API pubbliche
    if (request->url().startsWith("/api/")) {
      request->send(404, "application/json", "{\"success\":false,\"message\":\"API non trovata\"}");
      return;
    }
    // Richiede autenticazione per le risorse statiche
    if (!request->authenticate(ADMIN_USER, ADMIN_PASSWORD)) {
      return request->requestAuthentication();
    }
    // Prova a servire dal FS, altrimenti 404
    if (SPIFFS.exists(request->url())) {
      request->send(SPIFFS, request->url(), String(), false);
    } else if (request->url() == "/") {
      request->send(SPIFFS, "/index.html", String(), false);
    } else {
      request->send(404, "text/plain", "Not Found");
    }
  });
  // Nota: l'handler onNotFound ora gestisce sia 404 API che le risorse statiche con auth

  Serial.println("✓ Web Server configurato");
}

void updateDisplay() {
  Serial.println("[DEBUG] Chiamata: updateDisplay()");
  
  // Aggiorna temperatura prima di mostrare il display
  systemStatus.esp32Temperature = getESP32Temperature();
  
  tft.fillScreen(TFT_BLACK);
  yield();

  // Prepara dimensioni display
  const int16_t W = tft.width();
  const int16_t H = tft.height();
  
  // === HEADER CENTRATO ===
  tft.setTextColor(TFT_CYAN);
  tft.setTextSize(1);
  tft.setTextDatum(TC_DATUM); // Top-Center
  tft.drawString("CAMPANE CHIESA", W/2, 0);
  yield();

  // === OROLOGIO GRANDE AL CENTRO ===
  char timeStr[20];
  char dateStr[20];
  bool timeValid = false;
  
  if (systemStatus.rtcConnected) {
     DateTime now = rtc.now();
     snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
     snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d", now.day(), now.month(), now.year());
     timeValid = true;
     Serial.printf("[DEBUG] updateDisplay() - Ora da RTC: %s, Data: %s\n", timeStr, dateStr);
   } else if (manualTimeValid) {
     snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", manualTime.hour(), manualTime.minute(), manualTime.second());
     snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d", manualTime.day(), manualTime.month(), manualTime.year());
     timeValid = true;
     Serial.printf("[DEBUG] updateDisplay() - Ora manuale: %s, Data: %s\n", timeStr, dateStr);
   } else if (systemStatus.ntpSynced) {
     time_t now = time(nullptr);
     struct tm * timeinfo = localtime(&now);
     snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
     snprintf(dateStr, sizeof(dateStr), "%02d/%02d/%04d", timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900);
     timeValid = true;
     Serial.printf("[DEBUG] updateDisplay() - Ora da NTP: %s, Data: %s\n", timeStr, dateStr);
  }
  
  // Mostra orologio grande
  if (timeValid) {
    // Ora grande centrata
    tft.setTextDatum(TC_DATUM); // Top-Center
    tft.setTextSize(3);
    tft.setTextColor(TFT_WHITE);
    tft.drawString(timeStr, W/2, 22);
    yield();

    // Data più piccola centrata
    tft.setTextSize(1);
    tft.setTextColor(TFT_CYAN);
    tft.drawString(dateStr, W/2, 54);
    yield();
  } else {
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.setTextColor(TFT_RED);
    tft.drawString("NO TIME", W/2, 30);
    yield();
  }
  
  // === INDICATORI DI STATO CON ICONE COLORATE ===
  // Ripristina allineamento a sinistra per le righe successive
  tft.setTextDatum(TL_DATUM); // Top-Left
  tft.setTextSize(1);
  int yPos = 75;
  
  // WiFi Status con icona
  tft.setCursor(0, yPos);
  if (systemStatus.wifiConnected) {
    tft.setTextColor(TFT_GREEN);
    tft.print("● WiFi ");
    tft.setTextColor(TFT_WHITE);
    tft.print(WiFi.localIP());
  } else {
    tft.setTextColor(TFT_YELLOW);
    tft.print("◐ WiFi AP ");
    tft.setTextColor(TFT_WHITE);
    tft.print(WiFi.softAPIP());
  }
  yPos += 12;
  yield();
  
  // RTC Status con icona
  tft.setCursor(0, yPos);
  if (systemStatus.rtcConnected) {
    tft.setTextColor(TFT_GREEN);
    tft.print("● RTC ");
    tft.setTextColor(TFT_WHITE);
    tft.print("Connesso");
  } else {
    tft.setTextColor(TFT_RED);
    tft.print("✗ RTC ");
    tft.setTextColor(TFT_WHITE);
    tft.print("Non trovato");
  }
  yPos += 12;
  yield();
  
  // Stato Campane con icona
  tft.setCursor(0, yPos);
  if (systemStatus.bellsEnabled) {
    tft.setTextColor(TFT_GREEN);
    tft.print("♪ Campane ");
    tft.setTextColor(TFT_GREEN);
    tft.print("ABILITATE");
  } else {
    tft.setTextColor(TFT_RED);
    tft.print("♪ Campane ");
    tft.setTextColor(TFT_RED);
    tft.print("DISABILITATE");
  }
  yPos += 12;
  yield();
  
  // Temperatura ESP32 con icona e colori
  tft.setCursor(0, yPos);
  if (systemStatus.thermalProtection) {
    tft.setTextColor(TFT_RED);
    tft.print("▲ ESP32 ");
    tft.print(systemStatus.esp32Temperature, 1);
    tft.print("°C [CRITICA!]");
  } else if (systemStatus.temperatureWarning) {
    tft.setTextColor(TFT_YELLOW);
    tft.print("△ ESP32 ");
    tft.print(systemStatus.esp32Temperature, 1);
    tft.print("°C [ALTA]");
  } else {
    tft.setTextColor(TFT_GREEN);
    tft.print("● ESP32 ");
    tft.print(systemStatus.esp32Temperature, 1);
    tft.print("°C [OK]");
  }
  yPos += 12;
  yield();
  
  // Fuso orario compatto
  tft.setCursor(0, yPos);
  tft.setTextColor(TFT_CYAN);
  tft.print("⏰ UTC+");
  tft.print(utcOffsetHours);
  tft.print(" ");
  tft.print(isDST ? "DST" : "STD");
  yPos += 12;
  yield();

  // Info temperatura aggiuntive se necessario
  if (systemStatus.thermalProtection || systemStatus.temperatureWarning) {
    tft.setCursor(0, yPos);
    tft.setTextColor(TFT_RED);
    if (systemStatus.thermalProtection) {
      tft.print("⚠ PROTEZIONE TERMICA ON");
    } else {
      tft.print("⚠ Monitoraggio attivo");
    }
  }
}


