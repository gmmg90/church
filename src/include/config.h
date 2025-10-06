#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ========== CONFIGURAZIONE HARDWARE ==========
// Pin per T-Display (già configurati in platformio.ini)

// Pin per RTC DS3231 (I2C)
#define RTC_SDA_PIN 21
#define RTC_SCL_PIN 22

// Pin per Relè Campane
#define RELAY1_PIN 25  // Campana 1 (piccola/segnale)
#define RELAY2_PIN 26  // Campana 2 (grande/principale)

// Buzzer rimosso: non più utilizzato

// Pin per LED di stato
#define STATUS_LED_PIN 2

// Pin per pulsante di configurazione WiFi (boot button)
#define CONFIG_BUTTON_PIN 0   // Pulsante boot (GPIO0)

// Pulsante fisico per avvio rapido "Funerale" (GPIO27 - pulsante esterno)
#ifndef FUNERAL_BUTTON_PIN
#define FUNERAL_BUTTON_PIN 27
#endif

// Pulsante fisico per avvio rapido "Chiamata Messa" (GPIO32 - pulsante esterno)
#ifndef MASS_BUTTON_PIN
#define MASS_BUTTON_PIN 32
#endif

// ========== CONFIGURAZIONE RETE ==========
// WiFi Credentials (da modificare)
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;

// Web Server
#define WEB_SERVER_PORT 80
#define ADMIN_PASSWORD "chiesa123"
// Utente per autenticazione Basic (UI Web). Cambia se necessario.
#ifndef ADMIN_USER
#define ADMIN_USER "admin"
#endif

// Debug e logging (ridotti per evitare surriscaldamento)
#define DEBUG_SCHEDULING false
#define DEBUG_MELODY_PLAYBACK false           // Cambiato da true a false
#define SERIAL_DEBUG_INTERVAL 60000           // Aumentato da 10s a 60s
#define SCHEDULE_DEBUG_INTERVAL 300000        // Aumentato da 60s a 5 minuti

// Comportamento all'avvio
#define AUTO_ENABLE_BELLS_ON_STARTUP true  // Se true, le campane sono sempre abilitate all'avvio
#define STARTUP_TEST_BELL false            // Se true, esegue un test rapido all'avvio

// ========== CONFIGURAZIONE SISTEMA ==========
// Colori per display
#define ORANGE 0xFD20   // Arancione per indicatori

// Timing
#define DISPLAY_UPDATE_INTERVAL 1000    // Aggiornamento display (ms)
#define SCHEDULE_CHECK_INTERVAL 30000   // Controllo programmazione (ms)
#define BELL_MIN_PULSE 100              // Durata minima impulso campana (ms)
#define BELL_MAX_PULSE 2000             // Durata massima impulso campana (ms)
#define BELL_MIN_DELAY 50               // Ritardo minimo tra impulsi (ms)
#define BELL_MAX_DELAY 5000             // Ritardo massimo tra impulsi (ms)

// Programmazione
#define MAX_WEEKLY_SCHEDULES 64         // Max programmazioni settimanali (aumentato da 20)
#define MAX_SPECIAL_EVENTS 10           // Max eventi speciali
#define MAX_MELODY_STEPS 120            // Max passi per melodia (aumentato per scampanio fitto)

// Monitoraggio temperatura ESP32
#define TEMP_CHECK_INTERVAL 30000       // Controllo temperatura ogni 30 secondi
#define TEMP_WARNING_THRESHOLD 70.0     // Soglia avviso temperatura (°C)
#define TEMP_CRITICAL_THRESHOLD 80.0    // Soglia critica temperatura (°C)
#define TEMP_SHUTDOWN_THRESHOLD 85.0    // Soglia spegnimento protezione (°C)

// ========== STRUTTURE DATI ==========

// Giorni della settimana
enum DayOfWeek {
  DOMENICA = 0,
  LUNEDI = 1,
  MARTEDI = 2,
  MERCOLEDI = 3,
  GIOVEDI = 4,
  VENERDI = 5,
  SABATO = 6
};

// Tipi di eventi
enum EventType {
  EVENTO_MESSA = 0,
  EVENTO_ANGELUS = 1,
  EVENTO_MATRIMONIO = 2,
  EVENTO_FUNERALE = 3,
  EVENTO_FESTA = 4,
  EVENTO_PERSONALIZZATO = 5
};

// Struttura per un singolo "colpo" di campana
struct BellNote {
  uint8_t bellNumber;     // 1 o 2
  uint16_t duration;      // Durata impulso (ms)
  uint16_t delay;         // Pausa dopo l'impulso (ms)
};

// Struttura per una melodia completa
struct BellMelody {
  char name[32];              // Nome melodia
  BellNote notes[MAX_MELODY_STEPS];  // Sequenza di note
  uint8_t noteCount;          // Numero di note nella sequenza
  bool isActive;              // Se la melodia è attiva
};

// Struttura per programmazione settimanale
struct WeeklySchedule {
  uint8_t id;                 // ID univoco
  char name[32];              // Nome programmazione
  DayOfWeek dayOfWeek;        // Giorno della settimana
  uint8_t hour;               // Ora (0-23)
  uint8_t minute;             // Minuto (0-59)
  uint8_t melodyIndex;        // Indice melodia da suonare
  bool isActive;              // Se attiva
};

// Struttura per eventi speciali
struct SpecialEvent {
  uint8_t id;                 // ID univoco
  char name[32];              // Nome evento
  EventType type;             // Tipo evento
  uint16_t year;              // Anno
  uint8_t month;              // Mese (1-12)
  uint8_t day;                // Giorno (1-31)
  uint8_t hour;               // Ora (0-23)
  uint8_t minute;             // Minuto (0-59)
  uint8_t melodyIndex;        // Indice melodia da suonare
  bool isActive;              // Se attivo
  bool isRecurring;           // Se si ripete ogni anno
};

// Struttura per stato sistema
struct SystemStatus {
  bool wifiConnected;
  bool rtcConnected;
  bool ntpSynced;
  uint32_t lastBellTime;
  uint8_t activeMelody;
  bool bellsEnabled;
  bool testMode;
  uint16_t totalBellRings;
  
  // Monitoraggio temperatura
  float esp32Temperature;
  bool temperatureWarning;
  bool thermalProtection;
};

// ========== VARIABILI GLOBALI (dichiarazioni) ==========
extern BellMelody melodies[10];           // Max 10 melodie
extern WeeklySchedule weeklySchedules[MAX_WEEKLY_SCHEDULES];
extern SpecialEvent specialEvents[MAX_SPECIAL_EVENTS];
extern SystemStatus systemStatus;

// ========== FUNZIONI UTILITY ==========
String dayOfWeekToString(DayOfWeek day);
String eventTypeToString(EventType type);
bool isValidTime(uint8_t hour, uint8_t minute);
bool isValidDate(uint16_t year, uint8_t month, uint8_t day);

#endif
