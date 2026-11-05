#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ========== CONFIGURAZIONE HARDWARE ==========
// Organizzata con namespace per migliorare leggibilità e manutenibilità

namespace Hardware {
    // Pin per RTC DS3231 (I2C)
    constexpr uint8_t RTC_SDA_PIN = 21;
    constexpr uint8_t RTC_SCL_PIN = 22;
    
    // Pin per Relè Campane
    constexpr uint8_t RELAY1_PIN = 25;  // Campana 1 (piccola/segnale)
    constexpr uint8_t RELAY2_PIN = 26;  // Campana 2 (grande/principale)

    // Polarita' comando relè.
    // false = modulo SSR comandato direttamente (attivo LOW): con l'ESP spento o in avvio
    //         gli ingressi vanno a massa e i relè si ECCITANO -> non sicuro con i martelli.
    // true  = transistor NPN tra ESP e ingresso modulo (attivo HIGH): ESP spento = relè spenti.
    // Cambiare SOLO insieme al cablaggio, altrimenti i relè restano sempre accesi.
    constexpr bool RELAY_ACTIVE_HIGH = false;
    constexpr uint8_t RELAY_ON = RELAY_ACTIVE_HIGH ? HIGH : LOW;
    constexpr uint8_t RELAY_OFF = RELAY_ACTIVE_HIGH ? LOW : HIGH;
    
    // Pin per LED di stato
    constexpr uint8_t STATUS_LED_PIN = 2;         // LED integrato ESP32

    // Relè generale: contatti in serie all'alimentazione dei martelli.
    // Chiuso solo mentre si suona: se un SSR o un teleruttore resta incollato, i martelli restano senza corrente.
    constexpr uint8_t MASTER_RELAY_PIN = 33;
    constexpr bool MASTER_RELAY_ACTIVE_HIGH = false;  // modulo relè con ingresso attivo LOW
    // Open-drain: OFF = pin scollegato (il modulo a 5V non si spegne con i 3,3V di un HIGH)
    constexpr bool MASTER_RELAY_OPEN_DRAIN = false;
    constexpr uint8_t MASTER_PIN_MODE = MASTER_RELAY_OPEN_DRAIN ? OUTPUT_OPEN_DRAIN : OUTPUT;
    constexpr uint8_t MASTER_ON = MASTER_RELAY_ACTIVE_HIGH ? HIGH : LOW;
    constexpr uint8_t MASTER_OFF = MASTER_RELAY_ACTIVE_HIGH ? LOW : HIGH;
    constexpr uint16_t MASTER_ON_DELAY_MS = 50;       // attesa chiusura contatti prima del primo colpo
    constexpr uint16_t MASTER_OFF_DELAY_MS = 1500;    // apertura dopo l'ultimo colpo (contatti senza carico)
    
    // Pulsanti
    constexpr uint8_t CONFIG_BUTTON_PIN = 0;    // Boot button
    constexpr uint8_t FUNERAL_BUTTON_PIN = 27;  // Pulsante funerale
    constexpr uint8_t MASS_BUTTON_PIN = 32;     // Pulsante chiamata messa
    constexpr uint8_t RESET_BUTTON_PIN = 13;    // Pulsante riavvio manuale

    // Polarita' pulsanti: true = active-low (premuto quando il pin va LOW)
    // Imposta a false se il tuo cablaggio e' active-high.
    constexpr bool CONFIG_BUTTON_ACTIVE_LOW = true;
    constexpr bool FUNERAL_BUTTON_ACTIVE_LOW = true;
    constexpr bool MASS_BUTTON_ACTIVE_LOW = true;
    constexpr bool RESET_BUTTON_ACTIVE_LOW = true;
}

// Mantieni define per compatibilità con codice esistente
#define RTC_SDA_PIN Hardware::RTC_SDA_PIN
#define RTC_SCL_PIN Hardware::RTC_SCL_PIN
#define RELAY1_PIN Hardware::RELAY1_PIN
#define RELAY2_PIN Hardware::RELAY2_PIN
#define RELAY_ON Hardware::RELAY_ON
#define RELAY_OFF Hardware::RELAY_OFF
#define STATUS_LED_PIN Hardware::STATUS_LED_PIN
#define MASTER_RELAY_PIN Hardware::MASTER_RELAY_PIN
#define CONFIG_BUTTON_PIN Hardware::CONFIG_BUTTON_PIN
#define FUNERAL_BUTTON_PIN Hardware::FUNERAL_BUTTON_PIN
#define MASS_BUTTON_PIN Hardware::MASS_BUTTON_PIN
#define RESET_BUTTON_PIN Hardware::RESET_BUTTON_PIN

// ========== CONFIGURAZIONE RETE ==========
// WiFi Credentials (da modificare)
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;

// Web Server
#define WEB_SERVER_PORT 80
#define ADMIN_PASSWORD "LA_TUA_PASSWORD_ADMIN"
// Utente per autenticazione Basic (UI Web). Cambia se necessario.
#ifndef ADMIN_USER
#define ADMIN_USER "admin"
#endif

// ========== TELEGRAM BOT CONFIGURATION ==========
// Token del bot Telegram (ottenuto da @BotFather)
extern const char* TELEGRAM_BOT_TOKEN;

// Chat IDs autorizzati (max 10 utenti)
// Lasciare vuoto per configurazione runtime via web UI
extern const char* TELEGRAM_AUTHORIZED_USERS[];
extern const uint8_t TELEGRAM_AUTHORIZED_COUNT;

// Abilita/disabilita bot all'avvio
#define TELEGRAM_BOT_ENABLED true

// Abilita notifiche automatiche
#define TELEGRAM_NOTIFICATIONS_ENABLED true

namespace Config {
    // Debug e logging (ridotti per evitare surriscaldamento)
    constexpr bool DEBUG_SCHEDULING = false;
    constexpr bool DEBUG_MELODY_PLAYBACK = false;
    constexpr uint32_t SERIAL_DEBUG_INTERVAL = 60000;      // 60s
    constexpr uint32_t SCHEDULE_DEBUG_INTERVAL = 300000;   // 5 minuti
    
    // Comportamento all'avvio
    constexpr bool AUTO_ENABLE_BELLS_ON_STARTUP = true;
    constexpr bool STARTUP_TEST_BELL = false;
    
    // Versione firmware
    constexpr char FIRMWARE_VERSION[] = "v2.3.9-telegram";
    
    // ===== NETWORK ADVANCED =====
    namespace Network {
        constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;
        constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 30000;
        constexpr uint32_t WIFI_CHECK_INTERVAL_MS = 30000;
        constexpr uint32_t NTP_RESYNC_INTERVAL_MS = 3600000; // 1 ora
    }
    
    // ===== LIMITS =====
    namespace Limits {
        constexpr uint8_t MAX_MELODIES = 10;
        constexpr uint8_t MAX_NOTES_PER_MELODY = 100;
        constexpr uint16_t MIN_NOTE_DURATION_MS = 50;
        constexpr uint16_t MAX_NOTE_DURATION_MS = 5000;
        constexpr uint16_t MAX_NOTE_DELAY_MS = 10000;
    }
    
    // ===== WATCHDOG =====
    namespace Watchdog {
        constexpr uint32_t FEED_INTERVAL_MS = 100;
        constexpr uint32_t TIMEOUT_SECONDS = 300; // 5 minuti
    }
    
    // ===== FILESYSTEM =====
    namespace FileSystem {
        constexpr char MELODIES_FILE[] = "/melodies.json";
        constexpr char SCHEDULES_FILE[] = "/schedules.json";
        constexpr char EVENTS_FILE[] = "/events.json";
        constexpr char WIFI_CONFIG_FILE[] = "/wifi.json";
        constexpr char NETWORK_FILE[] = "/network.json";  // Configurazione IP statico
        constexpr char TELEGRAM_USERS_FILE[] = "/telegram_users.json";  // Chat ID autorizzati
      constexpr char MAINTENANCE_FILE[] = "/maintenance.json";  // Stato auto-report/riavvio
    }
}

// Mantieni define per compatibilità
#define DEBUG_SCHEDULING Config::DEBUG_SCHEDULING
#define DEBUG_MELODY_PLAYBACK Config::DEBUG_MELODY_PLAYBACK
#define AUTO_ENABLE_BELLS_ON_STARTUP Config::AUTO_ENABLE_BELLS_ON_STARTUP
#define STARTUP_TEST_BELL Config::STARTUP_TEST_BELL

// ========== CONFIGURAZIONE SISTEMA ==========
// Colori per display
#define ORANGE 0xFD20   // Arancione per indicatori

namespace Timing {
    // Intervalli di aggiornamento
    constexpr uint32_t DISPLAY_UPDATE_INTERVAL = 1000;    // 1 secondo
    constexpr uint32_t SCHEDULE_CHECK_INTERVAL = 30000;   // 30 secondi
    constexpr uint32_t TEMP_CHECK_INTERVAL = 30000;       // 30 secondi
    
    // Limiti campane
    constexpr uint16_t BELL_MIN_PULSE = 100;              // 100ms minimo
    constexpr uint16_t BELL_MAX_PULSE = 2000;             // 2 secondi massimo
    constexpr uint16_t BELL_MIN_DELAY = 50;               // 50ms minimo
    constexpr uint16_t BELL_MAX_DELAY = 5000;             // 5 secondi massimo
    
    // Protezione picchi di tensione (v2.3.7)
    constexpr uint16_t POWER_STABILIZATION_MS = 2000;     // Tempo stabilizzazione post-boot
    constexpr uint16_t MIN_RELAY_INTERVAL_MS = 100;       // Intervallo minimo tra attivazioni relè
    constexpr uint16_t SOFT_START_PULSE1_US = 2000;       // Impulso 1 soft-start (2ms)
    constexpr uint16_t SOFT_START_PULSE2_MS = 5;          // Impulso 2 soft-start (5ms)
    constexpr uint16_t SOFT_START_PULSE3_MS = 10;         // Impulso 3 soft-start (10ms)
    
    // Feedback visivo (v2.3.7)
    constexpr uint16_t LED_BLINK_INTERVAL_MS = 300;       // Intervallo lampeggio LED durante melodia
}

// Mantieni define per compatibilità
#define DISPLAY_UPDATE_INTERVAL Timing::DISPLAY_UPDATE_INTERVAL
#define SCHEDULE_CHECK_INTERVAL Timing::SCHEDULE_CHECK_INTERVAL
#define BELL_MIN_PULSE Timing::BELL_MIN_PULSE
#define BELL_MAX_PULSE Timing::BELL_MAX_PULSE
#define BELL_MIN_DELAY Timing::BELL_MIN_DELAY
#define BELL_MAX_DELAY Timing::BELL_MAX_DELAY
#define TEMP_CHECK_INTERVAL Timing::TEMP_CHECK_INTERVAL

// Programmazione
#define MAX_WEEKLY_SCHEDULES 64         // Max programmazioni settimanali (aumentato da 20)
#define MAX_SPECIAL_EVENTS 10           // Max eventi speciali
#define MAX_MELODY_STEPS 120            // Max passi per melodia (aumentato per scampanio fitto)

namespace Temperature {
    // Monitoraggio temperatura ESP32
    constexpr float TEMP_WARNING_THRESHOLD = 70.0f;      // °C
    constexpr float TEMP_CRITICAL_THRESHOLD = 80.0f;     // °C
    constexpr float TEMP_SHUTDOWN_THRESHOLD = 85.0f;     // °C
}

// Mantieni define per compatibilità
#define TEMP_WARNING_THRESHOLD Temperature::TEMP_WARNING_THRESHOLD
#define TEMP_CRITICAL_THRESHOLD Temperature::TEMP_CRITICAL_THRESHOLD
#define TEMP_SHUTDOWN_THRESHOLD Temperature::TEMP_SHUTDOWN_THRESHOLD

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
