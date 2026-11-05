# 🚀 Deployment Guide v2.3

## Pre-Deployment Checklist

### 1. Backup Sistema Attuale ✅
```bash
# Backup configurazione corrente
> backup
# Salva JSON scaricato come: backup_pre_v2.3_YYYY-MM-DD.json

# Backup firmware attuale (opzionale)
esptool.py --port COM3 read_flash 0 0x400000 firmware_backup_v2.2.bin
```

### 2. Verifica Ambiente
- [ ] PlatformIO installato e funzionante
- [ ] Porta seriale ESP32 identificata
- [ ] WiFi credentials configurate in codice
- [ ] Backup configurazione salvato

---

## Deployment Steps

### Step 1: Compila Nuovo Firmware
```bash
# Compila progetto
pio run

# Verifica output:
# ✓ SUCCESS: Linking .pio\build\esp32dev\firmware.elf
# RAM:   [==        ]  21.2% (used 69464 bytes from 327680 bytes)
# Flash: [======    ]  61.5% (used 1286637 bytes from 2093056 bytes)
```

**Esito atteso**: ✅ Compilazione senza errori

---

### Step 2: Upload Firmware
```bash
# Carica firmware via USB
pio run --target upload
```

**Monitoraggio**:
```
Connecting.....
Writing at 0x00010000... (10 %)
...
Writing at 0x00130000... (100 %)
Wrote 1286637 bytes in XX.XX seconds
Leaving...
Hard resetting via RTS pin...
```

**Esito atteso**: ✅ Upload completato, ESP32 riavviato

---

### Step 3: Verifica Avvio
```bash
# Apri monitor seriale
pio device monitor

# Verifica log avvio:
```

**Log attesi**:
```
=== Avvio Campane Chiesa ===
SPIFFS mount: OK
Display init: OK
I2C init: OK
RTC: Connesso ✓
BellController init: OK
WiFi connecting...
✓ CONNESSIONE WiFi RIUSCITA!
✓ Indirizzo IP: 192.168.1.XXX
✓ SNTP sincronizzato
Web server avviato
```

**🔍 Verifica critica**:
- [x] WiFi connesso
- [x] IP assegnato
- [x] SNTP sincronizzato
- [x] Web server avviato

---

### Step 4: Test Diagnostici

#### 4.1 Status Generale
```bash
> status
```

**Verifica**:
```
=== STATO SISTEMA ===
WiFi: Connesso
RTC: Connesso
NTP: Sincronizzato
Campane: Abilitate
Test Mode: Disattivo
Melodia in riproduzione: No
Suonate totali: X
Programmazioni settimanali: X
Eventi speciali: X
Temperatura ESP32: XX.X°C [OK]
====================
```

#### 4.2 Diagnostica WiFi
```bash
> wifi_diag
```

**Verifica**:
```
=== 📡 DIAGNOSTICA WIFI ===
Stato connessione: CONNESSA ✓
SSID: YourNetwork
IP Address: 192.168.1.XXX ✓
Signal Strength (RSSI): -XX dBm (> -70 OK)
===========================
```

#### 4.3 Diagnostica NTP
```bash
> ntp_diag
```

**Verifica**:
```
=== 🕐 DIAGNOSTICA NTP/ORARIO ===
NTP Sincronizzato: SÌ ✓
RTC Connesso: SÌ ✓
System time (NTP): 2025-11-09 HH:MM:SS ✓
Drift NTP-RTC: X secondi ✓ OK
=================================
```

---

### Step 5: Test Funzionalità

#### 5.1 Test Relè
```bash
> test_relay_1
> test_relay_2
```

**Esito atteso**: 
- ✅ Relè 1/2 attivato per 500ms
- ✅ Click udibile
- ✅ Log: "Test relè X per 500ms..."

#### 5.2 Test Melodia
```bash
> play_melody_0
# Attendi completamento
> status
```

**Esito atteso**:
- ✅ Melodia eseguita completamente
- ✅ Nessun reset watchdog
- ✅ "Melodia in riproduzione: No" dopo fine

#### 5.3 Test Web UI
```
1. Apri http://<IP-ESP32>
2. Login: admin / LA_TUA_PASSWORD_ADMIN (o custom)
3. Verifica schede funzionanti:
   - Stato: dati aggiornati
   - Melodie: lista carica
   - Programmazione: liste caricate
   - Test Relè: funzionano
```

**Esito atteso**: ✅ Tutte le schede operative

---

### Step 6: Ripristina Configurazione (Opzionale)

Se hai backup da ripristinare:

```bash
# Via Web UI
1. Apri Impostazioni
2. Sezione "Backup/Ripristino"
3. Scegli file: backup_pre_v2.3_YYYY-MM-DD.json
4. Click "Ripristina Configurazione"
5. Attendi completamento

# Verifica
> list_melodies
> list_schedules
```

**Esito atteso**: 
- ✅ Melodie ripristinate
- ✅ Programmazioni ripristinate

---

## Post-Deployment Monitoring

### Monitoraggio Primi 30 Minuti (Critico)

**Ogni 5 minuti**:
```bash
> status
> temp
```

**Verifica**:
- [ ] WiFi: Connesso
- [ ] Temperatura: < 70°C
- [ ] Nessun reset inaspettato
- [ ] Log puliti (no errori)

---

### Monitoraggio Prime 24 Ore

**Ogni 2 ore**:
```bash
> full_diag
```

**Verifica**:
- [ ] WiFi: sempre connesso
- [ ] NTP: drift < 10 secondi
- [ ] Uptime: crescente (no reset)
- [ ] Free Heap: > 50KB

**Log da cercare**:
```bash
# OK - Riconnessione gestita
[WiFi] ⚠️ Connessione persa! Tentativo di riconnessione...
[WiFi] ✓ Riconnessione riuscita!

# OK - NTP sync periodico
[NTP] 🔄 Re-sync orario periodico...
[NTP] ✓ Re-sync completato

# ❌ NON DOVREBBE MAI APPARIRE
E (XXXXX) task_wdt: Task watchdog got triggered
```

---

### Monitoraggio Primi 7 Giorni

**Check giornaliero**:
```bash
> status
> wifi_diag
> ntp_diag
```

**Metriche target**:
| Metrica | Target | Verifica |
|---------|--------|----------|
| WiFi Uptime | > 99% | `wifi_diag` |
| NTP Drift | < 30s/giorno | `ntp_diag` |
| System Uptime | 7 giorni | `status` |
| Melodie Complete | 100% | Log seriale |
| Reset Inaspettati | 0 | Uptime |

---

## Rollback Procedure

Se riscontri problemi critici:

### Rollback via Git
```bash
# Se hai versionato con git
git checkout HEAD~1 src/main.cpp src/bell_controller.cpp
pio run --target upload
```

### Rollback Manuale
```bash
# Se hai backup firmware
esptool.py --port COM3 write_flash 0x10000 firmware_backup_v2.2.bin
```

### Rollback Configurazione
```bash
# Ripristina backup via Web UI
1. Impostazioni → Backup/Ripristino
2. Carica: backup_pre_v2.3_YYYY-MM-DD.json
```

---

## Troubleshooting Deployment

### Problema: Compilazione fallisce

**Errore**: `fatal error: esp_task_wdt.h: No such file`

**Soluzione**:
```bash
# Pulisci cache e ricompila
pio run --target clean
pio run
```

---

### Problema: Upload fallisce

**Errore**: `A fatal error occurred: Failed to connect`

**Soluzione**:
1. Premi BOOT button durante upload
2. Verifica porta COM corretta
3. Chiudi altri Serial Monitor aperti

---

### Problema: WiFi non si connette dopo upload

**Sintomi**: AP mode attivo invece di STA

**Soluzione**:
```bash
# Verifica credenziali WiFi
1. Monitor seriale: controlla SSID provato
2. Se sbagliato: riconnetti tramite AP mode
   - http://192.168.4.1
   - Impostazioni → WiFi
```

---

### Problema: Watchdog reset all'avvio

**Sintomi**: Continui reboot

**Soluzione**:
```bash
# Verifica log seriale completo
pio device monitor

# Cerca errore specifico
# Possibili cause:
# - SPIFFS corrotto: uploadfs nuovamente
# - JSON corrotto: cancella e ricrea
```

---

## Success Criteria

### Deployment Riuscito ✅
- [x] Firmware uploadato senza errori
- [x] ESP32 avviato e connesso WiFi
- [x] Web UI accessibile e funzionante
- [x] Tutti i test diagnostici OK
- [x] Nessun reset nei primi 30 minuti
- [x] Temperature < 70°C
- [x] Log puliti (no errori critici)

### Sistema Stabile (dopo 7 giorni) ✅
- [x] Uptime > 7 giorni senza reset
- [x] WiFi 99%+ uptime
- [x] NTP drift < 30 secondi
- [x] Tutte melodie completate senza interruzioni
- [x] Programmazioni eseguite correttamente
- [x] Zero watchdog timeouts

---

## Support & Diagnostics

### Log Files
I log sono accessibili via:
- **Serial Monitor**: Output in tempo reale
- **Web UI /api/status**: Stato JSON
- **Comandi diagnostici**: `full_diag`, `wifi_diag`, `ntp_diag`

### Report Bug
Se riscontri problemi, raccogli:
```bash
1. full_diag output
2. status output
3. Log seriale ultimi 5 minuti
4. Screenshot errori Web UI
5. Descrizione comportamento atteso vs effettivo
```

---

## Versioning

### Current Version
- **Firmware**: v2.3
- **Deploy Date**: 2025-11-09
- **Changes**: WiFi keepalive, NTP re-sync, Watchdog fix

### Version History
- **v2.2**: Sistema base funzionante
- **v2.3**: Fix stabilità long-running

### Next Version (Planned)
- v2.4: Logger system integration in main.cpp
- v2.5: Advanced error handling throughout

---

**Deployment Owner**: [Nome]  
**Deploy Date**: ___________  
**Deploy Status**: [ ] Success [ ] Rollback [ ] Partial  
**Notes**: _________________

---

✅ **Sistema pronto per produzione dopo 7 giorni di test stabilità!**
