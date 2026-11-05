# 📚 Guida API Explorer - Sistema Campanacci v2.3.4

## Panoramica

L'**API Explorer** è un'interfaccia web integrata che permette di visualizzare, esplorare e testare tutte le API REST disponibili nel sistema campanacci ESP32.

---

## 🚀 Come Accedere

1. Apri l'interfaccia web del sistema: `http://<IP_ESP32>`
2. Vai alla tab **Impostazioni**
3. Clicca sul pulsante **📚 API Explorer**

---

## ✨ Funzionalità

### 🔍 Ricerca
- Campo di ricerca in tempo reale
- Cerca per: path, descrizione, metodo HTTP
- Filtraggio istantaneo dei risultati

### 🗂️ Categorie API
Le API sono organizzate in 4 categorie:

#### ℹ️ **Informazioni** (Info)
API di sola lettura per ottenere lo stato del sistema:
- `/api/status` - Stato completo del sistema
- `/api/time` - Data e ora corrente
- `/api/relay-status` - Stato relè campanacci
- `/api/weekly-schedules` - Programmazioni settimanali
- `/api/special-events` - Eventi speciali
- `/api/melodies` - Melodie configurate

#### 🎮 **Controllo** (Control)
API per controllare attivamente il sistema:
- `/api/toggle-bells` - Abilita/disabilita campanacci
- `/api/test-melody` - Testa una melodia
- `/api/emergency-stop` - Arresto emergenza
- `/api/set-relay` - Controllo manuale relè

#### ⚙️ **Configurazione** (Config)
API per modificare le impostazioni:
- `/api/weekly-schedules` (POST) - Salva programmazioni
- `/api/special-events` (POST) - Salva eventi
- `/api/melody` (POST) - Salva melodia
- `/api/set-time` (POST) - Imposta ora
- `/api/wifi-config` - Configurazione WiFi
- `/api/ntp-config` - Configurazione NTP

#### 🔍 **Diagnostica** (Diagnostic)
API per monitoraggio e troubleshooting:
- `/api/diagnostics` - Diagnostica completa
- `/api/i2c-scan` - Scansione dispositivi I2C

---

## 📊 Badge e Indicatori

### Metodi HTTP
- 🟢 **GET** - Lettura dati (verde)
- 🔵 **POST** - Creazione/modifica (blu)
- 🔴 **DELETE** - Eliminazione (rosso)

### Tipi API
- 🔵 **info** - Informazioni (blu chiaro)
- 🟡 **control** - Controllo (giallo)
- 🟣 **config** - Configurazione (viola)
- 🟣 **diagnostic** - Diagnostica (rosa)

---

## 🔗 Testare le API

### Metodo 1: Click diretto
1. Clicca su un qualsiasi endpoint nella lista
2. Si apre in una nuova tab del browser
3. Vedrai la risposta JSON

### Metodo 2: Pulsante "🔗 Apri"
1. Clicca sul pulsante "🔗 Apri" a destra dell'endpoint
2. Si apre in una nuova tab

### Metodo 3: Copia il path
1. Copia il path dell'endpoint (es: `/api/status`)
2. Usa con strumenti come:
   - Browser: `http://<IP_ESP32>/api/status`
   - cURL: `curl http://<IP_ESP32>/api/status`
   - Postman/Insomnia
   - JavaScript: `fetch('/api/status')`

---

## 💡 Esempi d'Uso

### Esempio 1: Leggere lo stato del sistema
```bash
# Browser o cURL
http://192.168.1.100/api/status

# Risposta JSON
{
  "wifi": {"connected": true, "ip": "192.168.1.100"},
  "rtc": {"running": true},
  "ntp": {"synced": true},
  "bells": {"enabled": true},
  "temperature": 42.5
}
```

### Esempio 2: Testare una melodia (POST)
```bash
curl -X POST http://192.168.1.100/api/test-melody \
  -H "Content-Type: application/json" \
  -d '{"melodyId": 1}'
```

### Esempio 3: Salvare programmazione (POST)
```bash
curl -X POST http://192.168.1.100/api/weekly-schedules \
  -H "Content-Type: application/json" \
  -d '{
    "schedules": [
      {"day": 0, "hour": 12, "minute": 0, "melody": 1, "enabled": true}
    ]
  }'
```

### Esempio 4: Controllo relè manuale (GET con parametri)
```bash
# Nel browser o cURL
http://192.168.1.100/api/set-relay?relay=0&state=1

# relay: numero relè (0-7)
# state: 0=OFF, 1=ON
```

---

## 🎯 Filtri Rapidi

Usa i pulsanti nella toolbar per filtrare rapidamente:

| Pulsante | Mostra |
|----------|--------|
| 🌐 Tutte | Tutte le API (22+ endpoints) |
| ℹ️ Info | Solo API informative (8) |
| 🎮 Controllo | Solo API di controllo (4) |
| ⚙️ Config | Solo API di configurazione (10) |
| 🔍 Diagnosi | Solo API diagnostiche (2) |

---

## ⌨️ Scorciatoie da Tastiera

| Tasto | Azione |
|-------|--------|
| `ESC` | Chiudi API Explorer |
| Click fuori modal | Chiudi API Explorer |

---

## 🛠️ Integrazione con Altri Strumenti

### JavaScript (Frontend)
```javascript
// Fetch API status
fetch('/api/status')
  .then(response => response.json())
  .then(data => console.log(data));

// Post melody test
fetch('/api/test-melody', {
  method: 'POST',
  headers: {'Content-Type': 'application/json'},
  body: JSON.stringify({melodyId: 1})
})
.then(response => response.json())
.then(data => console.log(data));
```

### Python
```python
import requests

# Get status
response = requests.get('http://192.168.1.100/api/status')
print(response.json())

# Test melody
response = requests.post(
    'http://192.168.1.100/api/test-melody',
    json={'melodyId': 1}
)
print(response.json())
```

### Node.js
```javascript
const axios = require('axios');

// Get diagnostics
axios.get('http://192.168.1.100/api/diagnostics')
  .then(response => console.log(response.data));

// Toggle bells
axios.post('http://192.168.1.100/api/toggle-bells')
  .then(response => console.log(response.data));
```

---

## 📱 Responsive Design

L'API Explorer è completamente responsive:
- **Desktop**: Layout a due colonne con badge laterali
- **Mobile**: Layout verticale con badge impilati
- **Tablet**: Layout ottimizzato automaticamente

---

## 🔒 Sicurezza

⚠️ **Importante**: 
- Le API sono accessibili a chiunque sia connesso alla rete
- Non c'è autenticazione implementata
- Usare solo su reti fidate (WiFi privato)
- API di controllo e configurazione possono modificare il sistema

---

## 🐛 Troubleshooting

### Problema: API non risponde
**Soluzione**:
1. Verifica connessione WiFi ESP32
2. Controlla log nella Diagnostica Completa
3. Riavvia ESP32 se necessario

### Problema: Modal non si apre
**Soluzione**:
1. Ricarica la pagina web (F5)
2. Svuota cache browser (Ctrl+Shift+R)
3. Verifica che JavaScript sia abilitato

### Problema: API restituisce errore
**Soluzione**:
1. Verifica sintassi del JSON (per POST)
2. Controlla che i parametri siano corretti
3. Leggi la descrizione dell'API nell'Explorer
4. Consulta i log nella Diagnostica

---

## 📖 API Completa Reference

### GET APIs (Lettura)

| Endpoint | Descrizione | Risposta |
|----------|-------------|----------|
| `/api/status` | Stato sistema completo | JSON con WiFi, RTC, NTP, temp |
| `/api/time` | Data/ora corrente | JSON con timestamp |
| `/api/relay-status` | Stato tutti i relè | JSON array con stati |
| `/api/weekly-schedules` | Programmazioni settimanali | JSON array schedules |
| `/api/special-events` | Eventi speciali | JSON array events |
| `/api/melodies` | Lista melodie | JSON array melodies |
| `/api/diagnostics` | Diagnostica completa | JSON con metriche sistema |
| `/api/i2c-scan` | Scan dispositivi I2C | JSON array indirizzi |
| `/api/wifi-config` | Config WiFi corrente | JSON con SSID, IP |
| `/api/ntp-config` | Config NTP corrente | JSON con server, timezone |

### POST APIs (Scrittura)

| Endpoint | Body JSON | Descrizione |
|----------|-----------|-------------|
| `/api/toggle-bells` | - | Abilita/disabilita campanacci |
| `/api/test-melody` | `{melodyId: 1}` | Testa melodia |
| `/api/emergency-stop` | - | Arresto emergenza |
| `/api/weekly-schedules` | `{schedules: [...]}` | Salva programmazioni |
| `/api/special-events` | `{events: [...]}` | Salva eventi speciali |
| `/api/melody` | `{id, name, notes, durations}` | Salva melodia |
| `/api/set-time` | `{year, month, day, hour, minute, second}` | Imposta ora |
| `/api/wifi-config` | `{ssid, password}` | Config WiFi |
| `/api/ntp-config` | `{server, timezone}` | Config NTP |

### DELETE APIs (Eliminazione)

| Endpoint | Descrizione |
|----------|-------------|
| `/api/weekly-schedules` | Elimina tutte programmazioni |
| `/api/special-events` | Elimina tutti eventi speciali |

---

## 📝 Log e Tracking

Ogni operazione nell'API Explorer viene automaticamente loggata:
- Apertura/chiusura modale
- Click su endpoint
- Apertura API in nuova tab

I log sono visibili nella **Diagnostica Completa** → **Log Eventi**.

---

## 🎨 Personalizzazione

Il design dell'API Explorer segue lo stesso tema della Diagnostica:
- Dark theme
- Colori blu/azzurro
- Animazioni smooth
- Layout responsive

---

## 📊 Statistiche

- **22+ API endpoints** disponibili
- **3 metodi HTTP**: GET, POST, DELETE
- **4 categorie**: Info, Control, Config, Diagnostic
- **Ricerca real-time** su path e descrizioni
- **100% responsive** su tutti i dispositivi

---

## 🔄 Versione

- **Versione**: v2.3.4
- **Data**: 2025
- **Compatibile con**: Firmware v2.3.0+
- **Richiede**: index.html v2.3.4+

---

## 📞 Supporto

Per assistenza:
1. Consulta la **Diagnostica Completa** per verificare lo stato del sistema
2. Controlla i **Log Eventi** per errori recenti
3. Verifica la connessione WiFi e heap disponibile
4. Riavvia ESP32 se necessario

---

## ✅ Checklist Test API Explorer

- [ ] Modal si apre correttamente
- [ ] Tutte le 22+ API sono elencate
- [ ] Filtri per categoria funzionano
- [ ] Ricerca filtra in tempo reale
- [ ] Click su API apre nuova tab
- [ ] Pulsante "🔗 Apri" funziona
- [ ] Badge colorati visibili
- [ ] Responsive su mobile
- [ ] ESC chiude il modal
- [ ] Click fuori chiude il modal
- [ ] Log registra apertura/chiusura
- [ ] Nessun errore JavaScript in console

---

**🎉 Buon testing delle API!**
