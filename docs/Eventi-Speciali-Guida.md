# 📅 Guida Completa: Eventi Speciali

**Sistema Controller Campane v2.3.1**  
**Sezione**: Tab "Programmazione" → Card "Eventi Speciali"

---

## 🎯 Cos'è un Evento Speciale?

Un **Evento Speciale** è una programmazione per date specifiche (non ricorrenti settimanalmente), come:
- 🕯️ Feste religiose (Natale, Pasqua, ecc.)
- 💒 Matrimoni
- ⚰️ Funerali straordinari
- 🎉 Celebrazioni particolari
- 📆 Date commemorative

A differenza della "Programmazione Settimanale" che si ripete ogni settimana, gli eventi speciali sono legati a **date del calendario** (anno-mese-giorno).

---

## 📋 Struttura di un Evento Speciale

Ogni evento speciale contiene **8 campi**:

### 1. **Nome** 📝
- **Tipo**: Campo di testo
- **Descrizione**: Nome descrittivo dell'evento
- **Esempi**: 
  - "Natale - Messa di Mezzanotte"
  - "Matrimonio Rossi-Bianchi"
  - "Festa Patrono"
- **Obbligatorio**: No (ma consigliato per identificazione)
- **Lunghezza max**: ~50 caratteri

---

### 2. **Data** 📅
- **Tipo**: Selector data (date picker)
- **Formato**: `YYYY-MM-DD` (es. `2025-12-25`)
- **Descrizione**: La data esatta in cui l'evento deve attivarsi
- **Campi interni**:
  - `year`: Anno (es. 2025)
  - `month`: Mese (1-12)
  - `day`: Giorno (1-31)
- **Default**: Data corrente
- **Validazione**: Deve essere una data valida del calendario

**Esempio HTML:**
```html
<input type="date" class="form-control" value="2025-12-25" data-field="date">
```

**Come viene salvato:**
```json
{
  "year": 2025,
  "month": 12,
  "day": 25
}
```

---

### 3. **Orario** ⏰
- **Tipo**: Selector orario (time picker)
- **Formato**: `HH:MM` (es. `19:30`)
- **Descrizione**: L'ora esatta in cui le campane devono suonare
- **Campi interni**:
  - `hour`: Ora (0-23)
  - `minute`: Minuti (0-59)
- **Default**: `12:00`
- **Validazione**: Deve essere un orario valido (formato 24 ore)

**Esempio HTML:**
```html
<input type="time" class="form-control" value="19:30" data-field="time">
```

**Come viene salvato:**
```json
{
  "hour": 19,
  "minute": 30
}
```

---

### 4. **Tipo** 🏷️
- **Tipo**: Numero intero
- **Range**: 0-255 (tipicamente 0-10)
- **Descrizione**: Categoria/tipo dell'evento per classificazione
- **Valori comuni**:
  - `0` = Generico/Standard
  - `1` = Festa solenne
  - `2` = Matrimonio
  - `3` = Funerale
  - `4` = Commemorazione
  - `5` = Altro
- **Default**: `0`
- **Uso**: Permette filtraggio/statistiche future

**Esempio HTML:**
```html
<input type="number" class="form-control" placeholder="Tipo" 
       value="0" min="0" data-field="type">
```

**Note**: 
- Attualmente non usato dal firmware per logica specifica
- Serve per organizzazione e future espansioni
- Non obbligatorio

---

### 5. **Melodia** 🎵
- **Tipo**: Dropdown/Select
- **Opzioni**: Tutte le melodie configurate nel sistema
- **Descrizione**: Quale melodia suonare quando l'evento si attiva
- **Campo interno**: `melodyIndex` (ID numerico della melodia)
- **Default**: Prima melodia disponibile (ID 0)
- **Melodie predefinite**:
  - ID 0 = FUNERALE (30 colpi alternati)
  - ID 1 = CHIAMATA MESSA (scampanio fitto)
  - ID 2+ = Melodie personalizzate

**Esempio HTML:**
```html
<select class="form-control" data-field="melodyIndex">
  <option value="0">FUNERALE</option>
  <option value="1" selected>CHIAMATA MESSA</option>
  <option value="2">Gloria</option>
</select>
```

**Come viene salvato:**
```json
{
  "melodyIndex": 1  // ID numerico della melodia
}
```

---

### 6. **Switch "Ricorrente"** 🔄 (isRecurring)
- **Tipo**: Toggle Switch (checkbox)
- **Valori**: `true` / `false` (ON/OFF)
- **Campo**: `isRecurring`
- **Descrizione**: Determina se l'evento si ripete ogni anno

#### 🔴 **isRecurring = false** (Switch OFF - Default)
- L'evento suona **UNA SOLA VOLTA** nella data specificata
- Dopo l'esecuzione, NON si ripeterà mai più
- **Uso tipico**: 
  - Matrimoni (data unica)
  - Funerali straordinari
  - Eventi irripetibili

**Esempio**: 
```
Data: 2025-06-15 alle 15:00
isRecurring: false
→ Suonerà SOLO il 15 giugno 2025 alle 15:00
→ Nel 2026 NON suonerà
```

#### 🟢 **isRecurring = true** (Switch ON)
- L'evento suona **OGNI ANNO** lo stesso giorno
- Si ripete automaticamente negli anni successivi
- Ignora il campo `year` (usa solo `month` e `day`)
- **Uso tipico**:
  - Natale (25 dicembre)
  - Pasqua (se data fissa)
  - Festa del patrono
  - Ricorrenze annuali

**Esempio**:
```
Data: 2025-12-25 alle 00:00
isRecurring: true
→ Suonerà il 25 dicembre 2025 alle 00:00
→ Suonerà il 25 dicembre 2026 alle 00:00
→ Suonerà il 25 dicembre 2027 alle 00:00
... ogni anno!
```

**Esempio HTML:**
```html
<label class="switch">
  <input type="checkbox" checked data-field="isRecurring">
  <span class="slider"></span>
</label>
```

**Logica firmware:**
```cpp
bool shouldTrigger(SpecialEvent event, DateTime now) {
  if (event.isRecurring) {
    // Ignora anno, controlla solo mese/giorno
    return (now.month() == event.month && 
            now.day() == event.day &&
            now.hour() == event.hour &&
            now.minute() == event.minute);
  } else {
    // Controlla anno, mese, giorno
    return (now.year() == event.year &&
            now.month() == event.month && 
            now.day() == event.day &&
            now.hour() == event.hour &&
            now.minute() == event.minute);
  }
}
```

---

### 7. **Switch "Attivo"** ✅ (isActive)
- **Tipo**: Toggle Switch (checkbox)
- **Valori**: `true` / `false` (ON/OFF)
- **Campo**: `isActive`
- **Descrizione**: Abilita/disabilita temporaneamente l'evento

#### 🟢 **isActive = true** (Switch ON - Default)
- L'evento è **ATTIVO** e suonerà quando la data/ora corrisponde
- Controllato normalmente dal sistema

#### 🔴 **isActive = false** (Switch OFF)
- L'evento è **DISABILITATO** temporaneamente
- NON suonerà anche se data/ora corrisponde
- **Uso tipico**:
  - Disabilitare evento senza cancellarlo
  - Test/manutenzione
  - Sospensione temporanea

**Differenza con eliminazione**:
- ❌ Eliminare = Rimuove permanentemente
- 🔴 Disattivare = Mantiene configurazione, ma non suona

**Esempio HTML:**
```html
<label class="switch">
  <input type="checkbox" checked data-field="isActive">
  <span class="slider"></span>
</label>
```

**Esempio pratico**:
```
Evento: Natale 2025
isActive: false
→ Il 25 dicembre 2025 NON suonerà
→ Configurazione salvata, posso riattivarlo in futuro
```

---

### 8. **Pulsante Elimina** 🗑️
- **Tipo**: Pulsante azione
- **Funzione**: `removeSpecialEvent(index)`
- **Descrizione**: Rimuove permanentemente l'evento dalla lista
- **Effetto**: L'evento viene cancellato dal JSON e dal sistema
- **Conferma**: Nessuna conferma (eliminazione immediata)

**Attenzione**: L'eliminazione è permanente! Se vuoi solo disabilitare temporaneamente, usa lo switch "Attivo".

---

## 🎨 Visualizzazione nell'Interfaccia

```
┌─────────────────────────────────────────────────────────────┐
│ Eventi Speciali                                              │
├─────────────────────────────────────────────────────────────┤
│ [➕ Aggiungi Evento] [💾 Salva Modifiche]                   │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ [Nome_______________] [2025-12-25] [00:00] [0]          │ │
│ │ [CHIAMATA MESSA ▼] [🔄 Ricorrente: ON] [✅ Attivo: ON]  │ │
│ │                                             [🗑️ Elimina] │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                               │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ [Matrimonio Rossi] [2025-06-15] [15:00] [2]            │ │
│ │ [FUNERALE ▼] [🔄 Ricorrente: OFF] [✅ Attivo: ON]       │ │
│ │                                             [🗑️ Elimina] │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                               │
└─────────────────────────────────────────────────────────────┘

Legenda campi:
┌────────────────┬──────────┬─────────┬────────┬────────────────┬──────────────┬────────────┬─────────┐
│ Nome           │ Data     │ Orario  │ Tipo   │ Melodia        │ Ricorrente   │ Attivo     │ Azione  │
└────────────────┴──────────┴─────────┴────────┴────────────────┴──────────────┴────────────┴─────────┘
```

---

## 📝 Esempio Completo: Evento Natale

### Configurazione JSON completa:
```json
{
  "id": 1703520000,
  "name": "Natale - Messa Mezzanotte",
  "type": 1,
  "year": 2025,
  "month": 12,
  "day": 25,
  "hour": 0,
  "minute": 0,
  "melodyIndex": 1,
  "isRecurring": true,
  "isActive": true
}
```

### Significato:
- **Nome**: "Natale - Messa Mezzanotte"
- **Data**: 25 dicembre (ogni anno perché `isRecurring=true`)
- **Orario**: 00:00 (mezzanotte)
- **Tipo**: 1 (Festa solenne)
- **Melodia**: ID 1 (CHIAMATA MESSA)
- **Ricorrente**: ✅ SÌ (si ripete ogni 25 dicembre)
- **Attivo**: ✅ SÌ (funzionante)

### Comportamento:
```
✅ 25 dicembre 2025 alle 00:00 → Suona CHIAMATA MESSA
✅ 25 dicembre 2026 alle 00:00 → Suona CHIAMATA MESSA
✅ 25 dicembre 2027 alle 00:00 → Suona CHIAMATA MESSA
... ogni anno per sempre
```

---

## 🔧 Operazioni Disponibili

### 1. ➕ **Aggiungi Evento**
```javascript
function addSpecialEvent() {
  const now = new Date();
  specialEvents.push({
    id: Date.now(),           // ID univoco (timestamp)
    name: '',                 // Nome vuoto (da compilare)
    type: 0,                  // Tipo generico
    year: now.getFullYear(),  // Anno corrente
    month: now.getMonth() + 1,// Mese corrente (1-12)
    day: now.getDate(),       // Giorno corrente
    hour: 12,                 // Mezzogiorno di default
    minute: 0,                // 00 minuti
    melodyIndex: 0,           // Prima melodia
    isRecurring: false,       // Non ricorrente
    isActive: true            // Attivo di default
  });
  renderSpecialEvents();
}
```

**Crea un nuovo evento** con valori di default:
- Data/ora corrente
- Non ricorrente
- Attivo
- Melodia base

---

### 2. 💾 **Salva Modifiche**
```javascript
async function saveSpecialEvents() {
  try {
    const events = collectSpecialEvents(); // Raccoglie dati da form
    
    await apiCall('/api/special-events', {
      method: 'POST',
      body: JSON.stringify({ specialEvents: events })
    });
    
    showNotification('Eventi speciali salvati', 'success');
  } catch (error) {
    showNotification('Errore salvataggio eventi', 'error');
  }
}
```

**Salva tutti gli eventi** sul firmware ESP32:
1. Raccoglie dati da tutti i campi HTML
2. Converte in formato JSON
3. Invia a `/api/special-events` via POST
4. Firmware salva in SPIFFS (persistente)

---

### 3. 🗑️ **Elimina Evento**
```javascript
function removeSpecialEvent(index) {
  specialEvents.splice(index, 1); // Rimuove da array
  renderSpecialEvents();          // Aggiorna UI
}
```

**Rimuove evento** dalla lista (solo in memoria, devi salvare dopo).

---

### 4. 🔄 **Raccolta Dati (collectSpecialEvents)**
```javascript
function collectSpecialEvents() {
  const events = [];
  
  document.querySelectorAll('#specialContainer .schedule-item').forEach((item, index) => {
    const event = { ...specialEvents[index] }; // Copia base
    
    // Scorre tutti i campi input
    item.querySelectorAll('[data-field]').forEach(input => {
      const field = input.getAttribute('data-field');
      
      if (field === 'date') {
        // Separa YYYY-MM-DD
        const [year, month, day] = input.value.split('-');
        event.year = parseInt(year) || 2025;
        event.month = parseInt(month) || 1;
        event.day = parseInt(day) || 1;
        
      } else if (field === 'time') {
        // Separa HH:MM
        const [hour, minute] = input.value.split(':');
        event.hour = parseInt(hour) || 0;
        event.minute = parseInt(minute) || 0;
        
      } else if (field === 'melodyIndex') {
        // Converti a numero
        event[field] = parseInt(input.value) || 0;
        
      } else if (input.type === 'checkbox') {
        // Switch ON/OFF
        event[field] = input.checked;
        
      } else if (input.type === 'number') {
        // Campo numerico (tipo)
        event[field] = parseInt(input.value) || 0;
        
      } else {
        // Campo testo (nome)
        event[field] = input.value;
      }
    });
    
    events.push(event);
  });
  
  return events;
}
```

**Raccoglie tutti i dati** dai campi HTML e li converte in array JSON.

---

## 🎭 Casi d'Uso Pratici

### Caso 1: Natale (Ricorrente)
```json
{
  "name": "Natale",
  "year": 2025,
  "month": 12,
  "day": 25,
  "hour": 0,
  "minute": 0,
  "melodyIndex": 1,
  "isRecurring": true,  ← ATTIVA RICORRENZA ANNUALE
  "isActive": true
}
```
**Risultato**: Suona ogni 25 dicembre per sempre.

---

### Caso 2: Matrimonio (Una Tantum)
```json
{
  "name": "Matrimonio Rossi-Bianchi",
  "year": 2025,
  "month": 6,
  "day": 15,
  "hour": 15,
  "minute": 30,
  "type": 2,
  "melodyIndex": 0,
  "isRecurring": false, ← EVENTO SINGOLO
  "isActive": true
}
```
**Risultato**: Suona SOLO il 15/06/2025 alle 15:30, mai più.

---

### Caso 3: Evento Disabilitato Temporaneamente
```json
{
  "name": "Festa Patrono",
  "month": 8,
  "day": 15,
  "hour": 10,
  "minute": 0,
  "isRecurring": true,
  "isActive": false ← DISABILITATO (ma salvato)
}
```
**Risultato**: NON suona, ma configurazione mantenuta.

---

## ⚙️ Logica Firmware (Backend)

### Controllo Attivazione Evento

```cpp
void checkSpecialEvents(DateTime now) {
  for (SpecialEvent event : specialEvents) {
    // Skip se disabilitato
    if (!event.isActive) continue;
    
    bool shouldTrigger = false;
    
    if (event.isRecurring) {
      // Evento ricorrente: ignora anno
      shouldTrigger = (now.month() == event.month &&
                       now.day() == event.day &&
                       now.hour() == event.hour &&
                       now.minute() == event.minute);
    } else {
      // Evento singolo: controlla anche anno
      shouldTrigger = (now.year() == event.year &&
                       now.month() == event.month &&
                       now.day() == event.day &&
                       now.hour() == event.hour &&
                       now.minute() == event.minute);
    }
    
    if (shouldTrigger) {
      // Avvia melodia
      playMelody(event.melodyIndex);
      
      // Se non ricorrente, marca come eseguito (opzionale)
      if (!event.isRecurring) {
        event.isActive = false; // Auto-disabilita dopo esecuzione
      }
    }
  }
}
```

---

## 📊 Tabella Riepilogativa Campi

| Campo | Tipo | Obbligatorio | Default | Descrizione |
|-------|------|--------------|---------|-------------|
| **id** | number | ✅ | `Date.now()` | Identificatore univoco |
| **name** | string | ❌ | `""` | Nome descrittivo |
| **type** | number | ❌ | `0` | Categoria (0-255) |
| **year** | number | ✅ | Anno corrente | Anno (se non ricorrente) |
| **month** | number | ✅ | Mese corrente | Mese (1-12) |
| **day** | number | ✅ | Giorno corrente | Giorno (1-31) |
| **hour** | number | ✅ | `12` | Ora (0-23) |
| **minute** | number | ✅ | `0` | Minuti (0-59) |
| **melodyIndex** | number | ✅ | `0` | ID melodia da suonare |
| **isRecurring** | boolean | ✅ | `false` | Ripeti ogni anno |
| **isActive** | boolean | ✅ | `true` | Evento abilitato |

---

## 🚨 Validazioni e Limiti

### Limiti Sistema:
- **Max eventi**: ~50 (dipende da memoria SPIFFS)
- **Data minima**: 2000-01-01
- **Data massima**: 2099-12-31
- **Nome max**: ~50 caratteri
- **Tipo range**: 0-255

### Validazioni JavaScript:
```javascript
// Data valida
if (!input.value.match(/^\d{4}-\d{2}-\d{2}$/)) {
  alert("Data non valida");
}

// Orario valido
if (!input.value.match(/^\d{2}:\d{2}$/)) {
  alert("Orario non valido");
}

// MelodyIndex valido
if (melodyIndex < 0 || melodyIndex >= melodies.length) {
  alert("Melodia non valida");
}
```

---

## 🔍 Troubleshooting

### Problema: Evento non suona
**Checklist**:
1. ✅ Switch "Attivo" è ON?
2. ✅ Data/ora corrette?
3. ✅ Melodia valida selezionata?
4. ✅ RTC sincronizzato? (controlla con `ntp_diag`)
5. ✅ Campane abilitate globalmente? (Home → Attiva Campane)

### Problema: Evento ricorrente non si ripete
**Causa**: Switch "Ricorrente" non attivo  
**Soluzione**: Attiva toggle isRecurring

### Problema: Modifiche non salvate
**Causa**: Non premuto "💾 Salva Modifiche"  
**Soluzione**: Dopo ogni modifica, clicca sempre "Salva Modifiche"

---

## 📚 Riferimenti Tecnici

### API Endpoint:
```http
GET  /api/special-events        → Legge eventi salvati
POST /api/special-events        → Salva nuovi eventi
```

### Formato JSON Request (POST):
```json
{
  "specialEvents": [
    {
      "id": 1703520000,
      "name": "Evento",
      "type": 0,
      "year": 2025,
      "month": 12,
      "day": 25,
      "hour": 0,
      "minute": 0,
      "melodyIndex": 0,
      "isRecurring": true,
      "isActive": true
    }
  ]
}
```

### File Storage:
- **Path**: `/config.json` (SPIFFS)
- **Sezione**: `special_events[]`
- **Persistenza**: Sopravvive a riavvii

---

**Fine Documentazione Eventi Speciali** 📅

Per domande o supporto, consulta anche:
- `docs/Deploy-v2.3.1-Quick.md` - Deployment
- `README.md` - Overview generale
