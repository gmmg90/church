# 🔍 Pannello Diagnostica Completa - Guida Utente

**Versione**: v2.3.3  
**Data**: Novembre 2025  
**Feature**: Modal diagnostica con monitoraggio completo sistema

---

## 📋 PANORAMICA

Il **Pannello Diagnostica Completa** è un nuovo modal accessibile dall'interfaccia web che fornisce una visione completa e in tempo reale di:
- 📊 Stato sistema (firmware, uptime, CPU, chip)
- 🧠 Memoria (heap, frammentazione, allocazioni)
- 📡 WiFi e rete (qualità segnale, IP, NTP, RTC)
- 🌡️ Temperatura (ESP32, warning, protezione termica)
- 🔔 Campane (stato, suonate totali, ultima attivazione)
- 📅 Programmazioni (settimanali, eventi speciali, melodie)
- 📝 Log eventi in tempo reale (ultimi 100 eventi)

---

## 🎯 COME ACCEDERE

### **Metodo 1: Pulsante principale**
1. Apri interfaccia web: `http://<IP_ESP32>`
2. Vai alla sezione **"Impostazioni"** (tab in alto)
3. Clicca sul pulsante **"🔍 Diagnostica Completa"**

### **Caratteristiche**:
- ✅ **Auto-refresh**: Dati aggiornati ogni 5 secondi automaticamente
- ✅ **3 tab organizzati**: Panoramica, Programmazioni, Log Eventi
- ✅ **Colori intuitivi**: 
  - 🟢 Verde = OK/Normale
  - 🟡 Giallo = Warning/Attenzione
  - 🔴 Rosso = Errore/Critico
- ✅ **Responsive**: Ottimizzato per mobile e desktop
- ✅ **Export dati**: Esporta diagnostica completa in JSON

---

## 📊 TAB 1: PANORAMICA

### **Sezione Sistema**
Visualizza informazioni hardware e software:

| Campo | Descrizione | Esempio |
|-------|-------------|---------|
| **Firmware** | Versione firmware attuale | `2.3.3` |
| **Uptime** | Tempo di attività sistema | `1g 5h 23m` |
| **Chip** | Modello processore | `ESP32-D0WDQ6 Rev. 1` |
| **CPU** | Frequenza e core | `160 MHz • 2 core` |

### **Sezione Memoria**
Monitora stato memoria heap:

| Campo | Descrizione | Colore |
|-------|-------------|--------|
| **Heap Libera** | Memoria disponibile | 🟢 > 150KB<br>🟡 100-150KB<br>🔴 < 100KB |
| **Heap Minima** | Minimo storico raggiunto | Info |
| **Frammentazione** | Percentuale frammentazione | 🟢 < 20%<br>🟡 20-40%<br>🔴 > 40% |
| **Max Alloc** | Massimo blocco allocabile | Info |

**Progress Bar**: Visualizzazione grafica memoria libera (verde/giallo/rosso).

### **Sezione WiFi & Rete**
Stato connessione e qualità segnale:

| Campo | Descrizione | Valori |
|-------|-------------|--------|
| **Stato** | Connessione WiFi | ✅ Connesso / ❌ Disconnesso |
| **Qualità Segnale** | Potenza segnale | Eccellente / Buono / Discreto / Scarso |
| **NTP** | Sincronizzazione ora | ✅ Sincronizzato / ⏳ Non sincronizzato |
| **RTC** | Orologio hardware | ✅ Connesso / ❌ Offline |

**Valori RSSI** (intensità segnale):
- 🟢 **Eccellente**: ≥ -50 dBm
- 🟢 **Buono**: -50 to -60 dBm
- 🟡 **Discreto**: -60 to -70 dBm
- 🔴 **Scarso**: < -70 dBm

### **Sezione Temperatura**
Monitoraggio termico ESP32:

| Campo | Descrizione | Soglia |
|-------|-------------|--------|
| **ESP32** | Temperatura processore | Normale < 80°C |
| **Avviso** | Warning temperature | Attivo ≥ 80°C |
| **Protezione** | Thermal protection | Attiva ≥ 85°C |

**Colori**:
- 🟢 Verde: < 80°C (OK)
- 🟡 Giallo: 80-85°C (Warning attivo)
- 🔴 Rosso: ≥ 85°C (Protezione attiva)

### **Sezione Campane**
Stato sistema campane:

| Campo | Descrizione |
|-------|-------------|
| **Stato** | ✅ Abilitate / ⏸️ Disabilitate |
| **Suonate Totali** | Contatore complessivo attivazioni |
| **Ultima Suonata** | Ora ultima attivazione (formato HH:MM:SS) |
| **Scheduler** | ✅ Attivo / ⏸️ Inattivo |

---

## 📅 TAB 2: PROGRAMMAZIONI

Visualizza tutte le configurazioni attive.

### **Tabella Programmazioni Settimanali**
Mostra tutti gli eventi settimanali configurati:

| Colonna | Descrizione |
|---------|-------------|
| **ID** | Identificativo univoco |
| **Nome** | Nome evento |
| **Giorno** | Giorno della settimana (Lun-Dom) |
| **Ora** | Orario attivazione (HH:MM) |
| **Melodia** | Melodia associata (0-9) |
| **Stato** | ✅ Attiva / ⏸️ Disattiva |

### **Tabella Eventi Speciali**
Mostra eventi speciali (festività, matrimoni, ecc.):

| Colonna | Descrizione |
|---------|-------------|
| **ID** | Identificativo univoco |
| **Nome** | Nome evento |
| **Data** | Data evento (GG/MM o GG/MM/AAAA) |
| **Ora** | Orario attivazione (HH:MM) |
| **Melodia** | Melodia associata (0-9) |
| **Ricorrente** | 🔄 Ogni anno / 1️⃣ Una volta |
| **Stato** | ✅ Attivo / ⏸️ Disattivo |

### **Tabella Melodie Configurate**
Lista melodie salvate nel sistema:

| Colonna | Descrizione |
|---------|-------------|
| **ID** | Slot melodia (0-9) |
| **Nome** | Nome melodia |
| **Note** | Numero di note configurate |
| **Durata** | Durata totale in secondi |
| **Stato** | ✅ Sempre attiva |

---

## 📝 TAB 3: LOG EVENTI

Sistema di logging in tempo reale degli eventi di sistema.

### **Tipi di Log**:
- ℹ️ **Info** (blu): Eventi informativi
- ✅ **Success** (verde): Operazioni completate con successo
- ⚠️ **Warning** (giallo): Avvisi e attenzioni
- ❌ **Error** (rosso): Errori di sistema

### **Formato Log**:
```
[🔵 Icon] [HH:MM:SS] Messaggio descrittivo
```

### **Eventi Tracciati Automaticamente**:

| Evento | Tipo | Esempio |
|--------|------|---------|
| Avvio interfaccia | Success | `🚀 Sistema avviato - Interfaccia caricata con successo` |
| Apertura modal | Info | `Modal diagnostica aperto` |
| Cambio tab | Info | `Aperto tab: logs` |
| Campane abilitate | Success | `🔔 Campane ABILITATE - suoneranno automaticamente` |
| Campane disabilitate | Warning | `🔇 Campane DISABILITATE - nessun suono automatico` |
| Test melodia | Success | `Test melodia avviato (12 note)` |
| Melodia salvata | Success | `Melodia "Ave Maria" salvata (45 note)` |
| Programmazione salvata | Success | `Programmazione settimanale salvata (7 eventi)` |
| Eventi salvati | Success | `Eventi speciali salvati (3 eventi)` |
| Errore generico | Error | `❌ Errore caricamento iniziale: Network error` |
| Diagnostica esportata | Success | `Diagnostica esportata in JSON` |
| Log cancellati | Info | `Log cancellati` |

### **Capacità Log**:
- **Massimo 100 eventi** salvati (gli eventi più vecchi vengono automaticamente rimossi)
- **Badge contatore**: Mostra numero totale eventi registrati
- **Ordinamento**: Eventi più recenti in alto (ordine cronologico inverso)

---

## 🛠️ TOOLBAR FUNZIONI

Barra degli strumenti in alto nel modal:

| Pulsante | Funzione | Descrizione |
|----------|----------|-------------|
| **📊 Panoramica** | Tab sistema | Mostra metriche sistema in tempo reale |
| **📅 Programmazioni** | Tab programmazioni | Visualizza tutte le configurazioni |
| **📝 Log Eventi** | Tab log | Cronologia eventi di sistema |
| **🔄 Aggiorna** | Refresh manuale | Forza aggiornamento dati immediato |
| **💾 Esporta JSON** | Export dati | Scarica diagnostica completa in JSON |
| **🗑️ Pulisci Log** | Cancella log | Rimuove tutti i log eventi (richiede conferma) |

### **Auto-Refresh**:
- **Intervallo**: Ogni 5 secondi (automatico)
- **Attivo quando**: Modal aperto
- **Disattivato quando**: Modal chiuso (risparmio risorse)

---

## 💾 ESPORTAZIONE DATI

### **Formato Export**:
Cliccando su **"💾 Esporta JSON"** viene scaricato un file JSON con struttura:

```json
{
  "exportTime": "2025-11-09T15:30:45.123Z",
  "firmware": "2.3.3",
  "uptime": "1g 5h 23m",
  "heap": {
    "free": "165.3 KB",
    "min": "142.1 KB",
    "fragmentation": "18.5%"
  },
  "wifi": {
    "status": "✅ Connesso",
    "quality": "Eccellente",
    "rssi": "-45 dBm"
  },
  "temperature": {
    "esp32": "52.3°C",
    "warning": "✅ OK",
    "protection": "✅ OK"
  },
  "bells": {
    "enabled": "✅ Abilitate",
    "total": "156",
    "last": "15:25:30"
  },
  "logs": [
    {
      "time": "15:30:45",
      "type": "success",
      "message": "Diagnostica esportata in JSON",
      "timestamp": 1731167445123
    },
    // ... altri 99 log
  ]
}
```

### **Nome File**:
- **Formato**: `diagnostica-YYYY-MM-DD.json`
- **Esempio**: `diagnostica-2025-11-09.json`

### **Utilizzo Export**:
1. **Backup**: Salvare stato sistema prima di modifiche
2. **Debug**: Inviare diagnostica completa per assistenza
3. **Analisi**: Confrontare metriche tra diverse versioni firmware
4. **Report**: Documentare stato sistema per manutenzione

---

## ⚡ SHORTCUTS

### **Tastiera**:
- **ESC**: Chiude il modal diagnostica
- **F5**: Refresh browser (ricarica pagina)

### **Mouse**:
- **Clic fuori modal**: Chiude il modal
- **Scroll**: Scorrimento contenuto modal

### **Touch** (mobile):
- **Swipe**: Scorrimento contenuto
- **Tap fuori**: Chiude modal

---

## 🎨 INDICATORI VISIVI

### **Colori Stato**:
```
🟢 Verde (#10b981)  = OK / Normale / Connesso
🟡 Giallo (#f59e0b) = Warning / Attenzione
🔴 Rosso (#ef4444)  = Errore / Critico / Disconnesso
🔵 Blu (#3b82f6)    = Info / Neutro
```

### **Badge**:
- **Success**: Sfondo verde, testo verde chiaro
- **Warning**: Sfondo giallo, testo giallo chiaro
- **Error**: Sfondo rosso, testo rosso chiaro

### **Progress Bar**:
- **Verde**: Memoria > 50%
- **Giallo**: Memoria 30-50%
- **Rosso**: Memoria < 30%

---

## 📱 RESPONSIVE DESIGN

### **Desktop** (> 768px):
- Layout a griglia 2-4 colonne
- Modal 95% larghezza (max 1200px)
- Font grande e leggibile

### **Mobile** (≤ 768px):
- Layout singola colonna
- Modal 100% larghezza (fullscreen)
- Toolbar centrato verticale
- Log entry verticale (no flex-row)

---

## 🔧 TROUBLESHOOTING

### **Problema: Modal non si apre**
**Causa**: Errore JavaScript  
**Soluzione**:
1. Apri console browser (F12)
2. Verifica errori JavaScript
3. Refresh pagina (Ctrl+F5)
4. Svuota cache browser

### **Problema: Dati non si aggiornano**
**Causa**: Auto-refresh disattivato o API non risponde  
**Soluzione**:
1. Clicca **"🔄 Aggiorna"** manualmente
2. Verifica connessione WiFi ESP32
3. Controlla endpoint `/api/diagnostics` accessibile:
   ```
   http://<IP_ESP32>/api/diagnostics
   ```

### **Problema: Heap sempre bassa**
**Causa**: Memory leak o utilizzo intensivo  
**Soluzione**:
1. Monitora **"Heap Minima"** per 1 ora
2. Se continua a scendere → possibile memory leak
3. Riavvia ESP32
4. Controlla firmware aggiornato

### **Problema: WiFi "Scarso"**
**Causa**: Distanza router, interferenze  
**Soluzione**:
1. Avvicina ESP32 al router
2. Verifica RSSI: deve essere > -70 dBm
3. Cambia canale WiFi router (evita sovrapposizioni)
4. Usa WiFi 2.4 GHz (no 5 GHz)

### **Problema: Temperatura alta**
**Causa**: Ventilazione insufficiente  
**Soluzione**:
1. Verifica ESP32 non coperto
2. Aggiungi ventilazione (ventola 5V)
3. Riduci frequenza CPU (160 MHz → 80 MHz) se necessario
4. Se protezione attiva → sistema si autolimita automaticamente

---

## 📊 METRICHE OTTIMALI

Valori target per sistema sano:

| Metrica | Target | Warning | Critical |
|---------|--------|---------|----------|
| **Free Heap** | > 150 KB | 100-150 KB | < 100 KB |
| **Frammentazione** | < 20% | 20-40% | > 40% |
| **RSSI WiFi** | > -50 dBm | -60 to -70 | < -70 dBm |
| **Temperatura** | < 70°C | 70-80°C | > 80°C |
| **Uptime** | Giorni+ | - | Crash frequenti |

---

## 🔗 ENDPOINT API

Il modal utilizza questi endpoint:

| Endpoint | Metodo | Descrizione |
|----------|--------|-------------|
| `/api/diagnostics` | GET | Metriche sistema complete (v2.3.2+) |
| `/api/status` | GET | Stato generale sistema |
| `/api/weekly-schedules` | GET | Programmazioni settimanali |
| `/api/special-events` | GET | Eventi speciali |
| `/api/melodies` | GET | Melodie configurate |

**Compatibilità**: Richiede firmware **v2.3.2+** per endpoint `/api/diagnostics`.

---

## ✅ CHECKLIST UTILIZZO

**Controlli Giornalieri**:
- [ ] Heap libera > 150 KB
- [ ] WiFi qualità "Buono" o meglio
- [ ] Temperatura < 70°C
- [ ] Campane abilitate (se richiesto)
- [ ] Nessun log error recente

**Controlli Settimanali**:
- [ ] Heap minima non scende sotto 100 KB
- [ ] Uptime stabile (no riavvii imprevisti)
- [ ] Programmazioni attive corrette
- [ ] Log eventi controllato (ultimi 100)

**Controlli Mensili**:
- [ ] Esporta diagnostica completa (backup)
- [ ] Confronta metriche mese precedente
- [ ] Pulisci log vecchi
- [ ] Verifica aggiornamenti firmware

---

## 📚 RIFERIMENTI

**Documentazione Correlata**:
- `docs/Performance-Optimization-v2.3.2.md` - Ottimizzazioni prestazioni
- `docs/UI-Improvements-v2.3.1b.md` - Miglioramenti interfaccia
- `docs/BugFix-Stability.md` - Fix stabilità sistema

**Versione Documento**: 1.0  
**Ultima Modifica**: 09/11/2025  
**Compatibilità**: Firmware v2.3.2+ / HTML v2.3.3+

---

**Fine Guida Utente** - Pannello Diagnostica Completa
