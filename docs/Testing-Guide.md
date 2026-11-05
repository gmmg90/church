# 🧪 Guida Testing Sistema Campane Chiesa

## 📋 Indice
1. [Setup Testing](#setup-testing)
2. [Testing Hardware](#testing-hardware)
3. [Testing Funzionalità](#testing-funzionalità)
4. [Testing Stress](#testing-stress)
5. [Checklist Finale](#checklist-finale)

---

## 🔧 Setup Testing

### Prerequisiti
- [ ] ESP32 T-Display connesso via USB
- [ ] Monitor seriale aperto (115200 baud)
- [ ] Relè campane connessi (o LED per simulazione)
- [ ] RTC DS3231 connesso (opzionale)
- [ ] WiFi configurato

### Configurazione Logger
Nel `setup()` del `main.cpp`, configura il livello di logging:

```cpp
// Per testing dettagliato:
Logger::setLevel(Logger::DEBUG);

// Per uso normale:
Logger::setLevel(Logger::INFO);

// Per produzione:
Logger::setLevel(Logger::WARN);
```

---

## 🔌 Testing Hardware

### Test 1: Inizializzazione Base
**Obiettivo**: Verificare che tutti i componenti si inizializzino correttamente

**Procedura**:
1. Apri monitor seriale
2. Reset ESP32
3. Verifica output log

**Risultato Atteso**:
```
[...][INFO ][BellCtrl] Inizializzazione BellController...
[...][INFO ][BellCtrl] Pin configurati: RELAY1=25, RELAY2=26, LED=2
[...][INFO ][BellCtrl] Melodie predefinite caricate
```

**Checklist**:
- [ ] Nessun errore critico all'avvio
- [ ] BellController inizializzato
- [ ] Pin configurati correttamente

### Test 2: Relè e Campane
**Obiettivo**: Verificare attivazione fisica dei relè

**Procedura via Monitor Seriale**:
```
test_relay_1
test_relay_2
```

**Procedura via Web UI**:
1. Apri interfaccia web
2. Tab "Test Relè"
3. Clicca "Test Relè 1" e "Test Relè 2"

**Risultato Atteso**:
- [ ] Relè 1 si attiva per 500ms
- [ ] Relè 2 si attiva per 500ms
- [ ] LED di stato si accende durante attivazione
- [ ] Nessun errore nel log

**Verifica Log**:
```
[...][INFO ][BellCtrl] Campana 1 ATTIVATA (pin 25, durata 500ms)
[...][INFO ][BellCtrl] Campana 2 ATTIVATA (pin 26, durata 500ms)
```

### Test 3: RTC (se presente)
**Obiettivo**: Verificare comunicazione I2C con RTC

**Procedura**:
```
i2c
```

**Risultato Atteso**:
- [ ] Dispositivo trovato a indirizzo 0x68 (DS3231)
- [ ] Ora corrente letta correttamente

### Test 4: Display
**Obiettivo**: Verificare aggiornamento display

**Procedura**:
1. Osserva display T-Display
2. Verifica aggiornamento ogni secondo

**Checklist**:
- [ ] Ora visualizzata correttamente
- [ ] Data visualizzata
- [ ] Stato WiFi mostrato
- [ ] Stato campane mostrato
- [ ] Temperatura ESP32 mostrata

### Test 5: Temperatura
**Obiettivo**: Monitorare temperatura ESP32

**Procedura via Monitor Seriale**:
```
temp
```

**Risultato Atteso**:
```
🌡️ Temperatura ESP32: XX.X°C
Stato: OK / ELEVATA / CRITICA
```

**Checklist**:
- [ ] Temperatura letta correttamente (tipicamente 40-60°C)
- [ ] Nessun warning se temperatura normale
- [ ] Warning se temperatura > 70°C

---

## 🎵 Testing Funzionalità

### Test 6: Melodie Predefinite
**Obiettivo**: Verificare riproduzione melodie

**Procedura**:
```
play_melody_0  // FUNERALE
play_melody_1  // CHIAMATA MESSA
```

**Risultato Atteso**:
- [ ] Melodia 0 suona con sequenza corretta
- [ ] Melodia 1 suona con alternanza campane
- [ ] Melodie completate senza interruzioni

**Verifica Log (con Logger::DEBUG)**:
```
[...][INFO ][BellCtrl] AVVIO MELODIA: 'FUNERALE' (ID: 0, Note: 30)
[...][DEBUG][BellCtrl] Sequenza note per 'FUNERALE':
[...][DEBUG][BellCtrl]   [0] Campana 1: 300ms suono + 2700ms pausa
...
```

### Test 7: Stop Melodia
**Obiettivo**: Verificare interruzione melodia in corso

**Procedura**:
1. Avvia melodia: `play_melody_0`
2. Dopo 5 secondi: `stop_melody`

**Risultato Atteso**:
- [ ] Melodia si ferma immediatamente
- [ ] Tutti i relè disattivati

### Test 8: Gestione Errori
**Obiettivo**: Verificare validazione parametri

**Procedura**:
```
play_melody_99   // Indice non valido
test_relay_5     // Relè non valido
```

**Risultato Atteso**:
```
[...][ERROR][BellCtrl] Indice melodia non valido: 99 (max 9)
[...][ERROR][BellCtrl] Numero campana non valido: 5 (range: 1-2)
```

**Checklist**:
- [ ] Errori gestiti correttamente
- [ ] Nessun crash
- [ ] Messaggi di errore chiari

### Test 9: Campane Disabilitate
**Obiettivo**: Verificare blocco quando campane disabilitate

**Procedura**:
1. Disabilita campane: `disable_bells`
2. Prova suonare: `play_melody_0`

**Risultato Atteso**:
```
[...][WARN ][BellCtrl] Campane disabilitate - melodia 0 non riprodotta
```

**Checklist**:
- [ ] Melodia non suona
- [ ] Messaggio di warning appropriato
- [ ] Sistema rimane stabile

### Test 10: Modalità Test
**Obiettivo**: Verificare bypass campane disabilitate

**Procedura**:
1. Disabilita campane: `disable_bells`
2. Abilita test mode: `enable_test_mode`
3. Suona: `test_relay_1`

**Risultato Atteso**:
- [ ] Campana suona anche se disabilitate
- [ ] Modalità test attiva nel log

### Test 11: Programmazioni Settimanali
**Obiettivo**: Testare scheduler

**Procedura**:
1. Via web UI, crea programmazione per il giorno corrente, 2 minuti nel futuro
2. Aspetta trigger
3. Verifica suono

**Risultato Atteso**:
```
[...][INFO ][SCHED] ESECUZIONE: 'Test' -> melodia X
```

**Checklist**:
- [ ] Programmazione eseguita all'orario corretto
- [ ] Melodia riprodotta completamente
- [ ] Nessun doppio trigger

### Test 12: Protezione Termica
**Obiettivo**: Simulare protezione termica

**Nota**: Questo test è difficile da eseguire fisicamente senza riscaldare l'ESP32.
Si può testare modificando temporaneamente la soglia nel codice.

**Modifica temporanea in `config.h`**:
```cpp
constexpr float TEMP_WARNING_THRESHOLD = 30.0f;  // Abbassa per test
```

**Risultato Atteso**:
- [ ] Sistema rileva temperatura "elevata"
- [ ] Warning nel log
- [ ] Blocco operazioni se critica

---

## 💪 Testing Stress

### Test 13: Long Running
**Obiettivo**: Verificare stabilità a lungo termine

**Procedura**:
1. Lascia sistema acceso per 24 ore
2. Monitora memoria ogni ora: `status`

**Checklist**:
- [ ] Nessun crash
- [ ] Memoria stabile (no memory leak)
- [ ] Display aggiornato regolarmente
- [ ] WiFi connesso
- [ ] NTP sincronizzato

**Verifica Memoria**:
```
status
```
Nota la memoria libera, dovrebbe rimanere costante.

### Test 14: Stress Melodie
**Obiettivo**: Riproduzione continua

**Procedura**:
```
// Ripeti 10 volte
play_melody_0
// Attendi completamento
play_melody_1
// Attendi completamento
```

**Checklist**:
- [ ] Tutte le riprod

uzioni completate
- [ ] Nessun hang
- [ ] Memoria stabile

### Test 15: Stress API
**Obiettivo**: Molte richieste HTTP

**Procedura**:
Apri browser, esegui in console JavaScript:
```javascript
for (let i = 0; i < 100; i++) {
    fetch('/api/status').then(r => r.json()).then(console.log);
}
```

**Risultato Atteso**:
- [ ] Tutte le richieste servite
- [ ] Nessun timeout
- [ ] Rate limiting funziona (429 dopo troppe richieste)

### Test 16: Watchdog
**Obiettivo**: Verificare che watchdog non scatti

**Procedura**:
- Esegui operazioni normali per 30 minuti
- Monitora per reset inattesi

**Checklist**:
- [ ] Nessun reset watchdog
- [ ] Sistema stabile

---

## ✅ Checklist Finale

### Hardware ✓
- [ ] Tutti i relè funzionano
- [ ] Display aggiorna correttamente
- [ ] RTC sincronizzato (se presente)
- [ ] Temperatura monitorata

### Funzionalità ✓
- [ ] Melodie suonano correttamente
- [ ] Programmazioni eseguite
- [ ] Stop melodia funziona
- [ ] Gestione errori robusta

### Sicurezza ✓
- [ ] Campane disabilitate bloccano suoni
- [ ] Modalità test funziona
- [ ] Protezione termica attiva
- [ ] Emergency stop immediato

### Stabilità ✓
- [ ] Nessun memory leak
- [ ] Nessun crash dopo 24h
- [ ] WiFi stabile
- [ ] Scheduler affidabile

### UI/UX ✓
- [ ] Web UI responsive
- [ ] Tutte le API funzionano
- [ ] Backup/restore funziona
- [ ] Logging appropriato

---

## 📊 Report Testing

Compila dopo ogni sessione di test:

**Data Test**: _____________

**Versione Firmware**: v2.2

**Durata Test**: _____________

**Problemi Rilevati**:
1. ______________________________
2. ______________________________
3. ______________________________

**Note**:
______________________________
______________________________
______________________________

**Test Superati**: _____ / _____

**Stato Finale**: ✅ PASS / ❌ FAIL

---

## 🐛 Troubleshooting

### Relè non si attivano
- Verifica collegamenti fisici
- Controlla logica invertita (LOW = ON)
- Verifica pin corretti in `config.h`

### Melodie non suonano
- Verifica campane abilitate: `enable_bells`
- Controlla melodia valida: `list_melodies`
- Verifica log per errori

### Memoria bassa
- Riduci `DynamicJsonDocument` size
- Aggiungi più `yield()` nei loop
- Controlla memory leaks

### Watchdog Reset
- Aggiungi `yield()` in operazioni lunghe
- Riduci operazioni bloccanti
- Verifica timeout

### Display non aggiorna
- Controlla connessioni SPI
- Verifica `updateDisplay()` chiamato
- Controlla errori TFT_eSPI

---

**Buon Testing! 🎉**
