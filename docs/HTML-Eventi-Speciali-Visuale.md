# 🎨 HTML Eventi Speciali - Guida Visuale Completa

**Versione:** 2.3.1  
**File:** `data/index.html` (linee 730-740 circa)

---

## 📱 COME APPARE SULLO SCHERMO

```
╔════════════════════════════════════════════════════════════════════╗
║                        EVENTI SPECIALI                              ║
╠════════════════════════════════════════════════════════════════════╣
║                                                                     ║
║  [➕ Aggiungi Evento]  [💾 Salva Modifiche]                       ║
║                                                                     ║
║  ┌────────────────────────────────────────────────────────────┐   ║
║  │                                                             │   ║
║  │  ┌──────────────┐  ┌──────────┐  ┌───────┐  ┌───┐        │   ║
║  │  │ Natale       │  │2025-12-25│  │ 00:00 │  │ 1 │        │   ║
║  │  └──────────────┘  └──────────┘  └───────┘  └───┘        │   ║
║  │      ① NOME          ② DATA      ③ ORA    ④ TIPO         │   ║
║  │                                                             │   ║
║  │  ┌────────────────────┐  ┌─────────┐  ┌─────────┐        │   ║
║  │  │ CHIAMATA MESSA  ▼ │  │ ⭕─○   │  │ ⭕─○   │  [🗑️]   │   ║
║  │  └────────────────────┘  └─────────┘  └─────────┘        │   ║
║  │    ⑤ MELODIA           ⑥ RICORRENTE ⑦ ATTIVO  ⑧ ELIMINA │   ║
║  │                                                             │   ║
║  └────────────────────────────────────────────────────────────┘   ║
║                                                                     ║
║  ┌────────────────────────────────────────────────────────────┐   ║
║  │                                                             │   ║
║  │  ┌──────────────┐  ┌──────────┐  ┌───────┐  ┌───┐        │   ║
║  │  │ Matrimonio   │  │2025-06-15│  │ 15:00 │  │ 2 │        │   ║
║  │  └──────────────┘  └──────────┘  └───────┘  └───┘        │   ║
║  │                                                             │   ║
║  │  ┌────────────────────┐  ┌─────────┐  ┌─────────┐        │   ║
║  │  │ FUNERALE        ▼ │  │ ○─⭕   │  │ ⭕─○   │  [🗑️]   │   ║
║  │  └────────────────────┘  └─────────┘  └─────────┘        │   ║
║  │                            OFF          ON                 │   ║
║  │                                                             │   ║
║  └────────────────────────────────────────────────────────────┘   ║
║                                                                     ║
╚════════════════════════════════════════════════════════════════════╝
```

---

## 🔍 CODICE HTML COMPLETO

### 📦 Container Principale

```html
<!-- Sezione Eventi Speciali nel tab "Programmazione" -->
<div class="card">
  <h2>Eventi Speciali</h2>
  
  <!-- Pulsanti di controllo -->
  <div class="btn-group">
    <button class="btn btn-success" id="addSpecialBtn">
      ➕ Aggiungi Evento
    </button>
    <button class="btn btn-primary" id="saveSpecialBtn">
      💾 Salva Modifiche
    </button>
  </div>
  
  <!-- Container dove vengono inseriti gli eventi -->
  <div id="specialContainer"></div>
</div>
```

**Spiegazione:**
- `<div class="card">` → Box bianco con ombra
- `<h2>` → Titolo sezione
- `<div class="btn-group">` → Contenitore pulsanti affiancati
- `<div id="specialContainer">` → **QUI dentro** vengono generati dinamicamente gli eventi

---

## 🎯 STRUTTURA DI UN SINGOLO EVENTO

### HTML Generato Dinamicamente (JavaScript)

```html
<!-- Questo viene creato da renderSpecialEvents() -->
<div class="schedule-item">
  
  <!-- Contenuto evento -->
  <div class="schedule-content">
    
    <!-- ① CAMPO NOME -->
    <input 
      type="text" 
      class="form-control" 
      placeholder="Nome" 
      value="Natale" 
      data-field="name"
    />
    
    <!-- ② CAMPO DATA -->
    <input 
      type="date" 
      class="form-control" 
      value="2025-12-25" 
      data-field="date"
    />
    
    <!-- ③ CAMPO ORARIO -->
    <input 
      type="time" 
      class="form-control" 
      value="00:00" 
      data-field="time"
    />
    
    <!-- ④ CAMPO TIPO -->
    <input 
      type="number" 
      class="form-control" 
      placeholder="Tipo" 
      value="1" 
      min="0" 
      data-field="type"
    />
    
    <!-- ⑤ SELECT MELODIA -->
    <select class="form-control" data-field="melodyIndex">
      <option value="0">FUNERALE</option>
      <option value="1" selected>CHIAMATA MESSA</option>
      <option value="2">Gloria</option>
    </select>
    
    <!-- ⑥ SWITCH RICORRENTE -->
    <label class="switch">
      <input 
        type="checkbox" 
        checked 
        data-field="isRecurring"
      />
      <span class="slider"></span>
    </label>
    
    <!-- ⑦ SWITCH ATTIVO -->
    <label class="switch">
      <input 
        type="checkbox" 
        checked 
        data-field="isActive"
      />
      <span class="slider"></span>
    </label>
    
  </div>
  
  <!-- ⑧ PULSANTE ELIMINA -->
  <button class="btn btn-danger" onclick="removeSpecialEvent(0)">
    🗑️
  </button>
  
</div>
```

---

## 🎨 DETTAGLIO COMPONENTI HTML

### ① CAMPO NOME (Text Input)

```html
<input 
  type="text"           ← Tipo campo: testo libero
  class="form-control"  ← Classe CSS per styling
  placeholder="Nome"    ← Testo grigio quando vuoto
  value="Natale"        ← Valore corrente
  data-field="name"     ← Identificatore per JavaScript
/>
```

**Aspetto visivo:**
```
┌─────────────────────┐
│ Natale              │ ← Testo editabile
└─────────────────────┘
```

**Come funziona:**
1. Utente clicca → cursore appare
2. Utente scrive "Matrimonio Rossi"
3. JavaScript legge `input.value` quando salvi
4. Valore salvato nel campo `name` del JSON

**CSS applicato:**
```css
.form-control {
  padding: 8px 12px;
  border: 1px solid #ddd;
  border-radius: 5px;
  font-size: 14px;
}
```

---

### ② CAMPO DATA (Date Picker)

```html
<input 
  type="date"           ← Browser mostra calendario
  class="form-control"  
  value="2025-12-25"    ← Formato YYYY-MM-DD
  data-field="date"     
/>
```

**Aspetto visivo:**
```
┌──────────────┐
│ 25/12/2025 📅│ ← Click = apre calendario
└──────────────┘
```

**Come funziona:**
1. Click → Si apre calendario del browser
2. Utente seleziona giorno
3. Valore salvato come `"2025-12-25"`
4. JavaScript divide in: `year=2025`, `month=12`, `day=25`

**Codice conversione:**
```javascript
const [year, month, day] = input.value.split('-');
event.year = parseInt(year);   // 2025
event.month = parseInt(month); // 12
event.day = parseInt(day);     // 25
```

---

### ③ CAMPO ORARIO (Time Picker)

```html
<input 
  type="time"           ← Browser mostra selettore ore
  class="form-control"  
  value="00:00"         ← Formato HH:MM (24 ore)
  data-field="time"     
/>
```

**Aspetto visivo:**
```
┌────────┐
│ 00:00 ⏰│ ← Click = selettore ore/minuti
└────────┘
```

**Come funziona:**
1. Click → Selettore ore e minuti
2. Utente sceglie 15:30
3. Valore salvato come `"15:30"`
4. JavaScript divide in: `hour=15`, `minute=30`

**Codice conversione:**
```javascript
const [hour, minute] = input.value.split(':');
event.hour = parseInt(hour);     // 15
event.minute = parseInt(minute); // 30
```

---

### ④ CAMPO TIPO (Number Input)

```html
<input 
  type="number"         ← Solo numeri, con ▲▼
  class="form-control"  
  placeholder="Tipo"    
  value="1"             ← Valore iniziale
  min="0"               ← Minimo 0
  data-field="type"     
/>
```

**Aspetto visivo:**
```
┌──────┐
│ 1  ▲ │ ← Frecce per incrementare/decrementare
│    ▼ │
└──────┘
```

**Come funziona:**
1. Utente clicca ▲ → incrementa (1→2→3...)
2. Utente clicca ▼ → decrementa (3→2→1)
3. Oppure digita numero diretto
4. JavaScript legge: `parseInt(input.value)`

**Valori suggeriti:**
- `0` = Generico
- `1` = Festa religiosa
- `2` = Matrimonio
- `3` = Funerale

---

### ⑤ SELECT MELODIA (Dropdown)

```html
<select class="form-control" data-field="melodyIndex">
  <option value="0">FUNERALE</option>
  <option value="1" selected>CHIAMATA MESSA</option>
  <option value="2">Gloria</option>
  <option value="3">Ave Maria</option>
</select>
```

**Aspetto visivo:**
```
┌─────────────────────┐
│ CHIAMATA MESSA    ▼ │ ← Click = apre menu
└─────────────────────┘

Quando clicchi:
┌─────────────────────┐
│ FUNERALE            │
│▸CHIAMATA MESSA      │ ← Selezionato
│ Gloria              │
│ Ave Maria           │
└─────────────────────┘
```

**Come viene popolato (JavaScript):**
```javascript
// Codice che genera le option
<select data-field="melodyIndex">
  ${melodies.map(m => 
    `<option value="${m.id}" ${m.id === event.melodyIndex ? 'selected' : ''}>
      ${m.name}
    </option>`
  ).join('')}
</select>
```

**Esempio:**
```javascript
melodies = [
  { id: 0, name: "FUNERALE" },
  { id: 1, name: "CHIAMATA MESSA" },
  { id: 2, name: "Gloria" }
];

event.melodyIndex = 1; // CHIAMATA MESSA selezionato
```

---

### ⑥ SWITCH RICORRENTE (Toggle ON/OFF)

```html
<label class="switch">
  <input 
    type="checkbox"      ← Checkbox invisibile
    checked              ← Attributo = ON (true)
    data-field="isRecurring"
  />
  <span class="slider"></span> ← Slider visibile
</label>
```

**Aspetto visivo:**

**OFF (isRecurring = false):**
```
┌─────────┐
│ ○───────│ ← Pallino a sinistra, grigio
└─────────┘
```

**ON (isRecurring = true):**
```
┌─────────┐
│────────⭕│ ← Pallino a destra, verde
└─────────┘
```

**CSS Toggle Switch:**
```css
.switch {
  position: relative;
  width: 50px;
  height: 24px;
}

.switch input {
  opacity: 0;        /* Checkbox nascosto */
  width: 0;
  height: 0;
}

.slider {
  position: absolute;
  cursor: pointer;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: #ccc;  /* Grigio quando OFF */
  border-radius: 24px;
  transition: 0.4s;
}

/* Pallino */
.slider:before {
  position: absolute;
  content: "";
  height: 18px;
  width: 18px;
  left: 3px;
  bottom: 3px;
  background-color: white;
  border-radius: 50%;
  transition: 0.4s;
}

/* Quando CHECKED (ON) */
input:checked + .slider {
  background-color: #4caf50;  /* Verde */
}

input:checked + .slider:before {
  transform: translateX(26px);  /* Sposta pallino a destra */
}
```

**Come funziona:**
```javascript
// Lettura stato
if (input.checked) {
  // Switch è ON
  event.isRecurring = true;
} else {
  // Switch è OFF
  event.isRecurring = false;
}
```

**Animazione:**
```
User click → Switch OFF:
┌─────────┐           ┌─────────┐
│ ○───────│  →  ⚡  → │────────⭕│
└─────────┘           └─────────┘
 0.4s transition
```

---

### ⑦ SWITCH ATTIVO (Toggle ON/OFF)

```html
<label class="switch">
  <input 
    type="checkbox" 
    checked              ← ON di default
    data-field="isActive"
  />
  <span class="slider"></span>
</label>
```

**Identico a ⑥ Ricorrente, ma campo diverso:**

**OFF (isActive = false):**
```
┌─────────┐
│ ○───────│ ← DISABILITATO (non suona)
└─────────┘
```

**ON (isActive = true):**
```
┌─────────┐
│────────⭕│ ← ABILITATO (suona normalmente)
└─────────┘
```

**Differenza con Ricorrente:**
- **Ricorrente** → Controlla SE ripetere ogni anno
- **Attivo** → Controlla SE evento è abilitato

---

### ⑧ PULSANTE ELIMINA (Button)

```html
<button 
  class="btn btn-danger"        ← Classe CSS rosso
  onclick="removeSpecialEvent(0)" ← Funzione JS
>
  🗑️
</button>
```

**Aspetto visivo:**
```
┌─────┐
│ 🗑️  │ ← Pulsante rosso
└─────┘
```

**CSS:**
```css
.btn-danger {
  background: #ef4444;
  color: white;
  padding: 8px 12px;
  border: none;
  border-radius: 5px;
  cursor: pointer;
}

.btn-danger:hover {
  background: #dc2626;  /* Rosso più scuro al passaggio mouse */
}
```

**JavaScript:**
```javascript
function removeSpecialEvent(index) {
  // Conferma (opzionale)
  // if (!confirm("Eliminare evento?")) return;
  
  // Rimuove da array
  specialEvents.splice(index, 1);
  
  // Ridisegna UI
  renderSpecialEvents();
}
```

---

## 🔄 FLUSSO COMPLETO HTML → JAVASCRIPT → JSON

### 1️⃣ Caricamento Iniziale

```javascript
// 1. Carica dati da ESP32
async function loadSpecialEvents() {
  const data = await apiCall('/api/special-events');
  specialEvents = data.specialEvents || [];
  renderSpecialEvents();
}

// 2. Genera HTML
function renderSpecialEvents() {
  const container = document.getElementById('specialContainer');
  container.innerHTML = ''; // Svuota
  
  // 3. Per ogni evento, crea HTML
  specialEvents.forEach((event, index) => {
    const eventDiv = document.createElement('div');
    eventDiv.className = 'schedule-item';
    
    // 4. Prepara valori
    const dateStr = `${event.year}-${String(event.month).padStart(2,'0')}-${String(event.day).padStart(2,'0')}`;
    const timeStr = `${String(event.hour).padStart(2,'0')}:${String(event.minute).padStart(2,'0')}`;
    
    // 5. Genera HTML completo
    eventDiv.innerHTML = `
      <div class="schedule-content">
        <input type="text" value="${event.name}" data-field="name"/>
        <input type="date" value="${dateStr}" data-field="date"/>
        <input type="time" value="${timeStr}" data-field="time"/>
        <input type="number" value="${event.type}" data-field="type"/>
        <select data-field="melodyIndex">...</select>
        <label class="switch">
          <input type="checkbox" ${event.isRecurring ? 'checked' : ''} data-field="isRecurring"/>
          <span class="slider"></span>
        </label>
        <label class="switch">
          <input type="checkbox" ${event.isActive ? 'checked' : ''} data-field="isActive"/>
          <span class="slider"></span>
        </label>
      </div>
      <button onclick="removeSpecialEvent(${index})">🗑️</button>
    `;
    
    // 6. Aggiungi al container
    container.appendChild(eventDiv);
  });
}
```

---

### 2️⃣ Modifica dall'Utente

```
Utente modifica campo → Browser aggiorna input.value → Niente ancora salvato
                                                          (solo in memoria HTML)
```

**Esempio:**
```html
<!-- Prima -->
<input type="text" value="Natale" data-field="name"/>

<!-- Utente scrive "Pasqua" -->
<input type="text" value="Pasqua" data-field="name"/>
           ↑
    Cambiato ma non salvato!
```

---

### 3️⃣ Salvataggio (Click "💾 Salva Modifiche")

```javascript
// 1. Button click
document.getElementById('saveSpecialBtn').addEventListener('click', async () => {
  
  // 2. Raccoglie dati da TUTTI i campi HTML
  const events = collectSpecialEvents();
  
  // 3. Invia a ESP32
  await apiCall('/api/special-events', {
    method: 'POST',
    body: JSON.stringify({ specialEvents: events })
  });
  
  showNotification('Eventi salvati', 'success');
});
```

**collectSpecialEvents() in dettaglio:**
```javascript
function collectSpecialEvents() {
  const events = [];
  
  // Per ogni evento visibile nell'HTML
  document.querySelectorAll('#specialContainer .schedule-item')
    .forEach((item, index) => {
    
    // Copia base da array originale
    const event = { ...specialEvents[index] };
    
    // Trova TUTTI gli input dentro questo evento
    item.querySelectorAll('[data-field]').forEach(input => {
      
      const field = input.getAttribute('data-field');
      
      // ① NOME (text)
      if (field === 'name') {
        event.name = input.value;
      }
      
      // ② DATA (date)
      else if (field === 'date') {
        const [year, month, day] = input.value.split('-');
        event.year = parseInt(year);
        event.month = parseInt(month);
        event.day = parseInt(day);
      }
      
      // ③ ORARIO (time)
      else if (field === 'time') {
        const [hour, minute] = input.value.split(':');
        event.hour = parseInt(hour);
        event.minute = parseInt(minute);
      }
      
      // ④ TIPO (number)
      else if (field === 'type') {
        event.type = parseInt(input.value) || 0;
      }
      
      // ⑤ MELODIA (select)
      else if (field === 'melodyIndex') {
        event.melodyIndex = parseInt(input.value);
      }
      
      // ⑥⑦ SWITCH (checkbox)
      else if (input.type === 'checkbox') {
        event[field] = input.checked; // true/false
      }
    });
    
    events.push(event);
  });
  
  return events;
}
```

---

### 4️⃣ Formato JSON Finale

```javascript
// Questo viene inviato a ESP32
{
  "specialEvents": [
    {
      "id": 1703520000,
      "name": "Natale",           // ← Da input text
      "type": 1,                  // ← Da input number
      "year": 2025,               // ← Da input date
      "month": 12,                // ← Da input date
      "day": 25,                  // ← Da input date
      "hour": 0,                  // ← Da input time
      "minute": 0,                // ← Da input time
      "melodyIndex": 1,           // ← Da select
      "isRecurring": true,        // ← Da checkbox (switch)
      "isActive": true            // ← Da checkbox (switch)
    },
    {
      "id": 1703520123,
      "name": "Matrimonio",
      "type": 2,
      "year": 2025,
      "month": 6,
      "day": 15,
      "hour": 15,
      "minute": 30,
      "melodyIndex": 0,
      "isRecurring": false,       // ← Switch OFF
      "isActive": true
    }
  ]
}
```

---

## 🎯 ESEMPIO PRATICO PASSO-PASSO

### Scenario: Creare evento "Pasqua 2025"

#### PASSO 1: Click "➕ Aggiungi Evento"
```javascript
// JavaScript esegue
addSpecialEvent(); // Aggiunge evento vuoto
```

**Risultato HTML:**
```html
<div class="schedule-item">
  <div class="schedule-content">
    <input type="text" value="" placeholder="Nome"/>      ← VUOTO
    <input type="date" value="2025-11-09"/>               ← Data oggi
    <input type="time" value="12:00"/>                    ← Mezzogiorno
    <input type="number" value="0"/>                      ← Tipo 0
    <select><option value="0" selected>FUNERALE</option></select>
    <label class="switch">
      <input type="checkbox" data-field="isRecurring"/>   ← OFF
      <span class="slider"></span>
    </label>
    <label class="switch">
      <input type="checkbox" checked data-field="isActive"/> ← ON
      <span class="slider"></span>
    </label>
  </div>
  <button>🗑️</button>
</div>
```

---

#### PASSO 2: Compila Campi

**Utente compila:**
```
① Nome: "Pasqua"
② Data: Click calendario → 2025-04-20
③ Orario: Click ore → 10:30
④ Tipo: Digita "1"
⑤ Melodia: Select → "Gloria" (ID 2)
⑥ Ricorrente: Click switch → ON (ogni anno)
⑦ Attivo: Già ON
```

**HTML dopo modifiche:**
```html
<div class="schedule-item">
  <div class="schedule-content">
    <input type="text" value="Pasqua" data-field="name"/>
    <input type="date" value="2025-04-20" data-field="date"/>
    <input type="time" value="10:30" data-field="time"/>
    <input type="number" value="1" data-field="type"/>
    <select data-field="melodyIndex">
      <option value="0">FUNERALE</option>
      <option value="1">CHIAMATA MESSA</option>
      <option value="2" selected>Gloria</option>  ← Selezionato
    </select>
    <label class="switch">
      <input type="checkbox" checked data-field="isRecurring"/>  ← ON
      <span class="slider"></span>
    </label>
    <label class="switch">
      <input type="checkbox" checked data-field="isActive"/>
      <span class="slider"></span>
    </label>
  </div>
  <button onclick="removeSpecialEvent(2)">🗑️</button>
</div>
```

---

#### PASSO 3: Click "💾 Salva Modifiche"

**JavaScript raccoglie:**
```javascript
collectSpecialEvents(); // Legge HTML

// Trova input con data-field="date"
const dateInput = document.querySelector('[data-field="date"]');
const dateValue = dateInput.value; // "2025-04-20"

// Separa
const [year, month, day] = dateValue.split('-');
// year = "2025", month = "04", day = "20"

// Converte a numeri
event.year = parseInt(year);   // 2025
event.month = parseInt(month); // 4 (non "04")
event.day = parseInt(day);     // 20
```

**JSON generato:**
```json
{
  "id": 1699445678912,
  "name": "Pasqua",
  "type": 1,
  "year": 2025,
  "month": 4,
  "day": 20,
  "hour": 10,
  "minute": 30,
  "melodyIndex": 2,
  "isRecurring": true,
  "isActive": true
}
```

**Inviato a ESP32:**
```http
POST /api/special-events
Content-Type: application/json

{
  "specialEvents": [ ...evento Pasqua... ]
}
```

---

#### PASSO 4: ESP32 Salva su SPIFFS

```cpp
// Firmware ESP32 riceve
server.on("/api/special-events", HTTP_POST, [](AsyncWebServerRequest *request){
  // Legge JSON
  String json = request->getParam("plain", true)->value();
  
  // Salva in /config.json
  File file = SPIFFS.open("/config.json", "w");
  file.print(json);
  file.close();
  
  request->send(200, "application/json", "{\"success\":true}");
});
```

---

#### PASSO 5: Controllo Attivazione

```cpp
// Loop principale firmware
void checkSpecialEvents() {
  DateTime now = rtc.now();
  
  for (SpecialEvent event : specialEvents) {
    if (!event.isActive) continue; // Skip se disabilitato
    
    if (event.isRecurring) {
      // Evento ricorrente: ignora anno
      if (now.month() == 4 &&          // Aprile
          now.day() == 20 &&           // Giorno 20
          now.hour() == 10 &&          // Ore 10
          now.minute() == 30) {        // Minuti 30
        
        playMelody(event.melodyIndex); // Suona Gloria (ID 2)
      }
    }
  }
}
```

**Risultato:**
```
📅 20 aprile 2025 ore 10:30 → 🔔 Suona melodia "Gloria"
📅 20 aprile 2026 ore 10:30 → 🔔 Suona melodia "Gloria"
📅 20 aprile 2027 ore 10:30 → 🔔 Suona melodia "Gloria"
... ogni anno!
```

---

## 📊 TABELLA RIEPILOGO HTML

| Campo | Tag HTML | Attributi Chiave | Valore Esempio | Output JSON |
|-------|----------|------------------|----------------|-------------|
| **Nome** | `<input type="text">` | `data-field="name"` | `"Pasqua"` | `"name": "Pasqua"` |
| **Data** | `<input type="date">` | `data-field="date"` | `"2025-04-20"` | `"year":2025, "month":4, "day":20` |
| **Ora** | `<input type="time">` | `data-field="time"` | `"10:30"` | `"hour":10, "minute":30` |
| **Tipo** | `<input type="number">` | `data-field="type"` | `1` | `"type": 1` |
| **Melodia** | `<select>` | `data-field="melodyIndex"` | `value="2"` | `"melodyIndex": 2` |
| **Ricorrente** | `<input type="checkbox">` | `data-field="isRecurring"` | `checked` | `"isRecurring": true` |
| **Attivo** | `<input type="checkbox">` | `data-field="isActive"` | `checked` | `"isActive": true` |
| **Elimina** | `<button>` | `onclick="removeSpecialEvent(i)"` | - | (rimuove elemento) |

---

## 🎨 CSS COMPLETO

```css
/* Card container */
.card {
  background: white;
  border-radius: 15px;
  padding: 20px;
  box-shadow: 0 4px 6px rgba(0,0,0,0.1);
  margin-bottom: 20px;
}

/* Gruppo pulsanti */
.btn-group {
  display: flex;
  gap: 10px;
  margin-bottom: 15px;
}

/* Singolo evento */
.schedule-item {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 15px;
  background: #f9fafb;
  border-radius: 10px;
  margin-bottom: 10px;
}

/* Contenuto evento (campi) */
.schedule-content {
  display: grid;
  grid-template-columns: 2fr 1.5fr 1fr 0.5fr 2fr auto auto;
  gap: 10px;
  flex: 1;
  align-items: center;
}

/* Input generici */
.form-control {
  padding: 8px 12px;
  border: 1px solid #ddd;
  border-radius: 5px;
  font-size: 14px;
  background: white;
}

.form-control:focus {
  outline: none;
  border-color: #667eea;
  box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
}

/* Toggle switch */
.switch {
  position: relative;
  display: inline-block;
  width: 50px;
  height: 24px;
}

.switch input {
  opacity: 0;
  width: 0;
  height: 0;
}

.slider {
  position: absolute;
  cursor: pointer;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-color: #ccc;
  border-radius: 24px;
  transition: 0.4s;
}

.slider:before {
  position: absolute;
  content: "";
  height: 18px;
  width: 18px;
  left: 3px;
  bottom: 3px;
  background-color: white;
  border-radius: 50%;
  transition: 0.4s;
}

input:checked + .slider {
  background-color: #4caf50;
}

input:checked + .slider:before {
  transform: translateX(26px);
}

/* Pulsanti */
.btn {
  padding: 10px 20px;
  border: none;
  border-radius: 8px;
  font-size: 14px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.3s;
}

.btn-success {
  background: #10b981;
  color: white;
}

.btn-success:hover {
  background: #059669;
}

.btn-primary {
  background: #3b82f6;
  color: white;
}

.btn-primary:hover {
  background: #2563eb;
}

.btn-danger {
  background: #ef4444;
  color: white;
  padding: 8px 12px;
}

.btn-danger:hover {
  background: #dc2626;
}
```

---

## 🔍 DEBUGGING HTML

### Come Ispezionare nell'HTML

**F12 → Elements:**
```html
<div id="specialContainer">
  
  <div class="schedule-item" data-index="0">  ← Primo evento
    <div class="schedule-content">
      <input type="text" value="Natale" data-field="name">
      ...
    </div>
  </div>
  
  <div class="schedule-item" data-index="1">  ← Secondo evento
    <div class="schedule-content">
      <input type="text" value="Pasqua" data-field="name">
      ...
    </div>
  </div>
  
</div>
```

### Console JavaScript

```javascript
// Mostra tutti gli eventi HTML
document.querySelectorAll('.schedule-item').forEach((item, i) => {
  console.log(`Evento ${i}:`, item);
});

// Leggi valore campo specifico
const nameInput = document.querySelector('[data-field="name"]');
console.log('Nome:', nameInput.value);

// Leggi stato switch
const recurringSwitch = document.querySelector('[data-field="isRecurring"]');
console.log('Ricorrente:', recurringSwitch.checked); // true/false

// Simula raccolta dati
const events = collectSpecialEvents();
console.log('Eventi raccolti:', events);
```

---

## ✅ CHECKLIST FINALE

**HTML Corretto:**
- ✅ `id="specialContainer"` presente
- ✅ Ogni input ha `data-field="..."` corretto
- ✅ Date format `YYYY-MM-DD`
- ✅ Time format `HH:MM`
- ✅ Checkbox per switch (non button)
- ✅ `onclick` su pulsante elimina

**JavaScript Corretto:**
- ✅ `renderSpecialEvents()` popola HTML
- ✅ `collectSpecialEvents()` legge HTML
- ✅ Conversione corretta date/time
- ✅ `parseInt()` per numeri

**CSS Corretto:**
- ✅ `.switch` e `.slider` per toggle
- ✅ `.form-control` per input styling
- ✅ Grid layout per allineamento

---

**Fine Guida HTML Visuale** 🎨

Ora l'HTML dovrebbe essere super chiaro! 😊
