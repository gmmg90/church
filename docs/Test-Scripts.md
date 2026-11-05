# 🤖 Script Test Automatici

## Test Suite Rapida (5 minuti)

Esegui questi comandi in sequenza nel monitor seriale:

```bash
# 1. Diagnostica Sistema
help
status
temp
time
i2c

# 2. Test Hardware Base
test_relay_1
# Attendi 1 secondo
test_relay_2
# Attendi 1 secondo

# 3. Test Melodie
list_melodies
play_melody_0
# Attendi 5 secondi
stop_melody

# 4. Test Gestione Errori
play_melody_99      # Dovrebbe dare errore
test_relay_5        # Dovrebbe dare errore

# 5. Test Abilitazione Campane
disable_bells
play_melody_0       # Non dovrebbe suonare
enable_bells
play_melody_1       # Dovrebbe suonare

# 6. Test Modalità Test
disable_bells
enable_test_mode
test_relay_1        # Dovrebbe suonare anche se disabilitate
disable_test_mode

# 7. Debug Programmazioni
debug
list_schedules

# 8. Verifica Finale
status
temp
```

## Test Suite Completa (30 minuti)

### Setup Iniziale
```bash
# Imposta logger a DEBUG
enable_test_mode
disable_test_mode
status
```

### Test Sequenziale Campane
```bash
# Test 100 impulsi campana 1
test_relay_1
# Attendi 1s
test_relay_1
# Ripeti 98 volte...
# Verifica memoria dopo ogni 10 test
```

### Test Melodie Continue
```bash
play_melody_0
# Attendi completamento (~90s per FUNERALE)
play_melody_1
# Attendi completamento (~40s per CHIAMATA MESSA)
# Ripeti ciclo 5 volte
status  # Verifica memoria
```

### Test Stress Scheduler
```bash
# Crea programmazione per minuto successivo via Web UI
force_schedule_check
# Ripeti per 10 minuti
```

## Comandi Diagnostica Avanzata

### Memory Check
```bash
status
# Nota: freeHeap, totalBellRings
# Ripeti dopo ogni test importante
```

### Performance Check
```bash
time
# Verifica sincronizzazione
status
# Verifica uptime, no reset
```

### Temperature Monitoring
```bash
temp
# Ogni 5 minuti durante stress test
# Deve rimanere < 70°C
```

## Test di Regressione

Dopo ogni modifica al codice, esegui:

```bash
# Quick Smoke Test
status
test_relay_1
test_relay_2
play_melody_0
stop_melody
enable_bells
disable_bells
temp
time
```

Se tutti passano, il sistema è stabile.

## Automazione con Python

Per automatizzare i test, usa questo script Python:

```python
import serial
import time

ser = serial.Serial('COM3', 115200, timeout=1)  # Modifica COM port
time.sleep(2)

def send_command(cmd):
    print(f"Invio: {cmd}")
    ser.write((cmd + '\n').encode())
    time.sleep(0.5)
    while ser.in_waiting:
        response = ser.readline().decode('utf-8', errors='ignore').strip()
        if response:
            print(f"  -> {response}")

# Test Suite
commands = [
    'status',
    'test_relay_1',
    'test_relay_2',
    'play_melody_0',
    'stop_melody',
    'temp',
    'time'
]

for cmd in commands:
    send_command(cmd)
    time.sleep(2)  # Attesa tra comandi

print("Test completati!")
ser.close()
```

## Risultati Attesi

✅ **PASS Criteria**:
- Tutti i comandi eseguiti senza errore CRITICAL
- Memoria stabile (variazione < 5%)
- Temperatura < 70°C
- Nessun reset involontario
- Tutte le melodie completate

❌ **FAIL Criteria**:
- Errori CRITICAL nel log
- Memory leak (memoria decresce costantemente)
- Temperatura > 75°C
- Reset watchdog
- Melodie incomplete

## Log di Esempio Corretto

```
[   1234567][INFO ][BellCtrl] testBell(bellNumber=1)
[   1234568][DEBUG][BellCtrl] ringBell(campana=1, durata=500ms)
[   1234569][INFO ][BellCtrl] Campana 1 ATTIVATA (pin 25, durata 500ms)
```

## Log di Esempio con Errore

```
[   1234567][ERROR][BellCtrl] Indice melodia non valido: 99 (max 9)
```

---

**Note**: 
- Tutti i test devono essere eseguiti con campane DISABILITATE per sicurezza, a meno che non si stia testando specificamente il suono
- Monitora sempre la temperatura durante stress test
- Documenta ogni anomalia
