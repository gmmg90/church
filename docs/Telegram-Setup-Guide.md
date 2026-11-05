# 🤖 Guida Configurazione Bot Telegram

## Passo 1: Creare il Bot su Telegram

1. **Apri Telegram** sul tuo smartphone o computer
2. **Cerca @BotFather** nella barra di ricerca
3. **Avvia la conversazione** con `/start`
4. **Crea un nuovo bot** digitando:
   ```
   /newbot
   ```
5. **Scegli un nome** per il bot (esempio: "Campane Chiesa San Marco")
6. **Scegli un username** per il bot (deve finire con "bot", esempio: "SanMarcoChiesaBot")
7. **BotFather ti invierà il TOKEN** - **COPIALO E CONSERVALO!**
   - Formato: `123456789:ABCdefGHIjklMNOpqrsTUVwxyz`

## Passo 2: Configurare il Token nel Firmware

1. **Apri** `src/config.cpp`
2. **Trova la riga**:
   ```cpp
   const char* TELEGRAM_BOT_TOKEN = "123456789:ABCdefGHIjklMNOpqrsTUVwxyz";
   ```
3. **Sostituisci** con il tuo token ricevuto da BotFather
4. **Salva il file**

## Passo 3: Ottenere il tuo Chat ID

### Metodo 1: Tramite il Bot (CONSIGLIATO)

1. **Compila e carica** il firmware sull'ESP32
2. **Cerca il tuo bot** su Telegram (usando l'username scelto)
3. **Avvia la conversazione** con `/start`
4. **Il bot ti mostrerà il tuo Chat ID** nel messaggio di benvenuto
5. **Copia il Chat ID** (sarà un numero tipo `123456789`)

### Metodo 2: Manuale

1. **Cerca @userinfobot** su Telegram
2. **Avvia la conversazione** con `/start`
3. **Il bot ti mostrerà il tuo Chat ID**

## Passo 4: Autorizzare Utenti

### Opzione A: Via Web Interface (Dopo il primo caricamento)

1. Accedi all'interfaccia web del sistema
2. Vai su **Impostazioni → Telegram**
3. Inserisci i Chat ID degli utenti da autorizzare
4. Salva le modifiche

### Opzione B: Manualmente nel codice (PRIMA del caricamento)

1. **Apri** `src/config.cpp`
2. **Trova il blocco**:
   ```cpp
   const char* TELEGRAM_AUTHORIZED_USERS[] = {
       // "123456789",     // Esempio
       // "987654321",
   };
   const uint8_t TELEGRAM_AUTHORIZED_COUNT = 0;
   ```
3. **Aggiungi i Chat ID**:
   ```cpp
   const char* TELEGRAM_AUTHORIZED_USERS[] = {
       "123456789",     // Tuo Chat ID
       "987654321",     // Chat ID di un altro utente
   };
   const uint8_t TELEGRAM_AUTHORIZED_COUNT = 2;  // Numero di utenti
   ```
4. **Salva e ricompila**

## Passo 5: Compilare e Caricare

1. **Apri il terminale** in VS Code (Terminal → New Terminal)
2. **Compila il progetto**:
   ```bash
   pio run
   ```
3. **Carica sul dispositivo**:
   ```bash
   pio run --target upload
   ```
4. **Monitora il log seriale** (opzionale):
   ```bash
   pio device monitor
   ```

## Verifica Funzionamento

1. **Controlla il log seriale** - Dovresti vedere:
   ```
   [Telegram] Inizializzazione bot...
   [Telegram] ✓ Bot inizializzato con successo
   [Telegram] ✓ Utente autorizzato: 123456789
   ```

2. **Apri Telegram** e cerca il tuo bot
3. **Invia** `/start` - Dovresti ricevere un messaggio di benvenuto
4. **Prova un comando**: `/status` - Dovresti ricevere lo stato del sistema

## 🎯 Comandi Disponibili

Dopo aver avviato il bot, ecco i comandi che puoi usare:

### Comandi Base
- `/start` - Messaggio di benvenuto e verifica autorizzazione
- `/help` - Lista completa dei comandi
- `/status` - Stato attuale del sistema (campane, WiFi, temperatura)
- `/info` - Informazioni tecniche del sistema

### Controllo Campane
- `/enable` o `/attiva` - Attiva le campane
- `/disable` o `/disattiva` - Disattiva le campane
- `/ring 1 3000` - Suona campana 1 per 3 secondi
- `/ring 2 5000` - Suona campana 2 per 5 secondi

### Melodie
- `/funeral` o `/funerale` - Avvia scampanio funerale (melodia 0)
- `/mass` o `/messa` - Avvia chiamata messa (melodia 1)
- `/melody 3` - Avvia melodia specifica (0-9)

### Programmazione
- `/schedule` o `/orari` - Mostra prossimi eventi programmati

### Autenticazione
- `/auth` - Richiedi autorizzazione (mostra il tuo Chat ID all'admin)

## 🔐 Sicurezza

- **Solo utenti autorizzati** possono controllare il sistema
- Il **token del bot è segreto** - non condividerlo
- Tutti i comandi eseguiti vengono **registrati nel log**
- Puoi **revocare l'accesso** rimuovendo il Chat ID dalla lista autorizzati

## 🚨 Notifiche Automatiche

Il bot può inviare notifiche automatiche per:
- ✅ Avvio del sistema
- ⚠️ Errori critici
- 🔥 Temperatura elevata
- 📡 Perdita connessione WiFi

Per abilitare/disabilitare le notifiche, modifica in `config.h`:
```cpp
#define TELEGRAM_NOTIFICATIONS_ENABLED true
```

## ❓ Risoluzione Problemi

### Il bot non risponde
1. **Verifica il token** - Assicurati sia corretto in `config.cpp`
2. **Controlla WiFi** - Il bot necessita di connessione internet
3. **Verifica il log seriale** - Cerca messaggi di errore `[Telegram]`

### "Non sei autorizzato"
1. **Ottieni il tuo Chat ID** con `/start`
2. **Aggiungi il Chat ID** alla lista autorizzati in `config.cpp`
3. **Ricompila e ricarica** il firmware

### Il bot è lento
- È normale un leggero ritardo (1-2 secondi)
- Il bot controlla nuovi messaggi ogni secondo
- La priorità è data al controllo campane

## 📝 Esempio Configurazione Completa

**File: `src/config.cpp`**
```cpp
// Token del bot (da @BotFather)
const char* TELEGRAM_BOT_TOKEN = "6789012345:AAHdqTcvCH1vGWJxfSeofSAs0K5PALDsaw";

// Lista utenti autorizzati
const char* TELEGRAM_AUTHORIZED_USERS[] = {
    "123456789",     // Don Marco (parroco)
    "987654321",     // Giovanni (sacrestano)
    "555777888",     // Maria (segretaria)
};
const uint8_t TELEGRAM_AUTHORIZED_COUNT = 3;
```

## 🎉 Ora sei pronto!

Il tuo sistema di campane è controllabile da remoto tramite Telegram! 🔔

---

**Nota**: Questo sistema NON richiede apertura porte sul router - funziona ovunque ci sia connessione internet!
