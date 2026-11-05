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
#include "include/validators.h"
#include "include/wifi_manager.h"
#include "include/watchdog_manager.h"
#include "include/recovery_manager.h"  // v2.3.8: Sistema recovery
#include "include/telegram_bot.h"      // v2.3.9: Bot Telegram
#include "include/command_handler.h"   // v2.3.10: Unificazione Telegram + HTTP
#include "include/web_assets.h"        // Pagine web compresse (generato da tools/embed_web.py)

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

// === MANAGER INSTANCES (v2.3.5+) ===
WiFiReconnectManager wifiManager;
WatchdogManager wdManager;
RecoveryManager recoveryManager;  // v2.3.8: Sistema recovery multilivello
TelegramBotManager telegramBot;   // v2.3.9: Bot Telegram

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
bool testMode = false;
bool schedulerActive = false;

// Timing variables
unsigned long lastUpdate = 0;
unsigned long lastNTPCheck = 0;
unsigned long lastScheduleCheck = 0;
int lastCheckedMinute = -1;
unsigned long lastWiFiCheck = 0;
unsigned long lastNTPSync = 0;

// Cache per risposte API (ottimizzazione prestazioni)
String cachedStatusJson = "";
String cachedTimeJson = "";
String cachedRelayStatusJson = "";
unsigned long lastStatusCache = 0;
unsigned long lastTimeCache = 0;
unsigned long lastRelayStatusCache = 0;
const unsigned long CACHE_DURATION_MS = 1000; // 1 secondo TTL

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

// FS paths - now using Config::FileSystem constants
static const char* SCHEDULES_FS = Config::FileSystem::SCHEDULES_FILE;
static const char* MELODIES_FS = Config::FileSystem::MELODIES_FILE;
static const char* MAINTENANCE_FS = Config::FileSystem::MAINTENANCE_FILE;

static const uint8_t AUTO_MAINTENANCE_HOURS[] = {0, 4, 8, 12, 16, 20};
static constexpr uint8_t AUTO_MAINTENANCE_MINUTE = 10;
static constexpr unsigned long AUTO_MAINTENANCE_RESTART_DELAY_MS = 5000;
String lastAutoMaintenanceSlot = "";
bool autoMaintenanceRestartPending = false;
unsigned long autoMaintenanceRestartAt = 0;

// Struttura configurazione di rete
struct NetworkConfig {
  bool useDHCP;
  IPAddress staticIP;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns1;
  IPAddress dns2;
};

// Configurazione di rete corrente
NetworkConfig networkConfig;

// === DICHIARAZIONI DI FUNZIONE ===
void updateDisplay();
bool loadMelodiesFromFS();
bool saveAllMelodiesToFS();
void scanI2CDevices();
void initSNTP(bool waitForSync);
bool loadSchedulesFromFS();
bool saveSchedulesToFS();
void checkAndRunSchedules();
bool saveTelegramUsersToFS();
bool loadTelegramUsersFromFS();
bool saveAutoMaintenanceStateToFS();
bool loadAutoMaintenanceStateFromFS();
void checkAutoTelegramMaintenance();
String buildAutoMaintenanceReport(const struct tm &timeInfo);
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

// === FUNZIONI DIAGNOSTICA ===
void printWiFiDiagnostics();
void printNTPDiagnostics();

inline bool isButtonPressedLevel(int rawLevel, bool activeLow) {
  return activeLow ? (rawLevel == LOW) : (rawLevel == HIGH);
}

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
  
  // Ultimo fallback: nessuna fonte disponibile, non simulare un'ora fittizia
  // per evitare che le campane suonino su una programmazione errata.
  return false;
}

// === FUNZIONI CONFIGURAZIONE DI RETE ===

bool loadNetworkConfig() {
  // Imposta valori di default (DHCP)
  networkConfig.useDHCP = true;
  networkConfig.staticIP = IPAddress(192, 168, 1, 100);
  networkConfig.gateway = IPAddress(192, 168, 1, 1);
  networkConfig.subnet = IPAddress(255, 255, 255, 0);
  networkConfig.dns1 = IPAddress(8, 8, 8, 8);
  networkConfig.dns2 = IPAddress(8, 8, 4, 4);
  
  if (!SPIFFS.exists(Config::FileSystem::NETWORK_FILE)) {
    Serial.println("[NETWORK] File network.json non trovato, uso DHCP");
    return true; // Default DHCP, ok
  }
  
  fs::File file = SPIFFS.open(Config::FileSystem::NETWORK_FILE, "r");
  if (!file) {
    Serial.println("[NETWORK] Errore apertura network.json");
    return false;
  }
  
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    Serial.print("[NETWORK] Errore parsing JSON: ");
    Serial.println(error.c_str());
    return false;
  }
  
  // Carica configurazione
  networkConfig.useDHCP = doc["dhcp"] | true;
  
  if (!networkConfig.useDHCP) {
    String ip = doc["static_ip"] | "192.168.1.100";
    String gw = doc["gateway"] | "192.168.1.1";
    String sn = doc["subnet"] | "255.255.255.0";
    String d1 = doc["dns1"] | "8.8.8.8";
    String d2 = doc["dns2"] | "8.8.4.4";
    
    if (!networkConfig.staticIP.fromString(ip) ||
        !networkConfig.gateway.fromString(gw) ||
        !networkConfig.subnet.fromString(sn) ||
        !networkConfig.dns1.fromString(d1) ||
        !networkConfig.dns2.fromString(d2)) {
      Serial.println("[NETWORK] Errore parsing indirizzi IP, uso DHCP");
      networkConfig.useDHCP = true;
      return false;
    }
    
    Serial.println("[NETWORK] Configurazione IP statico caricata:");
    Serial.print("  IP: "); Serial.println(networkConfig.staticIP);
    Serial.print("  Gateway: "); Serial.println(networkConfig.gateway);
    Serial.print("  Subnet: "); Serial.println(networkConfig.subnet);
    Serial.print("  DNS1: "); Serial.println(networkConfig.dns1);
    Serial.print("  DNS2: "); Serial.println(networkConfig.dns2);
  } else {
    Serial.println("[NETWORK] Modalità DHCP attiva");
  }
  
  return true;
}

bool saveNetworkConfig() {
  DynamicJsonDocument doc(512);
  
  doc["dhcp"] = networkConfig.useDHCP;
  doc["static_ip"] = networkConfig.staticIP.toString();
  doc["gateway"] = networkConfig.gateway.toString();
  doc["subnet"] = networkConfig.subnet.toString();
  doc["dns1"] = networkConfig.dns1.toString();
  doc["dns2"] = networkConfig.dns2.toString();
  
  fs::File file = SPIFFS.open(Config::FileSystem::NETWORK_FILE, "w");
  if (!file) {
    Serial.println("[NETWORK] Errore scrittura network.json");
    return false;
  }
  
  bool success = serializeJson(doc, file) > 0;
  file.close();
  
  if (success) {
    Serial.println("[NETWORK] Configurazione salvata su SPIFFS");
  }
  
  return success;
}

bool saveSchedulesToFS() {
  DynamicJsonDocument doc(16384);

  JsonArray w = doc.createNestedArray("weekly");
  for (int i = 0; i < weeklyCount; i++) {
    JsonObject o = w.createNestedObject();
    o["id"]          = weeklyArr[i].id;
    o["name"]        = weeklyArr[i].name;
    o["dayOfWeek"]   = weeklyArr[i].dayOfWeek;
    o["hour"]        = weeklyArr[i].hour;
    o["minute"]      = weeklyArr[i].minute;
    o["melodyIndex"] = weeklyArr[i].melodyIndex;
    o["isActive"]    = weeklyArr[i].isActive;
  }

  JsonArray s = doc.createNestedArray("special");
  for (int i = 0; i < specialCount; i++) {
    JsonObject o = s.createNestedObject();
    o["id"]          = specialArr[i].id;
    o["name"]        = specialArr[i].name;
    o["type"]        = specialArr[i].type;
    o["year"]        = specialArr[i].year;
    o["month"]       = specialArr[i].month;
    o["day"]         = specialArr[i].day;
    o["hour"]        = specialArr[i].hour;
    o["minute"]      = specialArr[i].minute;
    o["melodyIndex"] = specialArr[i].melodyIndex;
    o["isActive"]    = specialArr[i].isActive;
    o["isRecurring"] = specialArr[i].isRecurring;
  }

  fs::File f = SPIFFS.open(SCHEDULES_FS, "w");
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

bool loadSchedulesFromFS() {
  weeklyCount = 0; specialCount = 0;
  if (!SPIFFS.exists(SCHEDULES_FS)) return true; // nothing to load
  fs::File f = SPIFFS.open(SCHEDULES_FS, "r");
  if (!f) return false;
  DynamicJsonDocument doc(16384);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  
  // Track next available ID for auto-generation
  uint8_t nextWeeklyId = 1;
  uint8_t nextSpecialId = 1;
  bool needsSave = false;
  
  if (doc.containsKey("weekly")){
    JsonArray w = doc["weekly"].as<JsonArray>();
    for (JsonObject o : w){
      if (weeklyCount>=MAX_WEEKLY_SCHEDULES) break;
      WeeklySchedule &it = weeklyArr[weeklyCount++];
      strlcpy(it.name, (o["name"] | ""), sizeof(it.name));
      
      // Generate ID if missing
      if (o.containsKey("id")) {
        it.id = o["id"];
        if (it.id >= nextWeeklyId) nextWeeklyId = it.id + 1;
      } else {
        it.id = nextWeeklyId++;
        needsSave = true;
      }
      
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
      
      // Generate ID if missing
      if (o.containsKey("id")) {
        it.id = o["id"];
        if (it.id >= nextSpecialId) nextSpecialId = it.id + 1;
      } else {
        it.id = nextSpecialId++;
        needsSave = true;
      }
      
      it.type = (EventType)(int)(o["type"] | 5);
      it.year = o["year"] | 0; it.month = o["month"] | 0; it.day = o["day"] | 0;
      it.hour = o["hour"] | 0; it.minute = o["minute"] | 0;
      it.melodyIndex = o["melodyIndex"] | 0; it.isActive = o["isActive"] | true; it.isRecurring = o["isRecurring"] | false;
    }
  }
  
  // Save back if we generated any IDs
  if (needsSave) {
    saveSchedulesToFS();
  }
  
  return true;
}

// === PERSISTENZA UTENTI TELEGRAM ===
static const char* TELEGRAM_USERS_FS = Config::FileSystem::TELEGRAM_USERS_FILE;

bool saveTelegramUsersToFS() {
  String usersList = telegramBot.getAuthorizedUsers();
  StaticJsonDocument<512> doc;
  JsonArray arr = doc.createNestedArray("users");
  if (usersList.length() > 0) {
    int startIdx = 0;
    int commaIdx = usersList.indexOf(',', startIdx);
    while (commaIdx > 0) {
      String uid = usersList.substring(startIdx, commaIdx);
      uid.trim();
      if (uid.length() > 0) arr.add(uid);
      startIdx = commaIdx + 1;
      commaIdx = usersList.indexOf(',', startIdx);
    }
    String uid = usersList.substring(startIdx);
    uid.trim();
    if (uid.length() > 0) arr.add(uid);
  }
  fs::File f = SPIFFS.open(TELEGRAM_USERS_FS, "w");
  if (!f) return false;
  bool ok = serializeJson(doc, f) > 0;
  f.close();
  Serial.printf("[Telegram] Utenti salvati su FS (%d)\n", (int)arr.size());
  return ok;
}

bool loadTelegramUsersFromFS() {
  if (!SPIFFS.exists(TELEGRAM_USERS_FS)) return true;
  fs::File f = SPIFFS.open(TELEGRAM_USERS_FS, "r");
  if (!f) return false;
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  if (doc.containsKey("users")) {
    JsonArray arr = doc["users"].as<JsonArray>();
    for (JsonVariant v : arr) {
      String uid = v.as<String>();
      uid.trim();
      if (uid.length() > 0) telegramBot.addAuthorizedUser(uid);
    }
    Serial.printf("[Telegram] Utenti caricati da FS (%d)\n", (int)arr.size());
  }
  return true;
}

bool saveAutoMaintenanceStateToFS() {
  StaticJsonDocument<128> doc;
  doc["lastSlot"] = lastAutoMaintenanceSlot;

  fs::File file = SPIFFS.open(MAINTENANCE_FS, "w");
  if (!file) {
    Serial.println("[Maintenance] Errore scrittura maintenance.json");
    return false;
  }

  bool ok = serializeJson(doc, file) > 0;
  file.close();
  return ok;
}

bool loadAutoMaintenanceStateFromFS() {
  if (!SPIFFS.exists(MAINTENANCE_FS)) {
    return true;
  }

  fs::File file = SPIFFS.open(MAINTENANCE_FS, "r");
  if (!file) {
    Serial.println("[Maintenance] Errore apertura maintenance.json");
    return false;
  }

  StaticJsonDocument<128> doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();
  if (err) {
    Serial.println("[Maintenance] JSON maintenance non valido");
    return false;
  }

  lastAutoMaintenanceSlot = doc["lastSlot"] | "";
  return true;
}

String buildAutoMaintenanceReport(const struct tm &timeInfo) {
  char timestamp[24];
  snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
           timeInfo.tm_year + 1900,
           timeInfo.tm_mon + 1,
           timeInfo.tm_mday,
           timeInfo.tm_hour,
           timeInfo.tm_min,
           timeInfo.tm_sec);

  String report = "[Auto Maintenance]\n";
  report += "Slot: ";
  report += timestamp;
  report += "\n";
  report += "WiFi: ";
  report += (wifiManager.isConnected() ? "OK" : "DOWN");
  report += "\n";
  if (wifiManager.isConnected()) {
    report += "IP: ";
    report += WiFi.localIP().toString();
    report += "\n";
  }
  report += "Bells: ";
  report += systemStatus.bellsEnabled ? "ENABLED" : "DISABLED";
  report += "\n";
  report += "NTP: ";
  report += systemStatus.ntpSynced ? "SYNCED" : "NOT_SYNCED";
  report += "\n";
  report += "Temp: ";
  report += String(currentESP32Temperature, 1);
  report += "C\n";
  report += "Uptime h: ";
  report += String(millis() / 3600000.0, 2);
  report += "\n";
  report += "Telegram msgs/cmds: ";
  report += String(telegramBot.getMessageCount());
  report += "/";
  report += String(telegramBot.getCommandCount());

  String recentLogs = Logger::getRecentLogs(6);
  if (recentLogs.length() > 0) {
    report += "\n\nRecent logs:\n";
    report += recentLogs;
  }

  report += "\n\nRestarting in 5s.";
  return report;
}

void checkAutoTelegramMaintenance() {
  if (!telegramBot.isEnabled() || autoMaintenanceRestartPending) {
    return;
  }

  struct tm timeInfo;
  if (!getLocalTm(timeInfo)) {
    return;
  }

  bool matchingHour = false;
  for (uint8_t hour : AUTO_MAINTENANCE_HOURS) {
    if (timeInfo.tm_hour == hour) {
      matchingHour = true;
      break;
    }
  }

  if (!matchingHour || timeInfo.tm_min != AUTO_MAINTENANCE_MINUTE) {
    return;
  }

  char slotKey[18];
  snprintf(slotKey, sizeof(slotKey), "%04d%02d%02d%02d%02d",
           timeInfo.tm_year + 1900,
           timeInfo.tm_mon + 1,
           timeInfo.tm_mday,
           timeInfo.tm_hour,
           timeInfo.tm_min);

  if (lastAutoMaintenanceSlot == slotKey) {
    return;
  }

  lastAutoMaintenanceSlot = slotKey;
  saveAutoMaintenanceStateToFS();

  String report = buildAutoMaintenanceReport(timeInfo);
  Serial.printf("[Maintenance] Slot automatico %s: invio report e riavvio pianificato\n", slotKey);
  telegramBot.sendNotification(report);

  autoMaintenanceRestartPending = true;
  autoMaintenanceRestartAt = millis() + AUTO_MAINTENANCE_RESTART_DELAY_MS;
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
  // Relè spenti subito: fino a qui i pin sono flottanti
  digitalWrite(RELAY1_PIN, RELAY_OFF);
  digitalWrite(RELAY2_PIN, RELAY_OFF);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  digitalWrite(MASTER_RELAY_PIN, Hardware::MASTER_OFF);
  pinMode(MASTER_RELAY_PIN, Hardware::MASTER_PIN_MODE);

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

  // === INIZIALIZZA MANAGER (v2.3.5+) ===
  Serial.printf("[Setup] Firmware versione: %s\n", Config::FIRMWARE_VERSION);
  Serial.println("[Setup] Inizializzazione manager...");
  
  // WiFi Manager - configurato dopo connessione WiFi
  wifiManager.setCredentials(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("[Setup] ✓ WiFiReconnectManager configurato");
  
  // Watchdog Manager - già attivo (auto-feed)
  Serial.println("[Setup] ✓ WatchdogManager attivo");
  
  // Recovery Manager - v2.3.8
  recoveryManager.begin();
  Serial.println("[Setup] ✓ RecoveryManager inizializzato (BOOT=10s factory reset)");

  // Pulsante fisico per avvio "FUNERALE" (GPIO27 - pulsante esterno)
  pinMode(FUNERAL_BUTTON_PIN, INPUT_PULLUP); // Usa pullup interno

  // Pulsante fisico per avvio "CHIAMATA MESSA" (GPIO32 - pulsante esterno)
  pinMode(MASS_BUTTON_PIN, INPUT_PULLUP); // Usa pullup interno
  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP); // Pulsante riavvio manuale (GPIO13)

  // Pulsante di configurazione (GPIO0 - boot button)
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);

  Serial.printf("[Buttons] Config pin=%d activeLow=%s\n", CONFIG_BUTTON_PIN, Hardware::CONFIG_BUTTON_ACTIVE_LOW ? "true" : "false");
  Serial.printf("[Buttons] Funeral pin=%d activeLow=%s\n", FUNERAL_BUTTON_PIN, Hardware::FUNERAL_BUTTON_ACTIVE_LOW ? "true" : "false");
  Serial.printf("[Buttons] Mass pin=%d activeLow=%s\n", MASS_BUTTON_PIN, Hardware::MASS_BUTTON_ACTIVE_LOW ? "true" : "false");
  Serial.printf("[Buttons] Reset pin=%d activeLow=%s\n", RESET_BUTTON_PIN, Hardware::RESET_BUTTON_ACTIVE_LOW ? "true" : "false");

  // Carica melodie, schedules e utenti Telegram da FS
  loadMelodiesFromFS();
  loadSchedulesFromFS();
  loadTelegramUsersFromFS();
  loadAutoMaintenanceStateFromFS();

  // Assicura che le due melodie predefinite esistano
  if (bellController.getMelodyNoteCount(0) == 0) {
    bellController.loadDefaultMelodies();
  }
  if (bellController.getMelodyNoteCount(1) == 0) {
    bellController.loadDefaultMelodies();
  }
  if (bellController.migrateLegacyMassCall()) {
    saveAllMelodiesToFS();
  }

  // WiFi e SNTP
  connectWiFi();

  // Web server
  setupWebServer();
  server.begin();
  Serial.println("Web server avviato");

  // === COMMAND HANDLER SETUP (v2.3.10) - Unificazione Telegram + HTTP ===
  CommandHandler& cmdHandler = CommandHandler::getInstance();
  cmdHandler.init(&bellController, &systemStatus);
  Serial.println("[CommandHandler] ✓ Inizializzato (unificazione API)");

  // === TELEGRAM BOT SETUP (v2.3.9) ===
  if (strlen(TELEGRAM_BOT_TOKEN) > 0 && strcmp(TELEGRAM_BOT_TOKEN, "123456789:ABCdefGHIjklMNOpqrsTUVwxyz") != 0) {
    Serial.println("[Telegram] Inizializzazione bot...");
    if (telegramBot.begin(TELEGRAM_BOT_TOKEN)) {
      Serial.println("[Telegram] ✓ Bot inizializzato con successo");
      
      // Collega bot al bell controller e system status
      telegramBot.setBellController(&bellController);
      telegramBot.setSystemStatus(&systemStatus);
      
      // Carica utenti autorizzati dalla configurazione
      for (uint8_t i = 0; i < TELEGRAM_AUTHORIZED_COUNT; i++) {
        telegramBot.addAuthorizedUser(TELEGRAM_AUTHORIZED_USERS[i]);
        Serial.printf("[Telegram] ✓ Utente autorizzato: %s\n", TELEGRAM_AUTHORIZED_USERS[i]);
      }
      
      // Abilita bot se configurato
      telegramBot.setEnabled(TELEGRAM_BOT_ENABLED);
      
      // La notifica di avvio la invia il task del bot dopo 20 s di funzionamento stabile
    } else {
      Serial.println("[Telegram] ⚠️ Errore inizializzazione bot");
    }
  } else {
    Serial.println("[Telegram] ⚠️ Token non configurato - bot disabilitato");
    Serial.println("[Telegram] Configura TELEGRAM_BOT_TOKEN in config.cpp");
  }

  // Prima schermata
  updateDisplay();
}

void loop() {
  // === WATCHDOG FEED (v2.3.5) - Auto-feed ogni 100ms ===
  wdManager.feedIfNeeded();
  
  // === RECOVERY SYSTEM (v2.3.8) - Factory reset check ===
  recoveryManager.checkFactoryReset();
  
  // Aggiorna controller campane (non bloccante)
  bellController.update();

  // === WIFI KEEPALIVE (v2.3.5+) - WiFiReconnectManager + RecoveryManager ===
  if (millis() - lastWiFiCheck >= Config::Network::WIFI_CHECK_INTERVAL_MS) {
    lastWiFiCheck = millis();
    
    // Skip check se in AP recovery mode
    if (recoveryManager.isAPModeActive()) {
      return; // Non fare nulla se in modalità recovery
    }
    
    if (!apMode && !wifiManager.isConnected()) {
      systemStatus.wifiConnected = false;

      // tryReconnect() non-bloccante: avvia la connessione e ritorna immediatamente.
      // Il risultato positivo si vede al prossimo ciclo via isConnected().
      if (wifiManager.tryReconnect()) {
        Serial.println("[WiFi] ✓ Riconnessione riuscita!");
        Serial.printf("[WiFi] ✓ IP: %s\n", WiFi.localIP().toString().c_str());
        systemStatus.wifiConnected = true;
        recoveryManager.resetWiFiFailCount();
      } else if (wifiManager.hadLastAttemptFailed()) {
        // Solo timeout definitivo conta come fallimento per il recovery manager
        recoveryManager.reportWiFiFailure();
      }
    } else if (wifiManager.isConnected() && !systemStatus.wifiConnected) {
      // Rilevata riconnessione
      Serial.println("[WiFi] ✓ Riconnessione rilevata!");
      Serial.printf("[WiFi] ✓ IP: %s\n", WiFi.localIP().toString().c_str());
      systemStatus.wifiConnected = true;
      
      // v2.3.8: Reset contatore fallimenti recovery
      recoveryManager.resetWiFiFailCount();
    }
  }

  // === FIX 2: NTP RE-SYNC - Risincronizza NTP ogni 1 ora ===
  if (millis() - lastNTPSync >= 3600000) { // Ogni 1 ora (3600000 ms)
    lastNTPSync = millis();
    if (systemStatus.wifiConnected) {
      Serial.println("[NTP] 🔄 Re-sync orario periodico...");
      initSNTP(false); // Sync senza attesa bloccante
      
      // Verifica se sync è andato a buon fine
      time_t now = time(nullptr);
      if (now > 1000000) {
        Serial.println("[NTP] ✓ Re-sync completato");
        systemStatus.ntpSynced = true;
        
        // Aggiorna anche RTC se presente
        if (systemStatus.rtcConnected) {
          struct tm timeinfo;
          if (getLocalTime(&timeinfo, 1000)) {
            DateTime ntpTime(
              timeinfo.tm_year + 1900,
              timeinfo.tm_mon + 1,
              timeinfo.tm_mday,
              timeinfo.tm_hour,
              timeinfo.tm_min,
              timeinfo.tm_sec
            );
            rtc.adjust(ntpTime);
            Serial.println("[NTP] ✓ RTC aggiornato da NTP");
          }
        }
      } else {
        Serial.println("[NTP] ⚠️ Re-sync fallito, riproverò tra 1 ora");
      }
    }
  }

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

  // Auto report Telegram + riavvio sugli slot richiesti
  checkAutoTelegramMaintenance();

  // Il bot Telegram gira in un task dedicato (vedi TelegramBotManager): niente da fare qui

  if (autoMaintenanceRestartPending && millis() >= autoMaintenanceRestartAt) {
    Serial.println("[Maintenance] Riavvio automatico in esecuzione");
    telegramBot.waitForOutbox(10000);
    ESP.restart();
  }

  // Comandi seriale
  processSerialCommands();

  // === PULSANTI FUNERALE (GPIO27) e CHIAMATA MESSA (GPIO32) ===
  // Filtro anti-disturbo: conta solo una pressione stabile per almeno BTN_DEBOUNCE_MS.
  // Click breve (rilasciato entro BTN_HOLD_MS): un colpo singolo.
  // Tenuto premuto BTN_HOLD_MS: avvia la melodia.
  {
    struct BellButton {
      uint8_t pin;
      bool activeLow;
      uint8_t melodyIndex;
      uint8_t clickBell;
      const char *name;
      bool stable;          // stato filtrato: true = premuto
      bool lastRaw;
      uint32_t rawSince;    // da quando la lettura grezza è invariata
      uint32_t pressedAt;
      bool holdFired;
    };
    static BellButton buttons[] = {
      {FUNERAL_BUTTON_PIN, Hardware::FUNERAL_BUTTON_ACTIVE_LOW, 0, 2, "FUNERALE", false, false, 0, 0, false},
      {MASS_BUTTON_PIN,    Hardware::MASS_BUTTON_ACTIVE_LOW,    1, 1, "CHIAMATA MESSA", false, false, 0, 0, false},
    };
    static constexpr uint32_t BTN_DEBOUNCE_MS = 80;
    static constexpr uint32_t BTN_HOLD_MS = 1000;
    static constexpr uint16_t BTN_CLICK_PULSE_MS = 200;

    uint32_t now = millis();
    static bool buttonsInit = false;
    if (!buttonsInit) {
      // Un tasto già premuto (o in corto) all'avvio non fa nulla finché non viene rilasciato
      buttonsInit = true;
      for (BellButton &btn : buttons) {
        btn.lastRaw = btn.stable = isButtonPressedLevel(digitalRead(btn.pin), btn.activeLow);
        btn.rawSince = btn.pressedAt = now;
        btn.holdFired = true;
      }
    }
    for (BellButton &btn : buttons) {
      bool raw = isButtonPressedLevel(digitalRead(btn.pin), btn.activeLow);
      if (raw != btn.lastRaw) {
        btn.lastRaw = raw;
        btn.rawSince = now;
      }
      // Cambia stato solo se la lettura resta uguale per tutto il tempo di debounce
      if (raw != btn.stable && now - btn.rawSince >= BTN_DEBOUNCE_MS) {
        btn.stable = raw;
        if (raw) {
          btn.pressedAt = now;
          btn.holdFired = false;
          Serial.printf("[BUTTON] %s (GPIO%d) premuto\n", btn.name, btn.pin);
        } else {
          uint32_t held = now - btn.pressedAt;
          Serial.printf("[BUTTON] %s (GPIO%d) rilasciato dopo %lu ms\n", btn.name, btn.pin, (unsigned long)held);
          if (!btn.holdFired && !bellController.isPlayingMelody()) {
            Serial.printf("%s: click -> colpo campana %d\n", btn.name, btn.clickBell);
            // Modalità test per suonare anche con campane disabilitate (come la melodia)
            bellController.enableTestMode(true);
            bellController.ringBell(btn.clickBell, BTN_CLICK_PULSE_MS);
            bellController.enableTestMode(false);
          }
        }
      }
      // Pressione lunga: avvia la melodia mentre il tasto è ancora premuto
      if (btn.stable && !btn.holdFired && now - btn.pressedAt >= BTN_HOLD_MS) {
        btn.holdFired = true;
        if (!bellController.isPlayingMelody()) {
          Serial.printf("%s: tenuto premuto -> avvio melodia\n", btn.name);
          if (bellController.getMelodyNoteCount(btn.melodyIndex) == 0) {
            bellController.loadDefaultMelodies();
          }
          bellController.enableTestMode(true);
          bellController.playMelody(btn.melodyIndex);
        }
      }
    }
  }

  // === PULSANTE STOP / RIAVVIO (GPIO13) ===
  // Pressione (>=50ms): ferma subito melodia e relè, con notifica Telegram.
  // Tenuto 3 secondi: riavvia. Invia notifica Telegram prima del restart.
  {
    static bool lastResetState = true;   // pull-up: true = rilasciato
    static uint32_t resetPressStart = 0;
    static bool resetHandled = false;
    static bool stopHandled = false;
    static constexpr uint32_t STOP_DEBOUNCE_MS = 50;
    static constexpr uint32_t RESET_HOLD_MS = 3000;

    bool resetState = digitalRead(RESET_BUTTON_PIN);
    bool resetPressed = isButtonPressedLevel(resetState, Hardware::RESET_BUTTON_ACTIVE_LOW);

    if (resetPressed && !resetHandled) {
      if (lastResetState) {                      // fronte di discesa
        resetPressStart = millis();
        stopHandled = false;
      }
      if (!stopHandled && millis() - resetPressStart >= STOP_DEBOUNCE_MS) {
        stopHandled = true;
        bool wasPlaying = bellController.isPlayingMelody();
        String name = wasPlaying ? String(bellController.getMelodyName(bellController.getCurrentMelodyIndex())) : String();
        bellController.stopMelody();
        bellController.emergencyStop();          // stacca anche un colpo singolo in corso
        Serial.println("[STOP] Pulsante GPIO13: arresto immediato");
        if (wasPlaying && telegramBot.isEnabled()) {
          telegramBot.sendNotification("⏹ Melodia \"" + name + "\" fermata con il pulsante.");
        }
      }
      if (millis() - resetPressStart >= RESET_HOLD_MS) {
        Serial.println("[RESET] Pulsante GPIO13 tenuto 3s: riavvio");
        if (telegramBot.isEnabled()) {
          telegramBot.sendNotification("🔄 Riavvio manuale richiesto via pulsante GPIO13.");
          telegramBot.waitForOutbox(8000);
        }
        ESP.restart();
        resetHandled = true;
      }
    } else if (!resetPressed) {
      resetHandled = false;
    }

    lastResetState = !resetPressed;
  }

  // Gestione pulsante CONFIG (GPIO0):
  // - breve pressione (<2s): toggle campane abilitate
  // - lunga pressione (>=2s): attiva AP di configurazione
  {
  static bool lastCfgState = false; // GPIO0 boot button
    static uint32_t pressStart = 0;
    static bool longHandled = false;
  bool pressed = isButtonPressedLevel(digitalRead(CONFIG_BUTTON_PIN), Hardware::CONFIG_BUTTON_ACTIVE_LOW);
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
        Serial.println("wifi_diag               - Diagnostica WiFi dettagliata");
        Serial.println("ntp_diag                - Diagnostica NTP/orario dettagliata");
        Serial.println("full_diag               - Diagnostica completa sistema");
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
        bellController.setEnabled(true);
        Serial.println("🔔 CAMPANE ABILITATE manualmente");
        
    } else if (command == "disable_bells") {
        systemStatus.bellsEnabled = false;
        bellController.setEnabled(false);
        Serial.println("🔇 CAMPANE DISABILITATE manualmente");
        Serial.println("⚠️ ATTENZIONE: Le campane rimarranno disabilitate fino al prossimo riavvio");
        Serial.println("💡 Per riabilitare: 'enable_bells' o riavvio sistema");
        
    } else if (command == "i2c" || command == "scan") {
        Serial.println("=== SCANSIONE DISPOSITIVI I2C ===");
        scanI2CDevices();
        
    } else if (command == "wifi_diag") {
        printWiFiDiagnostics();
        
    } else if (command == "ntp_diag") {
        printNTPDiagnostics();
        
    } else if (command == "full_diag") {
        Serial.println("\n╔═══════════════════════════════════════╗");
        Serial.println("║   DIAGNOSTICA COMPLETA SISTEMA       ║");
        Serial.println("╚═══════════════════════════════════════╝\n");
        
        // Status generale
        Serial.println("\n=== 📊 STATO GENERALE ===");
        Serial.printf("Firmware: %s\n", Config::FIRMWARE_VERSION);
        Serial.printf("Uptime: %lu ms (%.2f ore)\n", millis(), millis() / 3600000.0);
        Serial.printf("Free Heap: %u bytes\n", ESP.getFreeHeap());
        Serial.printf("Heap Size: %u bytes\n", ESP.getHeapSize());
        Serial.printf("Chip Model: %s\n", ESP.getChipModel());
        Serial.printf("CPU Freq: %u MHz\n", ESP.getCpuFreqMHz());
        Serial.printf("Flash Size: %u bytes\n", ESP.getFlashChipSize());
        
        // WiFi
        printWiFiDiagnostics();
        
        // NTP
        printNTPDiagnostics();
        
        // Temperatura
        Serial.println("\n=== 🌡️ TEMPERATURA ===");
        Serial.printf("ESP32: %.1f°C [%s]\n", systemStatus.esp32Temperature, getTemperatureStatus().c_str());
        Serial.printf("Protezione termica: %s\n", systemStatus.thermalProtection ? "ATTIVA" : "Non attiva");
        Serial.printf("Warning temperatura: %s\n", systemStatus.temperatureWarning ? "SÌ" : "No");
        
        // Campane
        Serial.println("\n=== 🔔 STATO CAMPANE ===");
        Serial.printf("Campane abilitate: %s\n", systemStatus.bellsEnabled ? "SÌ" : "NO");
        Serial.printf("Test mode: %s\n", testMode ? "ATTIVO" : "Non attivo");
        Serial.printf("Melodia in play: %s\n", bellController.isPlayingMelody() ? "SÌ" : "No");
        Serial.printf("Totale suonate: %d\n", systemStatus.totalBellRings);
        Serial.printf("Ultima suonata: %lu ms fa\n", millis() - systemStatus.lastBellTime);
        
        // Programmazioni
        Serial.println("\n=== 📅 PROGRAMMAZIONI ===");
        Serial.printf("Settimanali attive: %d/%d\n", weeklyCount, MAX_WEEKLY_SCHEDULES);
        Serial.printf("Eventi speciali: %d/%d\n", specialCount, MAX_SPECIAL_EVENTS);
        
        // Melodie
        Serial.println("\n=== 🎵 MELODIE ===");
        int melodyCount = 0;
        for (int i = 0; i < 10; i++) {
            if (bellController.getMelodyNoteCount(i) > 0) melodyCount++;
        }
        Serial.printf("Melodie caricate: %d/10\n", melodyCount);
        
        Serial.println("\n╔═══════════════════════════════════════╗");
        Serial.println("║   FINE DIAGNOSTICA                   ║");
        Serial.println("╚═══════════════════════════════════════╝\n");
        
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
  
  // Cooldown: evita nuove esecuzioni se una melodia è appena terminata (<10s fa)
  static uint32_t lastMelodyEndTime = 0;
  if (systemStatus.lastBellTime > 0 && (millis() - systemStatus.lastBellTime < 10000)) {
    // C'è una melodia in corso o appena terminata
    if (bellController.isPlayingMelody()) {
      return; // Melodia ancora in esecuzione
    }
    // Se la melodia è terminata da meno di 10s, aggiorna il timestamp
    if (millis() - lastMelodyEndTime < 10000) {
      return; // Cooldown attivo
    }
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
      
      // Aggiorna statistiche e cooldown
      systemStatus.totalBellRings++;
      systemStatus.lastBellTime = millis();
      systemStatus.activeMelody = e.melodyIndex;
      lastMelodyEndTime = millis(); // Imposta cooldown
      
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
      lastMelodyEndTime = millis(); // Imposta cooldown
      
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
  
  // Carica configurazione di rete (IP statico o DHCP)
  loadNetworkConfig();
  
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
        
        // === v2.3.8: Sistema recovery con DHCP fallback ===
        bool staticIPSuccess = false;
        bool dhcpFallbackUsed = false;
        
        // TENTATIVO 1: IP Statico (se configurato)
        if (!networkConfig.useDHCP && !wifiManager.hasStaticIPFailed()) {
          Serial.println("[NETWORK] 🔧 Tentativo connessione con IP statico...");
          if (!WiFi.config(networkConfig.staticIP, networkConfig.gateway, 
                           networkConfig.subnet, networkConfig.dns1, networkConfig.dns2)) {
            Serial.println("[NETWORK] ✗ Errore WiFi.config() - Fallback a DHCP");
            wifiManager.reportStaticIPFailure();
          } else {
            Serial.println("[NETWORK] ✓ WiFi.config() applicato, tentativo connessione...");
            WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
            
            // Attendi connessione (30s timeout per IP statico)
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 30) {
              for (int i = 0; i < 10; i++) { // 10 x 100ms = 1s
                delay(100);
                yield();
                delay(0); // Feed asynch stack
              }
              Serial.print(".");
              attempts++;
            }
            
            if (WiFi.status() == WL_CONNECTED) {
              staticIPSuccess = true;
              Serial.println("\n[NETWORK] ✓ Connessione con IP statico riuscita!");
            } else {
              Serial.println("\n[NETWORK] ✗ Timeout IP statico (30s) - Fallback a DHCP");
              wifiManager.reportStaticIPFailure();
              WiFi.disconnect();
              delay(500);
            }
          }
        }
        
        // TENTATIVO 2: DHCP Fallback (se IP statico fallito O se DHCP configurato)
        if (!staticIPSuccess) {
          if (wifiManager.hasStaticIPFailed()) {
            Serial.println("[NETWORK] 🔄 FALLBACK AUTOMATICO A DHCP (IP statico fallito)");
            dhcpFallbackUsed = true;
          } else {
            Serial.println("[NETWORK] 🔧 Connessione con DHCP configurato");
          }
          
          // Reset config e riconnetti con DHCP
          WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
          WiFi.begin(savedSSID.c_str(), savedPassword.c_str());
          
          int attempts = 0;
          while (WiFi.status() != WL_CONNECTED && attempts < 20) { // 20s per DHCP
            for (int i = 0; i < 10; i++) {
              delay(100);
              yield();
              delay(0);
            }
            Serial.print(".");
            attempts++;
          }
        }
        
        // === VALUTAZIONE RISULTATO ===
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("");
          Serial.println("✓ CONNESSIONE WiFi RIUSCITA!");
          Serial.print("✓ Indirizzo IP: ");
          Serial.println(WiFi.localIP());
          Serial.print("✓ Gateway: ");
          Serial.println(WiFi.gatewayIP());
          Serial.print("✓ Subnet: ");
          Serial.println(WiFi.subnetMask());
          Serial.print("✓ DNS: ");
          Serial.println(WiFi.dnsIP());
          Serial.print("✓ Modalità: ");
          if (dhcpFallbackUsed) {
            Serial.println("DHCP (fallback automatico da IP statico)");
          } else {
            Serial.println(networkConfig.useDHCP ? "DHCP" : "IP Statico");
          }
          
          systemStatus.wifiConnected = true;
          apMode = false;
          
          // Reset contatori recovery
          recoveryManager.resetWiFiFailCount();
          
          // Riduci ulteriormente la potenza una volta connesso
          WiFi.setTxPower(WIFI_POWER_8_5dBm);
          Serial.println("📡 Potenza WiFi ridotta a 8.5dBm per prevenire surriscaldamento");
          
          // Inizializza SNTP con fuso orario Italia e attendi sync
          initSNTP(true);
          
          Serial.println("=== FINE CONNESSIONE WiFi ===");
          return;
        } else {
          // v2.3.8: Notifica recovery manager
          Serial.println("\n[NETWORK] ✗ Connessione fallita anche con DHCP!");
          recoveryManager.reportWiFiFailure();
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
    // Rate limiting (150ms minimo tra richieste)
    static uint32_t lastMs = 0;
    uint32_t now = millis();
    if (now - lastMs < 150) {
      request->send(429, "text/plain", "Too Many Requests");
      return;
    }
    lastMs = now;
    
    // Cache: se risposta già generata negli ultimi 1s, riutilizzala
    if (now - lastStatusCache < CACHE_DURATION_MS && cachedStatusJson.length() > 0) {
      AsyncWebServerResponse* r = request->beginResponse(200, "application/json", cachedStatusJson);
      r->addHeader("Connection", "close");
      r->addHeader("X-Cache", "HIT"); // Debug header
      request->send(r);
      return;
    }
    
    Serial.println("📡 Richiesta ricevuta: /api/status (cache miss)");

    CommandHandler& handler = CommandHandler::getInstance();
    CommandHandler::CommandResult result = handler.getStatus();
    if (!result.success || !result.jsonData) {
      request->send(500, "application/json", String("{\"success\":false,\"message\":\"") + result.message + "\"}");
      return;
    }

    JsonDocument& doc = *result.jsonData;
    doc["temperatureStatus"] = getTemperatureStatus();
    doc["timezoneDescription"] = String("Italia UTC+") + String(utcOffsetHours) + (isDST ? " (Ora Legale)" : " (Ora Solare)");
    doc["isPlaying"] = bellController.isPlayingMelody();
    if (bellController.isPlayingMelody()) {
      doc["playingName"] = bellController.getMelodyName(bellController.getCurrentMelodyIndex());
    }

    // Uptime e boot epoch (manteniamo i campi storici della UI)
    uint32_t bootEpoch = 0;
    if (systemStatus.ntpSynced) {
      time_t now = time(nullptr);
      if (now > 0) bootEpoch = (uint32_t)now - (millis() / 1000);
    } else if (systemStatus.rtcConnected) {
      DateTime now = rtc.now();
      bootEpoch = (uint32_t)now.unixtime() - (millis() / 1000);
    }
    if (bootEpoch > 0) {
      doc["bootEpoch"] = bootEpoch;
    }

    cachedStatusJson = "";
    serializeJson(doc, cachedStatusJson);
    lastStatusCache = now;
    
    AsyncWebServerResponse* r = request->beginResponse(200, "application/json", cachedStatusJson);
    r->addHeader("Connection", "close");
    r->addHeader("X-Cache", "MISS"); // Debug header
    request->send(r);
        
    Serial.println("✓ Risposta inviata: status JSON");
  });
  
  // API per ottenere le melodie
  server.on("/api/melodies", HTTP_GET, [](AsyncWebServerRequest *request){
    CommandHandler& handler = CommandHandler::getInstance();
    CommandHandler::CommandResult result = handler.getMelodiesList();
    if (!result.success || !result.jsonData) {
      request->send(500, "application/json", String("{\"success\":false,\"message\":\"") + result.message + "\"}");
      return;
    }

    String response;
    serializeJson(*result.jsonData, response);
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

    CommandHandler& handler = CommandHandler::getInstance();
    CommandHandler::CommandResult result = handler.getMelodyDetails((uint8_t)idx);
    if (!result.success || !result.jsonData) {
      request->send(404, "application/json", String("{\"success\":false,\"message\":\"") + result.message + "\"}");
      return;
    }

  String resp; serializeJson(*result.jsonData, resp);
  AsyncWebServerResponse* r = request->beginResponse(200, "application/json", resp);
  r->addHeader("Connection", "close");
  request->send(r);
  });

  // API orario corrente per UI
  server.on("/api/time", HTTP_GET, [](AsyncWebServerRequest *request){
    unsigned long now = millis();
    
    // Cache: se risposta già generata negli ultimi 1s, riutilizzala
    if (now - lastTimeCache < CACHE_DURATION_MS && cachedTimeJson.length() > 0) {
      request->send(200, "application/json", cachedTimeJson);
      return;
    }
    
    CommandHandler& handler = CommandHandler::getInstance();
    CommandHandler::CommandResult result = handler.getCurrentTime();
    if (!result.success || !result.jsonData) {
      request->send(503, "application/json", String("{\"success\":false,\"message\":\"") + result.message + "\"}");
      return;
    }

    cachedTimeJson = "";
    serializeJson(*result.jsonData, cachedTimeJson);
    lastTimeCache = now;
    
    request->send(200, "application/json", cachedTimeJson);
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
    
    // Track max ID for auto-generation
    uint8_t nextId = 1;
    weeklyCount = 0;
    for (JsonObject o : doc["schedules"].as<JsonArray>()){
      if (weeklyCount>=MAX_WEEKLY_SCHEDULES) break;
      WeeklySchedule &e = weeklyArr[weeklyCount++];
      strlcpy(e.name, (o["name"] | ""), sizeof(e.name));
      
      // Preserve existing ID or generate new one
      if (o.containsKey("id")) {
        e.id = o["id"];
        if (e.id >= nextId) nextId = e.id + 1;
      } else {
        e.id = nextId++;
      }
      
      e.dayOfWeek = (DayOfWeek)(int)(o["dayOfWeek"] | 0);
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
    
    // Track max ID for auto-generation
    uint8_t nextId = 1;
    specialCount = 0;
    for (JsonObject o : doc["events"].as<JsonArray>()){
      if (specialCount>=MAX_SPECIAL_EVENTS) break;
      SpecialEvent &e = specialArr[specialCount++];
      strlcpy(e.name, (o["name"] | ""), sizeof(e.name));
      
      // Preserve existing ID or generate new one
      if (o.containsKey("id")) {
        e.id = o["id"];
        if (e.id >= nextId) nextId = e.id + 1;
      } else {
        e.id = nextId++;
      }
      
      e.type = (EventType)(int)(o["type"] | 5);
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
    CommandHandler& handler = CommandHandler::getInstance();
    CommandHandler::CommandResult result = handler.resyncNTP();
    String resp = String("{\"success\":") + (result.success ? "true" : "false") + ",\"ntpSynced\":" + (systemStatus.ntpSynced ? "true" : "false") + ",\"message\":\"" + result.message + "\"}";
    request->send(result.success ? 200 : 503, "application/json", resp);
  });

  // API: backup completo (melodie + schedules). WiFi escluso per sicurezza.
  server.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest *request){
    AsyncResponseStream* s = request->beginResponseStream("application/json");
    // Header
    s->print('{');
    s->print("\"firmwareVersion\":\""); s->print(Config::FIRMWARE_VERSION); s->print("\",");
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
    
    // Ottimizzazione: StaticJsonDocument invece di Dynamic
    StaticJsonDocument<512> doc;
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
    String resp; 
    serializeJson(doc, resp);
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
    String payload = *body;
    delete body; request->_tempObject = nullptr;

    CommandHandler& handler = CommandHandler::getInstance();
    CommandHandler::CommandResult result = handler.testMelody(payload);

    String response = String("{\"success\":") + (result.success ? "true" : "false") + ",\"message\":\"" + result.message + "\"";
    if (result.success && result.jsonData && result.jsonData->containsKey("index")) {
      response += String(",\"index\":") + (*result.jsonData)["index"].as<int>();
    }
    response += "}";
    request->send(result.success ? 200 : 400, "application/json", response);
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
    String payload = *body;
    delete body; request->_tempObject = nullptr;

    CommandHandler& handler = CommandHandler::getInstance();
    CommandHandler::CommandResult result = handler.saveMelody(payload);

    String response = String("{\"success\":") + (result.success ? "true" : "false") + ",\"message\":\"" + result.message + "\"";
    if (result.success && result.jsonData && result.jsonData->containsKey("index")) {
      response += String(",\"index\":") + (*result.jsonData)["index"].as<int>();
    }
    response += "}";
    request->send(result.success ? 200 : 400, "application/json", response);
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
    String payload = *body;
    delete body; request->_tempObject = nullptr;

    CommandHandler& handler = CommandHandler::getInstance();
    CommandHandler::CommandResult result = handler.updateMelody(payload);
    String response = String("{\"success\":") + (result.success ? "true" : "false") + ",\"message\":\"" + result.message + "\"}";
    request->send(result.success ? 200 : 400, "application/json", response);
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
  if (parseErr) { request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON non valido\"}"); return; }
    int idx = doc["index"] | -1;
    if (idx < 0 || idx >= 10) { request->send(400, "application/json", "{\"success\":false,\"message\":\"index non valido\"}"); return; }
    CommandHandler& handler = CommandHandler::getInstance();
    CommandHandler::CommandResult result = handler.deleteMelody((uint8_t)idx);
    String response = String("{\"success\":") + (result.success ? "true" : "false") + ",\"message\":\"" + result.message + "\"}";
    request->send(result.success ? 200 : 400, "application/json", response);
  });

  // API per fermare la melodia in riproduzione (POST e GET)
  server.on("/api/stop-melody", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/stop-melody (POST)");
    CommandHandler& handler = CommandHandler::getInstance();
    CommandHandler::CommandResult result = handler.stopMelody();
    String response = String("{\"success\":") + (result.success ? "true" : "false") + ",\"message\":\"" + result.message + "\"}";
    request->send(result.success ? 200 : 500, "application/json", response);
  });
  server.on("/api/stop-melody", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/stop-melody (GET)");
    CommandHandler& handler = CommandHandler::getInstance();
    CommandHandler::CommandResult result = handler.stopMelody();
    String response = String("{\"success\":") + (result.success ? "true" : "false") + ",\"message\":\"" + result.message + "\"}";
    request->send(result.success ? 200 : 500, "application/json", response);
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
      // Salva le credenziali su SPIFFS (ottimizzazione: StaticJsonDocument)
      StaticJsonDocument<512> doc;
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

  // === API CONFIGURAZIONE RETE (IP STATICO/DHCP) ===
  
  // GET /api/network-config - Ottieni configurazione di rete attuale
  server.on("/api/network-config", HTTP_GET, [](AsyncWebServerRequest *request){
    DynamicJsonDocument doc(512);
    
    doc["dhcp"] = networkConfig.useDHCP;
    doc["static_ip"] = networkConfig.staticIP.toString();
    doc["gateway"] = networkConfig.gateway.toString();
    doc["subnet"] = networkConfig.subnet.toString();
    doc["dns1"] = networkConfig.dns1.toString();
    doc["dns2"] = networkConfig.dns2.toString();
    
    // Aggiungi info corrente
    doc["current_ip"] = WiFi.localIP().toString();
    doc["current_gateway"] = WiFi.gatewayIP().toString();
    doc["current_subnet"] = WiFi.subnetMask().toString();
    doc["current_dns"] = WiFi.dnsIP().toString();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });
  
  // POST /api/network-config - Salva nuova configurazione di rete
  server.on("/api/network-config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, 
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
    
    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, data, len);
    
    if (error) {
      request->send(400, "application/json", "{\"success\":false,\"error\":\"JSON non valido\"}");
      return;
    }
    
    // Validazione parametri
    bool useDHCP = doc["dhcp"] | true;
    
    if (!useDHCP) {
      // Validazione IP statico
      String ip = doc["static_ip"] | "";
      String gw = doc["gateway"] | "";
      String sn = doc["subnet"] | "";
      String d1 = doc["dns1"] | "";
      String d2 = doc["dns2"] | "";
      
      // Verifica formato IP
      IPAddress testIP, testGW, testSN, testD1, testD2;
      if (!testIP.fromString(ip) || !testGW.fromString(gw) || 
          !testSN.fromString(sn) || !testD1.fromString(d1) || !testD2.fromString(d2)) {
        request->send(400, "application/json", 
          "{\"success\":false,\"error\":\"Formato IP non valido\"}");
        return;
      }
      
      // Aggiorna configurazione
      networkConfig.useDHCP = false;
      networkConfig.staticIP = testIP;
      networkConfig.gateway = testGW;
      networkConfig.subnet = testSN;
      networkConfig.dns1 = testD1;
      networkConfig.dns2 = testD2;
    } else {
      networkConfig.useDHCP = true;
    }
    
    // Salva su SPIFFS
    if (saveNetworkConfig()) {
      request->send(200, "application/json", 
        "{\"success\":true,\"message\":\"Configurazione rete salvata, riavvio necessario\"}");
      Serial.println("[NETWORK] Configurazione salvata, riavviare per applicare");
    } else {
      request->send(500, "application/json", 
        "{\"success\":false,\"error\":\"Errore salvataggio configurazione\"}");
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
  
  // === API RECOVERY SYSTEM (v2.3.8) ===
  
  // GET /api/recovery-status - Stato sistema recovery
  server.on("/api/recovery-status", HTTP_GET, [](AsyncWebServerRequest *request){
    DynamicJsonDocument doc(512);
    doc["ap_mode_active"] = recoveryManager.isAPModeActive();
    doc["wifi_fail_count"] = recoveryManager.getWiFiFailCount();
    doc["wifi_max_retries"] = 3;
    doc["static_ip_fail_count"] = wifiManager.getStaticIPFailCount();
    doc["factory_reset_pin"] = 0; // GPIO0 = BOOT
    doc["factory_reset_hold_ms"] = 10000;
    
    if (recoveryManager.isAPModeActive()) {
      doc["message"] = "🆘 RECOVERY MODE - Sistema in modalità AP";
      doc["ap_ssid"] = "ChurchBells_Recovery";
      doc["ap_ip"] = "192.168.4.1";
    } else if (recoveryManager.getWiFiFailCount() > 0) {
      doc["message"] = String("⚠️ Tentativi WiFi falliti: ") + String(recoveryManager.getWiFiFailCount()) + "/3";
    } else {
      doc["message"] = "✅ Sistema normale - Nessun recovery attivo";
    }
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
  });
  
  // POST /api/trigger-recovery - Attiva manualmente AP mode (debug)
  server.on("/api/trigger-recovery", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("[API] 🆘 Trigger manuale recovery mode richiesto");
    recoveryManager.startAPMode();
    
    DynamicJsonDocument doc(256);
    doc["success"] = true;
    doc["ap_ssid"] = "ChurchBells_Recovery";
    doc["ap_ip"] = "192.168.4.1";
    doc["ap_password"] = "recovery123";
    doc["message"] = "Recovery mode attivato manualmente";
    
    String output;
    serializeJson(doc, output);
    request->send(200, "application/json", output);
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
    
    // Usa CommandHandler per operazione unificata
    CommandHandler& handler = CommandHandler::getInstance();
    CommandHandler::CommandResult result = handler.toggleBells(enabled);
    
    String response = String("{\"success\":") + (result.success ? "true" : "false") 
                    + ",\"enabled\":" + (enabled ? "true" : "false")
                    + ",\"message\":\"" + result.message + "\"}";
    request->send(200, "application/json", response);
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
      StaticJsonDocument<256> doc;
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
    
    // Usa CommandHandler per operazione unificata
    CommandHandler& handler = CommandHandler::getInstance();
    CommandHandler::CommandResult result = handler.toggleBells(enabled);
    
    String response = String("{\"success\":") + (result.success ? "true" : "false")
                    + ",\"enabled\":" + (enabled ? "true" : "false")
                    + ",\"message\":\"" + result.message + "\"}";
    request->send(200, "application/json", response);
  });

  // API diagnostica: lettura stato pin relè e LED
  server.on("/api/relay-status", HTTP_GET, [](AsyncWebServerRequest *request){
    // Rate limiting (150ms minimo tra richieste)
    static uint32_t lastMs = 0;
    uint32_t now = millis();
    if (now - lastMs < 150) {
      request->send(429, "text/plain", "Too Many Requests");
      return;
    }
    lastMs = now;
    
    // Cache: se risposta già generata negli ultimi 1s, riutilizzala
    if (now - lastRelayStatusCache < CACHE_DURATION_MS && cachedRelayStatusJson.length() > 0) {
      request->send(200, "application/json", cachedRelayStatusJson);
      return;
    }
    
    Serial.println("📡 Richiesta ricevuta: /api/relay-status (cache miss)");
    
    // Ottimizzazione: StaticJsonDocument invece di Dynamic
    StaticJsonDocument<256> doc;
    doc["relay_active_level"] = Hardware::RELAY_ACTIVE_HIGH ? "HIGH" : "LOW";
    doc["enabled"] = systemStatus.bellsEnabled;
    
    int r1 = digitalRead(RELAY1_PIN);
    int r2 = digitalRead(RELAY2_PIN);
    int led = digitalRead(STATUS_LED_PIN);
    
    doc["relay1_raw"] = r1;
    doc["relay2_raw"] = r2;
    doc["statusLed_raw"] = led;
    doc["relay1_on"] = (r1 == RELAY_ON);
    doc["relay2_on"] = (r2 == RELAY_ON);
    doc["master_on"] = (digitalRead(MASTER_RELAY_PIN) == Hardware::MASTER_ON);
    
    // Serializza e aggiorna cache
    cachedRelayStatusJson = "";
    serializeJson(doc, cachedRelayStatusJson);
    lastRelayStatusCache = now;
    
    request->send(200, "application/json", cachedRelayStatusJson);
  });

  // API diagnostica: accende (value=1) o spegne (value=0) un relè, indipendentemente dalla polarità.
  // ATTENZIONE: value=1 resta acceso finché non si invia value=0 (non usare con i martelli collegati)
  server.on("/api/set-relay", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/set-relay");
    int relay = 1;
    int value = -1;
    if (request->hasParam("relay")) relay = request->getParam("relay")->value().toInt();
    if (request->hasParam("value")) value = request->getParam("value")->value().toInt();

    // relay=3: relè generale (GPIO33). level=0/1 imposta il livello grezzo del pin.
    if (relay == 3 && request->hasParam("level")) {
      int level = request->getParam("level")->value().toInt() ? HIGH : LOW;
      pinMode(MASTER_RELAY_PIN, OUTPUT);
      digitalWrite(MASTER_RELAY_PIN, level);
      String resp = "{\"success\":true,\"relay\":3,\"level\":" + String(level) +
                    ",\"read\":" + String(digitalRead(MASTER_RELAY_PIN)) + "}";
      request->send(200, "application/json", resp);
      Serial.printf("Set master GPIO%d level -> %d\n", MASTER_RELAY_PIN, level);
      return;
    }
    if (relay < 1 || relay > 3) {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"relay must be 1, 2 or 3\"}");
      return;
    }
    if (value != 0 && value != 1) {
      request->send(400, "application/json", "{\"success\":false,\"message\":\"value must be 0 or 1\"}");
      return;
    }

    if (relay == 3) {
      pinMode(MASTER_RELAY_PIN, Hardware::MASTER_PIN_MODE);
      digitalWrite(MASTER_RELAY_PIN, value ? Hardware::MASTER_ON : Hardware::MASTER_OFF);
      String resp = "{\"success\":true,\"relay\":3,\"value\":" + String(value) +
                    ",\"read\":" + String(digitalRead(MASTER_RELAY_PIN)) + "}";
      request->send(200, "application/json", resp);
      Serial.printf("Set master -> %d\n", value);
      return;
    }
    int pin = (relay == 1) ? RELAY1_PIN : RELAY2_PIN;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, value ? RELAY_ON : RELAY_OFF);
    String resp = "{\"success\":true,\"relay\":" + String(relay) + ",\"value\":" + String(value) + "}";
    request->send(200, "application/json", resp);
    Serial.printf("Set relay %d -> %d\n", relay, value);
  });
  
  // === ENDPOINT TELEGRAM BOT (v2.3.9) ===
  
  // GET /api/telegram/status - Ottieni stato bot e lista utenti
  server.on("/api/telegram/status", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/telegram/status");
    
    StaticJsonDocument<1024> doc;
    doc["enabled"] = telegramBot.isEnabled();
    doc["initialized"] = telegramBot.isEnabled(); // Se enabled, è anche initialized
    doc["messageCount"] = telegramBot.getMessageCount();
    doc["commandCount"] = telegramBot.getCommandCount();
    doc["lastUpdate"] = telegramBot.getLastUpdateTime();
    
    // Token configurato (mascherato per sicurezza)
    String token = String(TELEGRAM_BOT_TOKEN);
    bool tokenConfigured = (token.length() > 0 && token != "123456789:ABCdefGHIjklMNOpqrsTUVwxyz");
    doc["tokenConfigured"] = tokenConfigured;
    if (tokenConfigured) {
      // Mostra solo primi e ultimi 4 caratteri
      String maskedToken = token.substring(0, 8) + "..." + token.substring(token.length() - 4);
      doc["tokenMasked"] = maskedToken;
    }
    
    // Lista utenti autorizzati
    JsonArray users = doc.createNestedArray("authorizedUsers");
    String usersList = telegramBot.getAuthorizedUsers();
    if (usersList.length() > 0) {
      // Parsing della lista (formato: "id1, id2, id3")
      int startIdx = 0;
      int commaIdx = usersList.indexOf(',', startIdx);
      while (commaIdx > 0) {
        String userId = usersList.substring(startIdx, commaIdx);
        userId.trim();
        if (userId.length() > 0) users.add(userId);
        startIdx = commaIdx + 1;
        commaIdx = usersList.indexOf(',', startIdx);
      }
      // Ultimo elemento
      String userId = usersList.substring(startIdx);
      userId.trim();
      if (userId.length() > 0) users.add(userId);
    }
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
  });
  
  // POST /api/telegram/add-user - Aggiungi utente autorizzato
  server.on("/api/telegram/add-user", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      Serial.println("📡 Richiesta ricevuta: /api/telegram/add-user");
      
      // Gestione body a chunk: accumula fino a ricezione completa
      if (index == 0) {
        String* body = new String();
        body->reserve(total);
        request->_tempObject = body;
      }
      
      String* body = reinterpret_cast<String*>(request->_tempObject);
      if (!body) {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Errore interno parsing body\"}");
        return;
      }
      
      body->concat(reinterpret_cast<const char*>(data), len);
      if (index + len < total) {
        return;
      }
      
      // Parse JSON body
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, *body);
      delete body;
      request->_tempObject = nullptr;
      
      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON non valido\"}");
        return;
      }
      
      const char* chatIdKey = nullptr;
      if (doc.containsKey("chatId")) {
        chatIdKey = "chatId";
      } else if (doc.containsKey("clientId")) {
        chatIdKey = "clientId";
      } else if (doc.containsKey("chat_id")) {
        chatIdKey = "chat_id";
      }

      if (!chatIdKey) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"chatId/clientId mancante\"}");
        return;
      }
      
      String chatId = doc[chatIdKey].as<String>();
      chatId.trim();
      
      if (chatId.length() == 0) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"chatId vuoto\"}");
        return;
      }
      
      // Valida che sia un numero
      bool isNumeric = true;
      for (size_t i = 0; i < chatId.length(); i++) {
        if (!isdigit(chatId[i]) && chatId[i] != '-') {
          isNumeric = false;
          break;
        }
      }
      
      if (!isNumeric) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"chatId deve essere numerico\"}");
        return;
      }
      
      // Aggiungi utente
      if (telegramBot.addAuthorizedUser(chatId)) {
        saveTelegramUsersToFS();  // Persistenza su SPIFFS
        String resp = "{\"success\":true,\"message\":\"Utente aggiunto\",\"chatId\":\"" + chatId + "\"}";
        request->send(200, "application/json", resp);
        Serial.printf("✓ Utente Telegram aggiunto: %s\n", chatId.c_str());
        
        // Invia notifica all'utente
        telegramBot.sendMessage(chatId, "✅ Sei stato autorizzato a controllare il sistema campanile! Usa /help per vedere i comandi disponibili.");
      } else {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Limite massimo utenti raggiunto (10) o già presente\"}");
      }
    }
  );
  
  // POST /api/telegram/remove-user - Rimuovi utente autorizzato
  server.on("/api/telegram/remove-user", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      Serial.println("📡 Richiesta ricevuta: /api/telegram/remove-user");
      
      // Gestione body a chunk: accumula fino a ricezione completa
      if (index == 0) {
        String* body = new String();
        body->reserve(total);
        request->_tempObject = body;
      }
      
      String* body = reinterpret_cast<String*>(request->_tempObject);
      if (!body) {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Errore interno parsing body\"}");
        return;
      }
      
      body->concat(reinterpret_cast<const char*>(data), len);
      if (index + len < total) {
        return;
      }
      
      // Parse JSON body
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, *body);
      delete body;
      request->_tempObject = nullptr;
      
      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON non valido\"}");
        return;
      }
      
      const char* chatIdKey = nullptr;
      if (doc.containsKey("chatId")) {
        chatIdKey = "chatId";
      } else if (doc.containsKey("clientId")) {
        chatIdKey = "clientId";
      } else if (doc.containsKey("chat_id")) {
        chatIdKey = "chat_id";
      }

      if (!chatIdKey) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"chatId/clientId mancante\"}");
        return;
      }
      
      String chatId = doc[chatIdKey].as<String>();
      chatId.trim();
      
      if (telegramBot.removeAuthorizedUser(chatId)) {
        saveTelegramUsersToFS();  // Persistenza su SPIFFS
        String resp = "{\"success\":true,\"message\":\"Utente rimosso\",\"chatId\":\"" + chatId + "\"}";
        request->send(200, "application/json", resp);
        Serial.printf("✓ Utente Telegram rimosso: %s\n", chatId.c_str());
        
        // Notifica utente rimosso
        telegramBot.sendMessage(chatId, "⚠️ La tua autorizzazione al sistema campanile è stata revocata.");
      } else {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"Utente non trovato\"}");
      }
    }
  );
  
  // POST /api/telegram/toggle - Abilita/disabilita bot
  server.on("/api/telegram/toggle", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      Serial.println("📡 Richiesta ricevuta: /api/telegram/toggle");
      
      // Gestione body a chunk: accumula fino a ricezione completa
      if (index == 0) {
        String* body = new String();
        body->reserve(total);
        request->_tempObject = body;
      }
      
      String* body = reinterpret_cast<String*>(request->_tempObject);
      if (!body) {
        request->send(500, "application/json", "{\"success\":false,\"message\":\"Errore interno parsing body\"}");
        return;
      }
      
      body->concat(reinterpret_cast<const char*>(data), len);
      if (index + len < total) {
        return;
      }
      
      StaticJsonDocument<128> doc;
      DeserializationError error = deserializeJson(doc, *body);
      delete body;
      request->_tempObject = nullptr;
      
      if (error) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"JSON non valido\"}");
        return;
      }
      
      if (!doc.containsKey("enabled")) {
        request->send(400, "application/json", "{\"success\":false,\"message\":\"enabled mancante\"}");
        return;
      }
      
      bool enabled = doc["enabled"].as<bool>();
      telegramBot.setEnabled(enabled);
      
      String resp = "{\"success\":true,\"enabled\":" + String(enabled ? "true" : "false") + "}";
      request->send(200, "application/json", resp);
      Serial.printf("✓ Bot Telegram %s\n", enabled ? "abilitato" : "disabilitato");
    }
  );
  
  // POST /api/telegram/send-test - Invia messaggio di test
  server.on("/api/telegram/send-test", HTTP_POST, [](AsyncWebServerRequest *request){
    Serial.println("📡 Richiesta ricevuta: /api/telegram/send-test");
    
    String testMsg = "🧪 *Messaggio di Test*\n\n";
    testMsg += "Questo è un messaggio di test dal sistema campanile.\n";
    testMsg += "Se lo ricevi, il bot funziona correttamente! ✅";
    
    telegramBot.sendNotification(testMsg);
    
    request->send(200, "application/json", "{\"success\":true,\"message\":\"Messaggio di test inviato\"}");
  });
  
  // === ENDPOINT DIAGNOSTICA AVANZATA ===
  server.on("/api/diagnostics", HTTP_GET, [](AsyncWebServerRequest *request){
    Serial.println("📊 Richiesta ricevuta: /api/diagnostics");
    
    // Ottimizzazione: StaticJsonDocument invece di Dynamic
    StaticJsonDocument<2048> doc;
    
    // === MEMORIA ===
    JsonObject heap = doc.createNestedObject("heap");
    heap["free"] = ESP.getFreeHeap();
    heap["min"] = ESP.getMinFreeHeap();
    heap["size"] = ESP.getHeapSize();
    heap["maxAlloc"] = ESP.getMaxAllocHeap();
    
    // Calcola frammentazione: quanto dello heap libero è allocabile in un blocco
    uint32_t freeHeap = ESP.getFreeHeap();
    uint32_t maxAlloc = ESP.getMaxAllocHeap();
    float fragmentation = 0;
    if (freeHeap > 0) {
      fragmentation = 100.0 - ((float)maxAlloc * 100.0 / (float)freeHeap);
    }
    heap["fragmentation"] = round(fragmentation * 10) / 10.0; // 1 decimale
    
    // === CPU ===
    JsonObject cpu = doc.createNestedObject("cpu");
    cpu["frequency"] = getCpuFrequencyMhz();
    cpu["cores"] = ESP.getChipCores();
    
    // === WiFi ===
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["connected"] = WiFi.status() == WL_CONNECTED;
    if (WiFi.status() == WL_CONNECTED) {
      wifi["rssi"] = WiFi.RSSI();
      wifi["txPower"] = WiFi.getTxPower();
      wifi["ssid"] = WiFi.SSID();
      wifi["ip"] = WiFi.localIP().toString();
      
      // Qualità segnale
      int rssi = WiFi.RSSI();
      String quality = "Scarso";
      if (rssi >= -50) quality = "Eccellente";
      else if (rssi >= -60) quality = "Buono";
      else if (rssi >= -70) quality = "Discreto";
      wifi["quality"] = quality;
    }
    
    // === TEMPERATURA ===
    JsonObject temp = doc.createNestedObject("temperature");
    temp["internal"] = temperatureRead();
    temp["esp32"] = systemStatus.esp32Temperature;
    temp["warning"] = systemStatus.temperatureWarning;
    temp["protection"] = systemStatus.thermalProtection;
    
    // === UPTIME ===
    JsonObject uptime = doc.createNestedObject("uptime");
    uint32_t uptimeSeconds = millis() / 1000;
    uptime["seconds"] = uptimeSeconds;
    uptime["minutes"] = uptimeSeconds / 60;
    uptime["hours"] = uptimeSeconds / 3600;
    uptime["days"] = uptimeSeconds / 86400;
    
    // === SISTEMA ===
    JsonObject system = doc.createNestedObject("system");
    system["firmware"] = Config::FIRMWARE_VERSION;
    system["chipModel"] = ESP.getChipModel();
    system["chipRevision"] = ESP.getChipRevision();
    system["sdkVersion"] = ESP.getSdkVersion();
    system["flashSize"] = ESP.getFlashChipSize();
    system["flashSpeed"] = ESP.getFlashChipSpeed() / 1000000; // MHz
    
    // === TASK STACK ===
    system["stackHighWaterMark"] = uxTaskGetStackHighWaterMark(NULL);
    
    // === CAMPANILE STATUS ===
    JsonObject bells = doc.createNestedObject("bells");
    bells["enabled"] = systemStatus.bellsEnabled;
    bells["totalRings"] = systemStatus.totalBellRings;
    bells["lastRingTime"] = systemStatus.lastBellTime;
    bells["schedulerActive"] = schedulerActive;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
    
    Serial.println("📊 Diagnostica inviata");
  });
  
  // Serve i file statici da SPIFFS (no-cache per evitare UI vecchie)
  // Registrato alla fine per non ombreggiare le API.
  // Protezione Basic Auth per UI statica
  // Pagine web incorporate nel firmware, già compresse (vedi tools/embed_web.py)
  auto sendPage = [](AsyncWebServerRequest *request, const uint8_t *data, size_t len) {
    if (!request->authenticate(ADMIN_USER, ADMIN_PASSWORD)) {
      return request->requestAuthentication();
    }
    AsyncWebServerResponse *r = request->beginResponse_P(200, "text/html", data, len);
    r->addHeader("Content-Encoding", "gzip");
    r->addHeader("Cache-Control", "no-cache");
    request->send(r);
  };
  server.on("/", HTTP_GET, [sendPage](AsyncWebServerRequest *request){ sendPage(request, WEB_INDEX_GZ, WEB_INDEX_GZ_LEN); });
  server.on("/index.html", HTTP_GET, [sendPage](AsyncWebServerRequest *request){ sendPage(request, WEB_INDEX_GZ, WEB_INDEX_GZ_LEN); });
  server.on("/diag.html", HTTP_GET, [sendPage](AsyncWebServerRequest *request){ sendPage(request, WEB_DIAG_GZ, WEB_DIAG_GZ_LEN); });
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
    } else {
      request->send(404, "text/plain", "Not Found");
    }
  });
  // Nota: l'handler onNotFound ora gestisce sia 404 API che le risorse statiche con auth

  Serial.println("✓ Web Server configurato");
}

// === FUNZIONI DIAGNOSTICA ===
void printWiFiDiagnostics() {
  Serial.println("\n=== 📡 DIAGNOSTICA WIFI ===");
  Serial.printf("Stato connessione: %s\n", WiFi.status() == WL_CONNECTED ? "CONNESSA" : "DISCONNESSA");
  Serial.printf("Modalità: %s\n", apMode ? "AP" : "STA");
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("SSID: %s\n", WiFi.SSID().c_str());
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
    Serial.printf("Signal Strength (RSSI): %d dBm\n", WiFi.RSSI());
    Serial.printf("MAC Address: %s\n", WiFi.macAddress().c_str());
    Serial.printf("Tx Power: %d (ridotta per thermal management)\n", WiFi.getTxPower());
  } else {
    Serial.printf("Motivo disconnessione: ");
    switch(WiFi.status()) {
      case WL_NO_SSID_AVAIL: Serial.println("SSID non disponibile"); break;
      case WL_CONNECT_FAILED: Serial.println("Connessione fallita"); break;
      case WL_CONNECTION_LOST: Serial.println("Connessione persa"); break;
      case WL_DISCONNECTED: Serial.println("Disconnesso"); break;
      default: Serial.printf("Codice: %d\n", WiFi.status()); break;
    }
    
    if (apMode) {
      Serial.printf("AP SSID: %s\n", AP_SSID);
      Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
      Serial.printf("Clients connessi: %d\n", WiFi.softAPgetStationNum());
    }
  }
  
  Serial.printf("systemStatus.wifiConnected: %s\n", systemStatus.wifiConnected ? "true" : "false");
  Serial.printf("Uptime WiFi check: %lu ms\n", millis() - lastWiFiCheck);
  Serial.println("===========================\n");
}

void printNTPDiagnostics() {
  Serial.println("\n=== 🕐 DIAGNOSTICA NTP/ORARIO ===");
  Serial.printf("NTP Sincronizzato: %s\n", systemStatus.ntpSynced ? "SÌ" : "NO");
  Serial.printf("RTC Connesso: %s\n", systemStatus.rtcConnected ? "SÌ" : "NO");
  Serial.printf("Ora manuale valida: %s\n", manualTimeValid ? "SÌ" : "NO");
  
  // Epoch time dal sistema
  time_t now = time(nullptr);
  if (now > 1000000) {
    struct tm* timeinfo = localtime(&now);
    Serial.printf("System time (NTP): %04d-%02d-%02d %02d:%02d:%02d\n",
                  timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday,
                  timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
    Serial.printf("Epoch timestamp: %lu\n", (unsigned long)now);
    Serial.printf("Timezone: UTC+%d (%s)\n", utcOffsetHours, isDST ? "DST" : "Standard");
  } else {
    Serial.println("⚠️ System time non valido (epoch < 1970)");
  }
  
  // RTC time se disponibile
  if (systemStatus.rtcConnected) {
    DateTime rtcNow = rtc.now();
    Serial.printf("RTC time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  rtcNow.year(), rtcNow.month(), rtcNow.day(),
                  rtcNow.hour(), rtcNow.minute(), rtcNow.second());
    
    // Calcola drift tra RTC e NTP
    if (now > 1000000) {
      long drift = (long)now - (long)rtcNow.unixtime();
      Serial.printf("Drift NTP-RTC: %ld secondi ", drift);
      if (abs(drift) > 60) {
        Serial.println("⚠️ ELEVATO - Richiede re-sync");
      } else if (abs(drift) > 10) {
        Serial.println("⚠️ Moderato");
      } else {
        Serial.println("✓ OK");
      }
    }
  }
  
  Serial.printf("Ultimo NTP sync: %lu ms fa\n", millis() - lastNTPSync);
  Serial.printf("Prossimo NTP sync tra: %lu ms\n", 3600000 - (millis() - lastNTPSync));
  Serial.println("=================================\n");
}

// === DISPLAY ===
// Due schermate, ridisegnate solo quando cambia qualcosa (niente refresh ogni secondo):
//  - Home: ora HH:MM grande, data, riga di stato (WiFi, campane, temperatura)
//  - Melodia: sfondo unico con nome melodia, "IN CORSO" e barra di avanzamento
// Il font GLCD non ha simboli Unicode: gli indicatori sono pallini disegnati.

static const uint16_t DISP_BG = 0x0841;       // quasi nero
static const uint16_t DISP_ACCENT = 0xFD20;   // ambra/oro
static const uint16_t DISP_DIM = 0x8410;      // grigio

static bool getDisplayTime(int &hh, int &mm, int &dd, int &mo, int &yy) {
  if (systemStatus.rtcConnected) {
    DateTime now = rtc.now();
    hh = now.hour(); mm = now.minute(); dd = now.day(); mo = now.month(); yy = now.year();
    return true;
  }
  if (manualTimeValid) {
    hh = manualTime.hour(); mm = manualTime.minute(); dd = manualTime.day(); mo = manualTime.month(); yy = manualTime.year();
    return true;
  }
  if (systemStatus.ntpSynced) {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    hh = t->tm_hour; mm = t->tm_min; dd = t->tm_mday; mo = t->tm_mon + 1; yy = t->tm_year + 1900;
    return true;
  }
  return false;
}

static void drawStatusDot(int x, int y, uint16_t color, const char *label) {
  tft.fillCircle(x + 4, y + 7, 4, color);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(TFT_WHITE, DISP_BG);
  tft.drawString(label, x + 12, y, 2);
}

static void drawHomeScreen(bool timeValid, int hh, int mm, int dd, int mo, int yy) {
  const int16_t W = tft.width();
  const int16_t H = tft.height();
  static const char *days[] = {"Dom", "Lun", "Mar", "Mer", "Gio", "Ven", "Sab"};

  tft.fillScreen(DISP_BG);

  // Intestazione
  tft.fillRect(0, 0, W, 22, DISP_ACCENT);
  tft.setTextColor(DISP_BG, DISP_ACCENT);
  tft.setTextDatum(ML_DATUM);
  tft.drawString("CAMPANE", 6, 11, 2);
  if (timeValid) {
    char dateStr[20];
    int dow = DateTime(yy, mo, dd).dayOfTheWeek();
    snprintf(dateStr, sizeof(dateStr), "%s %02d/%02d/%04d", days[dow % 7], dd, mo, yy);
    tft.setTextDatum(MR_DATUM);
    tft.drawString(dateStr, W - 6, 11, 2);
  }

  // Orologio grande (font 7 = 7 segmenti, 48px)
  tft.setTextDatum(MC_DATUM);
  if (timeValid) {
    char timeStr[6];
    snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hh, mm);
    tft.setTextColor(TFT_WHITE, DISP_BG);
    tft.drawString(timeStr, W / 2, 60, 7);
  } else {
    tft.setTextColor(TFT_RED, DISP_BG);
    tft.drawString("ORA NON IMPOSTATA", W / 2, 60, 2);
  }

  // Indirizzo IP (cambia con il DHCP): serve per aprire la pagina web
  IPAddress ip = systemStatus.wifiConnected ? WiFi.localIP() : WiFi.softAPIP();
  tft.setTextColor(DISP_ACCENT, DISP_BG);
  tft.drawString(ip.toString(), W / 2, 97, 2);

  // Riga di stato
  int y = H - 22;
  tft.drawFastHLine(8, y - 6, W - 16, DISP_DIM);
  drawStatusDot(6, y, systemStatus.wifiConnected ? TFT_GREEN : TFT_YELLOW,
                systemStatus.wifiConnected ? "WiFi" : "AP");
  drawStatusDot(70, y, systemStatus.bellsEnabled ? TFT_GREEN : TFT_RED,
                systemStatus.bellsEnabled ? "Campane ON" : "Campane OFF");
  uint16_t tColor = systemStatus.thermalProtection ? TFT_RED
                  : systemStatus.temperatureWarning ? TFT_YELLOW : TFT_GREEN;
  char tempStr[10];
  snprintf(tempStr, sizeof(tempStr), "%dC", (int)systemStatus.esp32Temperature);
  drawStatusDot(180, y, tColor, tempStr);
}

static void drawMelodyProgress(uint8_t played, uint8_t total) {
  const int16_t W = tft.width();
  const int x = 20, y = 108, w = W - 40, h = 10;
  int fill = total ? (int)((long)w * played / total) : 0;
  tft.drawRect(x - 1, y - 1, w + 2, h + 2, TFT_WHITE);
  tft.fillRect(x, y, fill, h, TFT_WHITE);
  tft.fillRect(x + fill, y, w - fill, h, TFT_NAVY);
}

static void drawMelodyScreen(uint8_t melodyIndex) {
  const int16_t W = tft.width();
  tft.fillScreen(TFT_NAVY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(DISP_ACCENT, TFT_NAVY);
  tft.drawString("IN CORSO", W / 2, 22, 2);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  const char *name = bellController.getMelodyName(melodyIndex);
  // Font 4 (26px) se il nome entra, altrimenti font 2
  if (tft.textWidth(name, 4) <= W - 10) {
    tft.drawString(name, W / 2, 62, 4);
  } else {
    tft.drawString(name, W / 2, 62, 2);
  }
}

void updateDisplay() {
  // Temperatura per la riga di stato e per i controlli termici
  systemStatus.esp32Temperature = getESP32Temperature();

  static int lastMode = -1;          // 0 = home, 1 = melodia
  static uint32_t lastSignature = 0;
  static uint8_t lastProgress = 255;

  if (bellController.isPlayingMelody()) {
    uint8_t idx = bellController.getCurrentMelodyIndex();
    if (lastMode != 1 || lastSignature != idx) {
      drawMelodyScreen(idx);
      lastMode = 1;
      lastSignature = idx;
      lastProgress = 255;
    }
    uint8_t played = bellController.getCurrentNoteIndex();
    if (played != lastProgress) {
      drawMelodyProgress(played, (uint8_t)bellController.getMelodyNoteCount(idx));
      lastProgress = played;
    }
    return;
  }

  int hh = 0, mm = 0, dd = 0, mo = 0, yy = 0;
  bool timeValid = getDisplayTime(hh, mm, dd, mo, yy);

  // Firma di tutto ciò che appare in home: si ridisegna solo se cambia
  uint32_t sig = timeValid ? (uint32_t)(dd * 1440 + hh * 60 + mm) : 0xFFFFF;
  sig = sig * 31 + (systemStatus.wifiConnected ? 1 : 0);
  sig = sig * 31 + (uint32_t)(systemStatus.wifiConnected ? WiFi.localIP() : WiFi.softAPIP());
  sig = sig * 31 + (systemStatus.bellsEnabled ? 1 : 0);
  // Solo lo stato termico: il valore in gradi si aggiorna al cambio minuto (evita ridisegni continui)
  sig = sig * 31 + (systemStatus.thermalProtection ? 2 : systemStatus.temperatureWarning ? 1 : 0);

  if (lastMode != 0 || sig != lastSignature) {
    drawHomeScreen(timeValid, hh, mm, dd, mo, yy);
    lastMode = 0;
    lastSignature = sig;
  }
}


