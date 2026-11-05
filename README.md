# Controller Campane Chiesa — ESP32

Sistema completo per il controllo automatico delle campane di una chiesa, basato su ESP32 T-Display TS0636G. Supporta programmazione settimanale, eventi speciali, controllo remoto via bot Telegram, interfaccia Web, RTC DS3231 e due relè.

**Versione firmware: v2.3.10-unified-api**

---

## Caratteristiche principali

- Web UI moderna (SPIFFS) con schede: Stato, Melodie, Programmazione, Test Relè, Impostazioni
- Bot Telegram e API HTTP unificati su layer comune `CommandHandler` (stessa logica business)
- Display TFT integrato (T-Display 135×240) con ora, data e stato sistema
- Due melodie predefinite:
  - **FUNERALE** (slot 0): 30 colpi (campana 1×3 + 2×3), 300ms suono + 2700ms pausa
  - **CHIAMATA MESSA** (slot 1): scampanio fitto alternato 1–2, 300ms + 400ms, ~100 colpi (~40s)
- Editor melodie personalizzate (fino a 120 passi)
- Programmazione settimanale e per eventi speciali (matrimoni, fest, funerali…)
- Backup/ripristino configurazione in JSON
- Pulsanti fisici esterni: FUNERALE (GPIO27) e CHIAMATA MESSA (GPIO32)
- Protezione termica con stop automatico sopra 85°C
- WiFi keepalive non-bloccante con backoff esponenziale e recovery AP
- NTP re-sync automatico ogni ora, RTC DS3231 come fallback
- Factory reset via BOOT button (tieni premuto 10s)

---

## Hardware

| Componente | Pin/Note |
|---|---|
| ESP32 T-Display TS0636G | — |
| Display TFT ST7789V 135×240 | Gestito da TFT_eSPI (pinout in `platformio.ini`) |
| RTC DS3231 | SDA=GPIO21, SCL=GPIO22 (I2C) |
| Relè Campana 1 | GPIO25 — active-low (LOW=ON) |
| Relè Campana 2 | GPIO26 — active-low (LOW=ON) |
| LED Stato | GPIO2 (LED integrato ESP32) |
| LED Feedback Melodie | GPIO33 (LED esterno, opzionale) |
| BOOT Button / Config | GPIO0 |
| Pulsante Funerale | GPIO27 — collegare a GND |
| Pulsante Messa | GPIO32 — collegare a GND |

> **Nota pin GPIO33 per LED**: nella vecchia versione era GPIO23, che coincideva con il pin TFT_RST del display. Il pin è stato corretto a GPIO33.

---

## Installazione

### 1. Configurare le credenziali

Prima di compilare, modificare `src/config.cpp`:

```cpp
const char* WIFI_SSID     = "LA_TUA_RETE_WIFI";
const char* WIFI_PASSWORD = "LA_TUA_PASSWORD_WIFI";

// Token ottenuto da @BotFather su Telegram
const char* TELEGRAM_BOT_TOKEN = "IL_TUO_TOKEN_BOT_TELEGRAM";
```

> **IMPORTANTE**: Non committare mai credenziali reali su repository pubblici. Aggiungere `src/config.cpp` al `.gitignore` se necessario.

### 2. Aggiungere utenti Telegram autorizzati

In `src/config.cpp`:

```cpp
const char* TELEGRAM_AUTHORIZED_USERS[] = {
    "123456789",   // Il tuo Chat ID (usa /start sul bot per ottenerlo)
};
const uint8_t TELEGRAM_AUTHORIZED_COUNT = 1;
```

### 3. Build e upload

In VS Code (PlatformIO):

| Task | Azione |
|---|---|
| Build firmware | `PlatformIO Build` |
| Upload firmware | `PlatformIO Upload` |
| Upload Web UI (SPIFFS) | `PlatformIO Upload FS` |
| Monitor seriale | `PlatformIO Monitor` (115200 baud) |

CLI equivalente:
```powershell
pio run -e esp32dev
pio run -e esp32dev --target upload
pio run -e esp32dev --target uploadfs
```

### 4. Primo avvio

1. Collega l'ESP32 e carica il firmware + SPIFFS.
2. Se le credenziali WiFi non sono configurate (o la connessione fallisce per 5 tentativi), il dispositivo avvia un Access Point di recovery:
   - SSID: `ChurchBells-Config`
   - Password: `campanile123`
   - Apri `http://192.168.4.1` → scheda Impostazioni → configura WiFi.
3. Al riavvio, l'IP locale appare sul display TFT e in seriale.

---

## Configurazione (file principali)

### `src/config.cpp` — Credenziali (NON committare!)
- WiFi SSID e password
- Token bot Telegram
- Lista Chat ID autorizzati

### `src/include/config.h` — Costanti sistema

| Costante | Default | Descrizione |
|---|---|---|
| `WEB_SERVER_PORT` | 80 | Porta Web UI |
| `ADMIN_USER` | `admin` | Username Basic Auth Web UI |
| `ADMIN_PASSWORD` | `LA_TUA_PASSWORD_ADMIN` | Password Basic Auth Web UI |
| `MAX_MELODY_STEPS` | 120 | Max passi per melodia |
| `BELL_MIN_PULSE` | 100ms | Impulso minimo relè |
| `BELL_MAX_PULSE` | 2000ms | Impulso massimo relè |
| `TEMP_WARNING_THRESHOLD` | 70°C | Soglia avviso temperatura |
| `TEMP_SHUTDOWN_THRESHOLD` | 85°C | Soglia stop emergenza temperatura |

---

## Bot Telegram — Comandi

| Comando | Descrizione |
|---|---|
| `/start` | Benvenuto + info autorizzazione |
| `/help` | Lista comandi |
| `/status` | Stato sistema (campane, WiFi, temperatura, uptime) |
| `/enable` / `/attiva` | Attiva campane |
| `/disable` / `/disattiva` | Disattiva campane |
| `/ring <1\|2> <ms>` | Suona campana N per X millisecondi |
| `/funeral` / `/funerale` | Avvia scampanio funerale (slot 0) |
| `/mass` / `/messa` | Avvia chiamata messa (slot 1) |
| `/melody <0-9>` | Avvia melodia per indice |
| `/schedule` / `/orari` | Prossimi eventi programmati |
| `/time` | Mostra ora/data corrente dispositivo |
| `/ntp` | Forza una resincronizzazione NTP |
| `/backup` | Mostra stato e istruzioni backup |
| `/restore` | Mostra istruzioni ripristino backup |
| `/info` | Info firmware, heap, CPU |
| `/auth` | Richiedi autorizzazione (mostra il tuo Chat ID) |

Per aggiungere un utente: dalla Web UI → Impostazioni → Telegram, oppure aggiungere il Chat ID in `config.cpp` e ricompilare.

---

## Web UI

Aprire `http://<IP-dispositivo>`. Autenticazione Basic richiesta (username/password da `config.h`).

- **Stato**: ora/data, WiFi, RTC, NTP, campane ON/OFF, test mode, temperatura, uptime
- **Melodie**: lista, riproduzione/stop, editor note (bellNumber, duration, delay)
- **Programmazione**: settimanale e eventi speciali con "programmazione semplificata"
- **Test Relè**: attivazione manuale singoli relè con durata
- **Impostazioni**: WiFi, rete IP statico/DHCP, orario manuale, backup/ripristino JSON, Telegram

---

## API REST principali

Le API sotto usano lo stesso livello comandi del bot Telegram (`CommandHandler`) per garantire coerenza dei comportamenti.

| Endpoint | Metodo | Descrizione |
|---|---|---|
| `/api/status` | GET | Stato completo sistema |
| `/api/time` | GET | Ora/data corrente |
| `/api/melodies` | GET | Lista melodie |
| `/api/melody?index=N` | GET | Dettaglio melodia N |
| `/api/test-melody` | POST | Avvia melodia `{"melodyId": N}` |
| `/api/stop-melody` | GET/POST | Stop melodia in corso |
| `/api/save-melody` | POST | Salva nuova melodia |
| `/api/update-melody` | POST | Aggiorna melodia esistente |
| `/api/delete-melody` | POST | Elimina melodia |
| `/api/weekly-schedules` | GET/POST | Programmazioni settimanali |
| `/api/special-events` | GET/POST | Eventi speciali |
| `/api/toggle-bells` | GET/POST | Abilita/disabilita campane |
| `/api/test-relay` | POST | Test relè `?relay=1&duration=500` |
| `/api/emergency-stop` | POST | Stop emergenza |
| `/api/backup` | GET | Download backup JSON |
| `/api/restore` | POST | Ripristino da JSON |
| `/api/configure-wifi` | POST | Salva WiFi e riavvia |
| `/api/set-time` | POST | Imposta ora manuale |

---

## Sicurezza

- **Credenziali WiFi e token Telegram** non devono mai essere committati su repository pubblici. Modificare sempre `src/config.cpp` prima del deploy.
- **Web UI**: protetta da Basic Auth. Le credenziali di default (`admin`/`LA_TUA_PASSWORD_ADMIN`) devono essere cambiate prima del deploy in `src/include/config.h`.
- **Bot Telegram**: solo gli utenti con Chat ID esplicitamente autorizzati possono usare i comandi. I nuovi utenti devono usare `/auth` e aspettare approvazione manuale.
- **TLS Telegram**: la connessione a `api.telegram.org` usa HTTPS. La verifica del certificato è disabilitata per default (`setInsecure()`). Per abilitarla su una rete non fidata, aggiungere il certificato root DigiCert Global Root G2:
  ```bash
  # Ottieni il certificato
  openssl s_client -connect api.telegram.org:443 -showcerts
  ```
  Poi in `telegram_bot.cpp` sostituire `setInsecure()` con `_client.setCACert(TELEGRAM_ROOT_CA_PEM)`.
- **Rete locale**: se il dispositivo deve essere raggiungibile da Internet, proteggere con VPN o reverse proxy HTTPS. Non esporre mai la Web UI direttamente su porte pubbliche.

---

## Diagnostica

Comandi disponibili via Monitor Seriale (115200 baud):

| Comando | Descrizione |
|---|---|
| `wifi_diag` | Stato WiFi dettagliato (IP, RSSI, gateway, DNS) |
| `ntp_diag` | Stato NTP/RTC (drift, ultimo sync) |
| `full_diag` | Diagnostica completa sistema |
| `temp` | Temperatura ESP32 corrente |
| `schedules` | Lista programmazioni caricate |

---

## Risoluzione problemi

| Problema | Soluzione |
|---|---|
| UI non raggiungibile | Controlla IP su display, prova `/api/status`, verifica rete |
| RTC non rilevato | Controlla cablaggio SDA/SCL, scansiona I2C con `/api/i2c-scan` (atteso 0x68) |
| Campane non suonano via Telegram | Esegui `/enable` + `/status` per verificare lo stato |
| Pulsante funerale/messa non risponde | Verifica collegamento GPIO-GND, controlla log `[FUNERAL]`/`[MASS]` in seriale |
| Display TFT non funziona | Verifica pinout in `platformio.ini`, controlla TFT_RST=23 non in conflitto con altri GPIO |
| WDT reset | Evita payload enormi senza rete stabile; il watchdog si auto-alimenta ogni 100ms |
| Restore fallisce | Verifica validità JSON; il sistema traccia contatori `imported.*` nella risposta |
| Bot Telegram non risponde | Verifica token in `config.cpp`, controlla log `[Telegram]` in seriale |
| Ore legale/solare errata | L'ora è gestita automaticamente via POSIX TZ `CET-1CEST,M3.5.0/2,M10.5.0/3` |

### Factory Reset

Tenere premuto il pulsante BOOT (GPIO0) per 10 secondi. Cancella tutta la configurazione SPIFFS (WiFi, melodie, schedules). Il firmware rimane intatto.

---

## Changelog

| Versione | Novità |
|---|---|
| v2.3.10 | Unificazione Telegram + HTTP tramite `CommandHandler`, nuove azioni Telegram `/time` `/ntp` `/backup` `/restore` |
| v2.3.9 | Bot Telegram con autenticazione whitelist, comandi remoti campane |
| v2.3.8 | Sistema recovery multilivello: DHCP fallback, AP mode dopo 5 fail WiFi |
| v2.3.7 | Protezione picchi tensione, soft-start relè, LED feedback melodie (GPIO33) |
| v2.3.6 | Refactoring schedules unificati (schedules.json) |
| v2.3.5 | WatchdogManager, WiFiReconnectManager, gestione errori strutturata |
| v2.3.2 | Performance Web UI, ottimizzazioni memoria |
| v2.3.1 | Anti-flicker UI, throttling API, CSS anti-FOUC |
| v2.3.0 | WiFi keepalive 30s, NTP re-sync orario, fix melodie interrotte |

