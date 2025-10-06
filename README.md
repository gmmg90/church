# Controller Campane Chiesa — ESP32

Sistema per il controllo delle campane con ESP32, interfaccia Web, RTC DS3231 e due relè. Supporta preset “FUNERALE” e “CHIAMATA MESSA”, backup/ripristino della configurazione, programmazione semplificata e pulsanti fisici per avvio rapido.

## Caratteristiche
- Web UI moderna (SPIFFS) con schede: Stato, Melodie, Programmazione, Test Relè, Impostazioni
- Due melodie predefinite:
	- FUNERALE: 30 colpi (campana 1×3 + 2×3) con 300ms suono + 2700ms pausa
	- CHIAMATA MESSA: scampanio fitto alternato 1–2, 300ms + 400ms, ~100 colpi (~40s)
- Editor melodie personalizzate (fino a 120 passi)
- Programmazione settimanale e “Programmazione Semplificata” (giorni multipli + orari multipli)
- Backup JSON in streaming, Restore robusto (gestione payload grandi, contatori import)
- Pulsanti fisici esterni: FUNERALE (GPIO27) e CHIAMATA MESSA (GPIO32)
- Protezione termica e stato temperatura ESP32
- Uptime e versione firmware in /api/status
- Autenticazione Basic per UI (username/password configurabili)

## Hardware
- ESP32 T‑Display TS0636G (ST7789V 135×240) o modalità headless/esterna (vedi Note)
- RTC DS3231 via I2C (SDA=GPIO21, SCL=GPIO22)
- Relè:
	- RELAY1_PIN=25 (Campana 1)
	- RELAY2_PIN=26 (Campana 2)
	- Active‑low (LOW=ON)
- LED stato su GPIO2
- Pulsanti:
	- CONFIG_BUTTON_PIN=0 (T‑Display sinistro, pulsante di boot)
	- FUNERAL_BUTTON_PIN=27 (pulsante esterno tra GPIO27 e GND)
	- MASS_BUTTON_PIN=32 (pulsante esterno tra GPIO32 e GND)

## Requisiti
- WiFi 2.4 GHz
- VS Code + PlatformIO (per build/aggiornamenti)
- Browser moderno

## Installazione e Primo Avvio
1. Cablaggio: collega RTC, relè e alimentazione come da pin sopra.
2. Alimenta il dispositivo. Se la rete WiFi non è configurata, parte l’Access Point di configurazione:
	 - SSID: `ChurchBells-Config`
	 - Password: `campanile123`
	 - IP AP: mostrato nel display o `192.168.4.1`
3. Apri `http://192.168.4.1` e configura SSID/Password nella scheda “Impostazioni”. Il dispositivo si riavvia e si collega alla rete. L’IP locale appare nel display e nella UI.

## Web UI
Apri `http://<IP-dispositivo>`.

- Stato: ora/data, WiFi/RTC/NTP, campane ON/OFF, test mode, STOP emergenza, temperatura ESP32, uptime e firmware.
- Melodie: elenco melodie, riproduzione/stop, editor con aggiunta note (bellNumber, duration, delay).
- Programmazione: settimanale + “semplificata” (fan‑out giorni × orari × melodia).
- Test Relè: stato e test manuali.
- Impostazioni: WiFi, orario manuale/NTP, backup/ripristino.

Autenticazione: all’accesso alla UI è richiesta Basic Auth (username/password configurati nel codice, vedi sotto).

Documentazione: guida stampabile completa in `docs/Manuale-Installazione.md`.

## Configurazione (file)
`src/include/config.h` contiene le costanti principali:
- Rete:
	- `WEB_SERVER_PORT` (default 80)
	- `ADMIN_USER` (default "admin") e `ADMIN_PASSWORD` (default "chiesa123") per la UI
- Limiti:
	- `MAX_MELODY_STEPS` (default 120)
	- `BELL_MIN_PULSE`, `BELL_MAX_PULSE`, `BELL_MIN_DELAY`, `BELL_MAX_DELAY`
- Pulsanti e pin hardware

`src/config.cpp` contiene le credenziali WiFi di default (modificarle prima del deploy):
```cpp
const char* WIFI_SSID = "...";
const char* WIFI_PASSWORD = "...";
```

## Firmware e UI: build e upload
In VS Code (PlatformIO):
- Build firmware: “PlatformIO Build”
- Upload firmware: “PlatformIO Upload”
- Monitor seriale: “PlatformIO Monitor” (115200 baud)
- Upload UI (SPIFFS): “Upload Filesystem Image”

CLI (opzionale):
```powershell
pio run -e esp32dev
pio run -e esp32dev --target upload
pio run -e esp32dev --target uploadfs
```

Versione firmware: modificare in `src/main.cpp` la costante `FIRMWARE_VERSION` (es. "2.2"). In alternativa è possibile definirla via `build_flags` nel `platformio.ini`.

## API principali
- GET `/api/status`: stato completo (wifiConnected, rtcConnected, ntpSynced, bellsEnabled, testMode, firmwareVersion, uptimeMs, bootEpoch, ipAddress, temperatura, ecc.)
- GET `/api/time`: ora/data per UI
- GET `/api/melodies`: elenco melodie attive
- GET `/api/melody?index=N`: dettagli melodia N
- POST `/api/test-melody`: avvia melodia di test (JSON: `{ "melodyId": <int> }` o con `notes`)
- POST `/api/stop-melody`: stop immediato melodia
- POST `/api/save-melody` | `/api/update-melody` | `/api/delete-melody`
- GET `/api/weekly-schedules` | POST `/api/weekly-schedules`
- GET `/api/special-events` | POST `/api/special-events`
- GET `/api/backup`: download backup JSON (streaming)
- POST `/api/restore`: ripristino (accumulo body chunk, contatori import in risposta)
- POST `/api/toggle-bells`: abilita/disabilita campane
- POST `/api/test-relay?relay=1|2&duration=ms`: test relè
- POST `/api/set-time`: imposta data/ora manuale
- POST `/api/configure-wifi`: salva SSID/password e riavvia
- POST `/api/emergency-stop`: stop di emergenza

Note: alcune route implementano rate‑limit e “Connection: close” per robustezza.

## Preset melodie
- Slot 0: “FUNERALE” — 30 note, 300ms + 2700ms
- Slot 1: “CHIAMATA MESSA” — ~100 note alternando 1–2, 300ms + 400ms (~40s)

È possibile creare/aggiornare melodie personalizzate dall’editor. Le melodie sono salvate su SPIFFS.

## Pulsante fisico “Funerale”
- `FUNERAL_BUTTON_PIN` (GPIO27): se premuto (debounce + cooldown), avvia la melodia slot 0 se non è in corso altra riproduzione.

## Pulsante fisico “Chiamata Messa”
- `MASS_BUTTON_PIN` (GPIO32): se premuto (debounce + cooldown), avvia la melodia slot 1 se non è in corso altra riproduzione.

## Sicurezza
- UI protetta con Basic Auth (username/password in `config.h`). Cambiali prima del deploy.
- Se esposto su Internet, usa un proxy HTTPS o VPN. Basic Auth invia credenziali in base64.

## Risoluzione problemi (rapido)
- UI non raggiungibile: verifica IP (display o AP), prova `/api/status`, controlla rete.
- RTC non rilevato: controlla SDA/SCL e `/api/i2c-scan` (0x68 per DS3231).
- Pulsante funerale non risponde: verifica collegamento GPIO27-GND, controlla Serial Monitor per debug "[FUNERAL] GPIO27 = X"
- Pulsante messa non risponde: verifica collegamento GPIO32-GND, controlla Serial Monitor per debug "[MASS] GPIO32 = X"
- Restore fallisce: assicurati che il JSON sia valido; il sistema accumula chunk e segnala contatori “imported.*”.
- WDT reset: le route sono cooperative (yield), ma evita test prolungati con payload enormi senza rete stabile.

## Note su display esterno o headless
Il progetto può funzionare anche senza T‑Display o con LCD I2C esterno introducendo un semplice adapter display (vedi discussione). Se ti serve, possiamo integrare la variante LCD 20x4 o modalità headless (log su Serial).

—
Firmware: la versione è esposta in UI (Stato). Le risorse statiche sono servite con no‑cache per evitare file UI obsoleti.
