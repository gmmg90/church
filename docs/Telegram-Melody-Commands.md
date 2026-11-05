# 🔔 Comandi Melodie Bot Telegram - Quick Reference

## Comandi Melodie Implementati

### `/funeral` o `/funerale`
**Avvia lo scampanio funerale (melodia indice 0)**

```
/funeral
```

**Comportamento:**
- ✅ Verifica che la melodia 0 sia configurata
- ✅ Controlla che non ci siano melodie in esecuzione
- ✅ Se campane disattivate, usa modalità test
- ✅ Mostra durata prevista prima dell'esecuzione
- ✅ Conferma completamento

**Risposta:**
```
⚫ Scampanio Funerale

Avvio melodia funerale...
Durata: XX secondi

✅ Scampanio funerale completato
```

---

### `/mass` o `/messa`
**Avvia la chiamata messa (melodia indice 1)**

```
/mass
```

**Comportamento:**
- ✅ Verifica che la melodia 1 sia configurata
- ✅ Controlla che non ci siano melodie in esecuzione
- ✅ Se campane disattivate, usa modalità test
- ✅ Mostra durata prevista prima dell'esecuzione
- ✅ Conferma completamento

**Risposta:**
```
⛪ Chiamata Messa

Avvio chiamata messa...
Durata: XX secondi

✅ Chiamata messa completata
```

---

### `/melody <indice>` o `/melodia <indice>`
**Avvia una melodia specifica (0-9)**

```
/melody 3
```

**Parametri:**
- `indice`: Numero melodia da 0 a 9

**Comportamento:**
- ✅ Validazione indice (0-9)
- ✅ Verifica che la melodia sia configurata
- ✅ Controlla che non ci siano melodie in esecuzione
- ✅ Se campane disattivate, usa modalità test
- ✅ Mostra info melodia (nome, durata, numero note)
- ✅ Conferma completamento

**Risposta:**
```
🎵 Melodia: Nome Melodia

Indice: 3
Durata: XX secondi
Note: YY

✅ Melodia completata
```

---

## Messaggi di Errore

### ❌ Campane disattivate
```
⚠️ Le campane sono disattivate. Uso modalità test...
```
La melodia viene eseguita comunque in modalità test (bypassa il flag bellsEnabled).

### ❌ Melodia non configurata
```
❌ Melodia X non configurata
```
La melodia richiesta non ha note configurate.

### ❌ Melodia già in esecuzione
```
⚠️ Una melodia è già in esecuzione. Attendi il completamento.
```
Il sistema può eseguire una sola melodia alla volta.

### ❌ BellController non disponibile
```
❌ BellController non disponibile
```
Errore interno: il controller campane non è inizializzato.

---

## Caratteristiche di Sicurezza

✅ **Validazione Input** - Tutti i parametri vengono validati
✅ **Controllo Concorrenza** - Una melodia alla volta
✅ **Modalità Test** - Funziona anche con campane disattivate
✅ **Logging** - Tutte le azioni vengono registrate
✅ **Feedback Utente** - Conferme e messaggi di errore chiari
✅ **Non Bloccante** - Il bot rimane reattivo durante l'esecuzione

---

## Esempio Sessione Completa

```
User: /status
Bot:  📊 Stato Sistema
      🔔 Campane: ✅ ATTIVE
      ...

User: /funeral
Bot:  ⚫ Scampanio Funerale
      Avvio melodia funerale...
      Durata: 45 secondi
      
      ✅ Scampanio funerale completato

User: /messa
Bot:  ⛪ Chiamata Messa
      Avvio chiamata messa...
      Durata: 30 secondi
      
      ✅ Chiamata messa completata

User: /melody 5
Bot:  🎵 Melodia: Festa Patronale
      Indice: 5
      Durata: 60 secondi
      Note: 48
      
      ✅ Melodia completata
```

---

## Note Tecniche

### File Modificati
- `src/telegram_bot.cpp` - Implementazione comandi
- `src/include/telegram_bot.h` - Dichiarazioni funzioni
- `docs/Telegram-Setup-Guide.md` - Documentazione

### Indici Melodie Standard
- **0** = Scampanio Funerale
- **1** = Chiamata Messa
- **2-9** = Melodie personalizzate

### Integrazione con Sistema
- Usa `BellController::playMelody(index)`
- Verifica stato con `isPlayingMelody()`
- Ottiene info con `getMelodyName()`, `getMelodyDuration()`, `getMelodyNoteCount()`
- Log tramite `Logger::info()` e `Logger::error()`

---

**Versione Firmware:** v2.3.9-telegram  
**Data Implementazione:** 07/01/2026
