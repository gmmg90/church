# 🎯 Quick Start Testing

## Avvio Rapido

1. **Compila e carica il firmware**
   ```bash
   pio run --target upload
   ```

2. **Apri monitor seriale**
   ```bash
   pio device monitor
   ```

3. **Verifica inizializzazione**
   ```
   Attendi log di avvio...
   ✓ BellController inizializzato
   ✓ WiFi connesso
   ✓ Web Server avviato
   ```

4. **Esegui test base**
   ```
   Scrivi: status
   Scrivi: test_relay_1
   Scrivi: play_melody_0
   ```

5. **Apri interfaccia web**
   ```
   http://[IP_DISPLAY]
   ```

## Comandi Rapidi Essenziali

| Comando | Descrizione |
|---------|-------------|
| `help` | Mostra tutti i comandi |
| `status` | Stato sistema completo |
| `test_relay_1` | Test campana 1 |
| `test_relay_2` | Test campana 2 |
| `play_melody_0` | Suona FUNERALE |
| `play_melody_1` | Suona CHIAMATA MESSA |
| `stop_melody` | Ferma melodia |
| `enable_bells` | Abilita campane |
| `disable_bells` | Disabilita campane |
| `temp` | Mostra temperatura |
| `debug` | Debug programmazioni |

## Problemi Comuni

### ❌ Problema: Campane non suonano
**Soluzione**:
```
enable_bells
enable_test_mode
test_relay_1
```

### ❌ Problema: Display non mostra nulla
**Verifica**: 
- Connessione SPI corretta
- Alimentazione sufficiente
- File `platformio.ini` configurato per T-Display

### ❌ Problema: WiFi non connette
**Soluzione**:
1. Via web UI in modalità AP:
   - Connetti a rete "ChurchBells-Config"
   - Password: "campanile123"
   - Vai su http://192.168.4.1
   - Configura WiFi

### ❌ Problema: Reset continui
**Verifica**:
- Temperatura ESP32 (`temp`)
- Memoria disponibile (`status`)
- Alimentazione stabile

## Livelli di Logger

Modifica in `setup()`:

```cpp
// Debugging dettagliato
Logger::setLevel(Logger::DEBUG);

// Normale (consigliato)
Logger::setLevel(Logger::INFO);

// Solo errori
Logger::setLevel(Logger::ERROR);
```

## Test Checklist Rapida

Prima di chiudere testing:

- [ ] `status` - Sistema OK
- [ ] `temp` - Temperatura < 70°C
- [ ] `test_relay_1` - Campana 1 OK
- [ ] `test_relay_2` - Campana 2 OK
- [ ] `play_melody_0` - Melodia completa
- [ ] Web UI accessibile
- [ ] Programmazioni salvate

## Link Utili

- [Guida Testing Completa](Testing-Guide.md)
- [Script Test Automatici](Test-Scripts.md)
- [Manuale Installazione](Manuale-Installazione.md)

---

**Pronto per il testing! 🚀**

Per testing completo, vedi [Testing-Guide.md](Testing-Guide.md)
