# 📘 Manuale di Installazione e Uso — Controller Campane Chiesa

## 🧩 Panoramica
- Hardware: ESP32 T‑Display TS0636G, RTC DS3231, 2 relè, pulsanti esterni per FUNERALE e CHIAMATA MESSA
- Interfaccia: Web UI su rete locale (SPIFFS), API HTTP REST
- Funzioni chiave: due melodie predefinite, editor melodie, programmazione settimanale e semplificata, backup/restore, pulsanti fisici

—

## 🔌 Collegamenti hardware (cablaggio)
Usa cavi corti e GND comune tra tutti i moduli.

- Alimentazione ESP32: 5V via USB (consigliato durante setup)
- Display: integrato (ST7789 135×240)
- LED di stato: GPIO2

1) RTC DS3231 (I2C)
- SDA → GPIO21
- SCL → GPIO22
- VCC → 3.3V
- GND → GND

2) Relè (2 canali, livello attivo LOW)
- IN1 → GPIO25 (Campana 1 / piccola)
- IN2 → GPIO26 (Campana 2 / grande)
- VCC → 5V (modulo relè tipico) — verifica compatibilità del modulo
- GND → GND (comune con ESP32)
Note:
- I moduli relè a 5V generalmente accettano segnali logici 3.3V sui pin INx. In caso contrario, usare un driver a transistor/optocoupler.
- Uscite attive LOW: livello LOW = relè ON.

3) Pulsanti fisici
- Pulsante FUNERALE → tra GPIO27 e GND (INPUT_PULLUP interno)
- Pulsante CHIAMATA MESSA → tra GPIO32 e GND (INPUT_PULLUP interno)
- Pulsante CONFIG (boot) integrato nel T‑Display → GPIO0 (non usare per funzioni esterne)
Logica pulsanti con INPUT_PULLUP:
- Rilasciato: livello HIGH
- Premuto: livello LOW

Riepilogo pin principali
- RTC: SDA=21, SCL=22
- Relè: 25 (Relè 1), 26 (Relè 2)
- LED: 2
- Pulsanti: 27 (Funerale), 32 (Chiamata Messa), 0 (Config)

—

## 🛠️ Installazione firmware e UI
Prerequisiti: PC con VS Code + PlatformIO, cavo USB‑C/Micro‑USB per il T‑Display.

1) Compila e carica il firmware
- Task VS Code: PlatformIO Build → PlatformIO Upload
- Oppure da terminale PowerShell (facoltativo):
```powershell
C:\Users\pep\.platformio\penv\Scripts\platformio.exe run --target upload
```

2) Carica la Web UI (SPIFFS)
- Task VS Code: Upload Filesystem Image
- Oppure:
```powershell
C:\Users\pep\.platformio\penv\Scripts\platformio.exe run --target uploadfs
```

3) Monitor seriale (debug a 115200 baud)
- Task VS Code: PlatformIO Monitor

—

## 📶 Primo avvio e rete
Se il dispositivo non trova una rete WiFi configurata, espone un Access Point di configurazione:
- SSID AP: ChurchBells‑Config
- Password AP: campanile123
- IP dell’AP: 192.168.4.1 (mostrato anche sul display)

Apri il browser su http://192.168.4.1 e imposta SSID e Password della tua rete nella scheda “Impostazioni”. Dopo il riavvio, l’IP locale viene mostrato sul display e in alto nella UI.

—

## 🔐 Credenziali e sicurezza
- Accesso UI (Basic Auth):
  - Username: admin
  - Password: chiesa123
- Dove cambiarle:
  - File `src/include/config.h`: ADMIN_USER (se definito via build flag) e ADMIN_PASSWORD
  - File `src/config.cpp`: credenziali WiFi di default (`WIFI_SSID`, `WIFI_PASSWORD`)
- Suggerimenti:
  - Cambia subito le credenziali prima di installare in produzione
  - Se esponi su Internet, usa un proxy HTTPS o VPN

—

## 🖥️ Uso dell’interfaccia web
Apri http://<IP-dispositivo> e autentica con le credenziali.

Schede principali
1) Stato
- Ora/data, WiFi/RTC/NTP, temperatura ESP32, test mode, STOP emergenza, uptime/firmware

2) Melodie
- Elenco melodie, riproduzione/stop
- Editor note: campana (1/2), durata (ms), pausa (ms)

3) Programmazione (ordine attentamente organizzato)
- Eventi Speciali (in alto): gestione eventi datati con tipo e melodia
- Programmazione Semplificata: selezioni Giorni × Orari × Melodia e genera le schedulazioni
- Programmazione Settimanale: gestione dettagliata delle occorrenze

4) Test Relè
- Stato e attivazioni manuali temporizzate

5) Impostazioni
- WiFi, NTP/ora manuale, backup/ripristino configurazione

Pulsanti fisici
- FUNERALE (GPIO27 → GND): avvia melodia slot 0
- CHIAMATA MESSA (GPIO32 → GND): avvia melodia slot 1
- Anti‑rimbalzo, cooldown e blocco se è già in riproduzione

—

## 🔗 API HTTP (REST)
Prefisso: http://<IP-dispositivo>

- Stato
  - GET `/api/status`
  - Esempio PowerShell:
    ```powershell
    curl http://<IP>/api/status
    ```

- Ora e melodie
  - GET `/api/time`
  - GET `/api/melodies`
  - GET `/api/melody?index=0`

- Riproduzione
  - POST `/api/test-melody`  Body JSON: `{ "melodyId": 0 }`
    ```powershell
    curl -X POST http://<IP>/api/test-melody -H "Content-Type: application/json" -d '{"melodyId":0}'
    ```
  - POST `/api/stop-melody`

- Programmazione
  - GET/POST `/api/weekly-schedules`
  - GET/POST `/api/special-events`

- Relè
  - GET `/api/relay-status`
  - POST `/api/test-relay?relay=1&duration=500`

- Sistema
  - POST `/api/toggle-bells` (abilita/disabilita campane)
  - POST `/api/set-time` (imposta orario manuale)
  - POST `/api/configure-wifi` (salva WiFi e riavvia)
  - GET `/api/backup` (download JSON)
  - POST `/api/restore` (ripristino da JSON)

- Diagnostica
  - GET `/api/i2c-scan` (DS3231 atteso a 0x68)

—

## 🧪 Test rapidi
- Cortocircuita GPIO27↔GND → parte “FUNERALE” (se libero)
- Cortocircuita GPIO32↔GND → parte “CHIAMATA MESSA” (se libera)
- In “Test Relè” verifica l’attivazione a tempo dei due canali

—

## 🧯 Risoluzione problemi
- UI non raggiungibile: verifica IP (display), controlla /api/status, prova AP di configurazione
- RTC non rilevato: controlla SDA/SCL, usa /api/i2c-scan → 0x68
- Pulsanti non rispondono: controlla collegamenti su GND; Serial Monitor mostra `[FUNERAL] GPIO27 = X` e `[MASS] GPIO32 = X`
- Relè invertiti: ricorda che sono active‑LOW (LOW = ON)
- NTP assente: imposta ora manuale in Impostazioni
- Protezione termica: se temperatura alta, il sistema limita le operazioni

—

## 📝 Note e buone pratiche
- Evita l’uso di GPIO0 per pulsanti esterni (pin di boot); usalo solo per CONFIG
- Mantieni fili ordinati e ben fissati; etichetta i cavi
- Esegui sempre il backup prima di un restore o aggiornamenti

—

## 📎 Riferimenti veloci
- Pin: RTC 21/22 • Relè 25/26 • LED 2 • Pulsanti 27/32/0
- UI: admin / chiesa123
- AP: ChurchBells‑Config / campanile123
- Baud seriale: 115200

—

