#include "include/config.h"

// ========== CONFIGURAZIONI WIFI ==========
// IMPORTANTE: Modificare con le credenziali della rete WiFi della chiesa
// NON committare mai le credenziali reali su un repository pubblico!
const char* WIFI_SSID = "LA_TUA_RETE_WIFI";           // <-- MODIFICARE QUI
const char* WIFI_PASSWORD = "LA_TUA_PASSWORD_WIFI";   // <-- MODIFICARE QUI

// ========== CONFIGURAZIONI TELEGRAM ==========
// Token del bot Telegram - OBBLIGATORIO
// Ottenerlo da @BotFather su Telegram
// IMPORTANTE: Sostituire con il tuo token! Non condividerlo mai pubblicamente.
// Il token vero sta in src/include/secrets.h (escluso da git: vedi secrets.example.h)
#if __has_include("include/secrets.h")
#include "include/secrets.h"
#endif
#ifndef SECRET_TELEGRAM_BOT_TOKEN
#define SECRET_TELEGRAM_BOT_TOKEN ""
#endif
const char* TELEGRAM_BOT_TOKEN = SECRET_TELEGRAM_BOT_TOKEN;

// Lista utenti autorizzati (Chat IDs)
// Lasciare vuoto {} per configurare via web UI
// Per ottenere il tuo Chat ID: avvia il bot e usa /start
const char* TELEGRAM_AUTHORIZED_USERS[] = {
    // "123456789",     // Esempio: aggiungi qui i Chat ID autorizzati
    // "987654321",
};
const uint8_t TELEGRAM_AUTHORIZED_COUNT = 0;  // Numero di utenti nella lista sopra

// Implementazioni delle funzioni di utilità

String dayOfWeekToString(DayOfWeek day) {
  switch (day) {
    case DOMENICA: return "Domenica";
    case LUNEDI: return "Lunedì";
    case MARTEDI: return "Martedì";
    case MERCOLEDI: return "Mercoledì";
    case GIOVEDI: return "Giovedì";
    case VENERDI: return "Venerdì";
    case SABATO: return "Sabato";
    default: return "Sconosciuto";
  }
}

String eventTypeToString(EventType type) {
  switch (type) {
    case EVENTO_MESSA: return "Messa";
    case EVENTO_ANGELUS: return "Angelus";
    case EVENTO_MATRIMONIO: return "Matrimonio";
    case EVENTO_FUNERALE: return "Funerale";
    case EVENTO_FESTA: return "Festa";
    case EVENTO_PERSONALIZZATO: return "Personalizzato";
    default: return "Sconosciuto";
  }
}

bool isValidTime(uint8_t hour, uint8_t minute) {
  return (hour < 24 && minute < 60);
}

bool isValidDate(uint16_t year, uint8_t month, uint8_t day) {
  if (year < 2020 || year > 2050) return false;
  if (month < 1 || month > 12) return false;
  if (day < 1 || day > 31) return false;
  
  // Controllo giorni per mese (semplificato)
  uint8_t daysInMonth[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return day <= daysInMonth[month - 1];
}
