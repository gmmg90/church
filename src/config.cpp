#include "include/config.h"

// ========== CONFIGURAZIONI WIFI ==========
// IMPORTANTE: Modificare con le credenziali della rete WiFi della chiesa
const char* WIFI_SSID = "stayathome";           // <-- MODIFICARE QUI
const char* WIFI_PASSWORD = "qwertyuiop9876541";        // <-- MODIFICARE QUI

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
