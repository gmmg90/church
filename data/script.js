// Legacy UI script stub — intentionally left blank.
// The new UI is fully contained in index.html with inline JS.
console.info('[UI] Legacy script.js ignored.');
// Variabili globali
let currentTab = 'control';
let systemStatus = {};
let melodies = [];
let weeklySchedules = [];
let specialEvents = [];
let noteCounter = 0;

// Inizializzazione
document.addEventListener('DOMContentLoaded', function() {
    loadSystemStatus();
    loadMelodies();
    loadWeeklySchedules();
    loadSpecialEvents();
    loadSystemInfo(); // Carica informazioni di sistema per tab impostazioni
    refreshRelayStatus(); // Carica stato relè
    updateTime();
    
    // Aggiorna ogni secondo
    setInterval(updateTime, 1000);
    setInterval(loadSystemStatus, 5000);
    setInterval(loadSystemInfo, 30000); // Aggiorna info sistema ogni 30 secondi
    setInterval(refreshRelayStatus, 10000); // Aggiorna stato relè ogni 10 secondi
});

// Gestione tab
function switchTab(tabName) {
    // Nascondi tutti i contenuti
    const contents = document.querySelectorAll('.tab-content');
    contents.forEach(content => content.classList.remove('active'));
    
    // Rimuovi active da tutti i tab
    const tabs = document.querySelectorAll('.tab');
    tabs.forEach(tab => tab.classList.remove('active'));
    
    // Mostra il contenuto selezionato
    document.getElementById('tab-' + tabName).classList.add('active');
    
    // Attiva il tab
    event.target.classList.add('active');
    
    currentTab = tabName;
}

// Aggiornamento tempo
function updateTime() {
    fetch('/api/time')
        .then(response => response.json())
        .then(data => {
            document.getElementById('currentTime').textContent = data.time;
        })
        .catch(error => {
            document.getElementById('currentTime').textContent = '--:--:--';
        });
}

// Caricamento stato sistema
function loadSystemStatus() {
    fetch('/api/status')
        .then(response => {
            if (response.status === 429) {
                // Troppo frequente: salta questo giro
                return null;
            }
            if (!response.ok) {
                throw new Error('HTTP ' + response.status);
            }
            return response.json();
        })
        .then(data => {
            if (data) {
                systemStatus = data;
                updateStatusDisplay();
            }
        })
        .catch(error => console.error('Errore caricamento stato:', error));
}

function updateStatusDisplay() {
    // WiFi Status
    const wifiElement = document.getElementById('statusWifi');
    const wifiStatus = document.getElementById('wifiStatus');
    if (systemStatus.wifiConnected) {
        wifiElement.className = 'status-item online';
        wifiStatus.textContent = 'Connesso';
    } else {
        wifiElement.className = 'status-item offline';
        wifiStatus.textContent = 'Disconnesso';
    }
    
    // RTC Status
    const rtcElement = document.getElementById('statusRTC');
    const rtcStatus = document.getElementById('rtcStatus');
    if (systemStatus.rtcConnected) {
        rtcElement.className = 'status-item online';
        rtcStatus.textContent = 'Connesso';
    } else {
        rtcElement.className = 'status-item offline';
        rtcStatus.textContent = 'Disconnesso';
    }
    
    // Bells Status
    const bellsElement = document.getElementById('statusBells');
    const bellsStatus = document.getElementById('bellsStatus');
    const enableBtn = document.getElementById('enableBellsBtn');
    
    if (systemStatus.bellsEnabled) {
        bellsElement.className = 'status-item online';
        bellsStatus.textContent = 'Abilitate';
        enableBtn.textContent = 'Disabilita Campane';
        enableBtn.className = 'danger';
    } else {
        bellsElement.className = 'status-item offline';
        bellsStatus.textContent = 'Disabilitate';
        enableBtn.textContent = 'Abilita Campane';
        enableBtn.className = 'success';
    }
    
    // Scheduler Status
    const schedulerElement = document.getElementById('statusScheduler');
    const schedulerStatus = document.getElementById('schedulerStatus');
    if (systemStatus.schedulerActive) {
        schedulerElement.className = 'status-item online';
        schedulerStatus.textContent = 'Attivo';
    } else {
        schedulerElement.className = 'status-item warning';
        schedulerStatus.textContent = 'Inattivo';
    }
    
    // Info aggiuntive
    document.getElementById('totalRings').textContent = systemStatus.totalRings || 0;
    if (systemStatus.lastRingTime) {
        const lastRing = new Date(systemStatus.lastRingTime).toLocaleString('it-IT');
        document.getElementById('lastRingInfo').textContent = lastRing;
    }
}

// Controllo campane
function testBell(bellNumber) {
    showNotification('Test campana ' + bellNumber + '...', 'info');
    // Uniforma al backend: usa endpoint test-relay con query
    testRelay(bellNumber, 500);
}

function playSelectedMelody() {
    const melodyIndex = document.getElementById('melodySelect').value;
    if (!melodyIndex) {
        showNotification('Seleziona una melodia', 'warning');
        return;
    }
    
    showNotification('Avvio melodia...', 'info');
    // Invia JSON come atteso dal backend
    fetch('/api/test-melody', { 
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ melodyId: parseInt(melodyIndex) })
        })
        .then(r => r.json())
        .then(data => {
            if (data.success) {
                showNotification('Melodia avviata!', 'success');
            } else {
                showNotification('Errore: ' + (data.message || 'Impossibile avviare la melodia'), 'error');
            }
        })
        .catch(() => showNotification('Errore di comunicazione', 'error'));
}

function stopMelody() {
    // Tenta POST, fallback a GET se non disponibile
    fetch('/api/stop-melody', {method: 'POST'})
        .then(response => response.ok ? response : fetch('/api/stop-melody'))
        .then(response => response.json())
        .then(() => {
            showNotification('Melodia fermata', 'info');
        })
        .catch(() => showNotification('Errore nel fermare la melodia', 'error'));
}

function toggleBells() {
    const newState = !systemStatus.bellsEnabled;
    // Prova POST; se fallisce o non è ok, usa GET fallback
    fetch('/api/toggle-bells', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ enabled: newState })
    })
    .then(response => response.ok ? response : fetch(`/api/toggle-bells?enabled=${newState ? 1 : 0}`))
    .then(response => response.json())
    .then(data => {
        if (data && (data.success === true || data.enabled === newState)) {
            showNotification(newState ? 'Campane abilitate' : 'Campane disabilitate', 'success');
            loadSystemStatus();
        } else {
            showNotification('Impossibile cambiare stato campane', 'error');
        }
    })
    .catch(() => showNotification('Errore di comunicazione', 'error'));
}

function toggleTestMode() {
    fetch('/api/toggle-test-mode', {method: 'POST'})
    .then(response => response.json())
    .then(data => {
        showNotification('Modalità test ' + (data.testMode ? 'attivata' : 'disattivata'), 'info');
    });
}

// Nuove funzioni per melodie rapide e diagnostica relè
function playQuickMelody(melodyId) {
    showNotification('Avvio melodia rapida...', 'info');
    fetch('/api/test-melody', { 
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ melodyId: melodyId })
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                showNotification('Melodia avviata!', 'success');
            } else {
                showNotification('Errore: ' + (data.message || 'Impossibile avviare la melodia'), 'error');
            }
        })
        .catch(() => showNotification('Errore di comunicazione', 'error'));
}

function refreshRelayStatus() {
    fetch('/api/relay-status')
        .then(response => {
            if (response.status === 429) {
                // Troppo frequente: salta questo giro
                return null;
            }
            if (!response.ok) {
                throw new Error('HTTP ' + response.status);
            }
            return response.json();
        })
        .then(data => {
            if (!data) return;
            document.getElementById('relay1Status').textContent = data.relay1_raw === 0 ? 'ON (0V)' : 'OFF (3.3V)';
            document.getElementById('relay2Status').textContent = data.relay2_raw === 0 ? 'ON (0V)' : 'OFF (3.3V)';
            document.getElementById('ledStatus').textContent = data.statusLed_raw === 1 ? 'ON (3.3V)' : 'OFF (0V)';
            document.getElementById('bellsEnabledStatus').textContent = data.enabled ? 'Abilitate' : 'Disabilitate';
            showNotification('Stato relè aggiornato', 'info');
        })
        .catch(() => showNotification('Errore nel leggere stato relè', 'error'));
}

function setRelay(relayNumber, value) {
    const action = value === 0 ? 'ON' : 'OFF';
    fetch(`/api/set-relay?relay=${relayNumber}&value=${value}`)
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                showNotification(`Relè ${relayNumber} impostato ${action}`, 'success');
                // Aggiorna automaticamente lo stato dopo il comando
                setTimeout(refreshRelayStatus, 500);
            } else {
                showNotification('Errore nell\'impostare il relè', 'error');
            }
        })
        .catch(() => showNotification('Errore di comunicazione', 'error'));
}

function testRelay(relayNumber, duration) {
    fetch(`/api/test-relay?relay=${relayNumber}&duration=${duration || 500}`)
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                showNotification(`Test relè ${relayNumber} completato`, 'success');
            } else {
                showNotification('Errore nel test relè', 'error');
            }
        })
        .catch(() => showNotification('Errore di comunicazione', 'error'));
}

function emergencyStop() {
    if (confirm('Sei sicuro di voler fermare tutte le campane?')) {
        fetch('/api/emergency-stop', {method: 'POST'})
        .then(() => {
            showNotification('STOP DI EMERGENZA ATTIVATO!', 'error');
        });
    }
}

// Gestione melodie
function loadMelodies() {
    console.log('🎵 Caricamento melodie...');
    fetch('/api/melodies')
        .then(response => {
            console.log('📡 Risposta ricevuta:', response.status);
            return response.json();
        })
        .then(data => {
            console.log('📊 Dati melodie ricevuti:', data);
            // Supporta diversi formati di risposta
            if (data && Array.isArray(data.melodies)) {
                melodies = data.melodies;
            } else if (Array.isArray(data)) {
                // Già un array
                melodies = data;
            } else if (data && Array.isArray(data[0])) {
                // Array annidato
                melodies = data[0];
            } else {
                melodies = [];
            }
            console.log('📝 Array melodie aggiornato:', melodies.length, 'elementi');
            updateMelodySelects();
            updateMelodyList();
        })
        .catch(error => {
            console.error('❌ Errore caricamento melodie:', error);
        });
}

function updateMelodySelects() {
    console.log('🔄 Aggiornamento menu melodie...');
    const selects = ['melodySelect', 'weeklyMelody', 'eventMelody'];
    
    selects.forEach(selectId => {
        const select = document.getElementById(selectId);
        if (!select) {
            console.warn('⚠️ Elemento non trovato:', selectId);
            return;
        }
        
        select.innerHTML = '<option value="">Seleziona melodia...</option>';
        melodies.forEach((melody, idx) => {
            console.log('🎼 Controllando melodia:', melody);
            if (melody.isActive) {
                const option = document.createElement('option');
                const id = (melody.id !== undefined ? melody.id : (melody.index !== undefined ? melody.index : idx));
                option.value = id;
                option.textContent = melody.name;
                select.appendChild(option);
                console.log('✅ Melodia aggiunta al menu:', melody.name, 'id:', id);
            } else {
                console.log('⏭️ Melodia inattiva saltata:', melody.name);
            }
        });
    });
    console.log('✅ Menu melodie aggiornati');
}

function updateMelodyList() {
    console.log('🔄 Aggiornamento lista melodie...', melodies);
    const list = document.getElementById('melodyList');
    list.innerHTML = '';
    
    if (!Array.isArray(melodies) || melodies.length === 0) {
        list.innerHTML = '<div class="empty-state">Nessuna melodia disponibile</div>';
        return;
    }
    
    melodies.forEach((melody, index) => {
        console.log('🎵 Processing melody:', melody);
        const item = document.createElement('div');
        item.className = 'melody-item';
        item.innerHTML = `
            <div class="item-info">
                <div class="item-title">${melody.name || 'Melodia senza nome'}</div>
                <div class="item-details">${melody.noteCount || 0} note - Durata: ${(melody.duration / 1000).toFixed(1)}s</div>
            </div>
            <div class="item-actions">
                <button onclick="playMelody(${melody.id !== undefined ? melody.id : index})" title="Testa melodia">▶️ Test</button>
                <button onclick="editMelody(${melody.id !== undefined ? melody.id : index})" title="Modifica">✏️</button>
                <button class="danger" onclick="deleteMelody(${melody.id !== undefined ? melody.id : index})" title="Elimina">🗑️</button>
            </div>
        `;
        list.appendChild(item);
    });
}

function calculateMelodyDuration(melody) {
    let totalDuration = 0;
    melody.notes.forEach(note => {
        totalDuration += note.duration + note.delay;
    });
    return (totalDuration / 1000).toFixed(1);
}

function playMelody(index) {
    console.log('🎵 Testing melody with index:', index);
    // Invia JSON come atteso dal backend
    fetch('/api/test-melody', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ melodyId: index })
    })
    .then(response => response.json())
    .then(data => {
        console.log('📡 Test melody response:', data);
        showNotification(data.success ? data.message || 'Melodia avviata' : 'Errore: ' + data.message, 
                        data.success ? 'success' : 'error');
    })
    .catch(error => {
        console.error('❌ Errore test melodia:', error);
        showNotification('Errore di comunicazione', 'error');
    });
}

function deleteMelody(index) {
    if (confirm('Sei sicuro di voler eliminare questa melodia?')) {
        fetch('/api/delete-melody', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({index: index})
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                showNotification('Melodia eliminata', 'success');
                loadMelodies();
            }
        });
    }
}

// Editor melodie
function addMelodyNote() {
    noteCounter++;
    const notesContainer = document.getElementById('melodyNotes');
    
    const noteRow = document.createElement('div');
    noteRow.className = 'note-row';
    noteRow.innerHTML = `
        <div>${noteCounter}</div>
        <select class="note-bell">
            <option value="1">Campana 1</option>
            <option value="2">Campana 2</option>
        </select>
        <input type="number" class="note-duration" min="100" max="2000" value="500" placeholder="Durata">
        <input type="number" class="note-delay" min="50" max="5000" value="1000" placeholder="Pausa">
        <button class="danger" onclick="removeNote(this)">❌</button>
    `;
    
    notesContainer.appendChild(noteRow);
}

function removeNote(button) {
    button.parentElement.remove();
}

function saveMelody() {
    const name = document.getElementById('melodyName').value;
    if (!name) {
        showNotification('Inserisci il nome della melodia', 'warning');
        return;
    }
    
    const noteRows = document.querySelectorAll('#melodyNotes .note-row');
    const notes = [];
    
    noteRows.forEach(row => {
        const bell = parseInt(row.querySelector('.note-bell').value);
        const duration = parseInt(row.querySelector('.note-duration').value);
        const delay = parseInt(row.querySelector('.note-delay').value);
        
        notes.push({bellNumber: bell, duration: duration, delay: delay});
    });
    
    if (notes.length === 0) {
        showNotification('Aggiungi almeno una nota', 'warning');
        return;
    }
    
    fetch('/api/save-melody', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({name: name, notes: notes})
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showNotification('Melodia salvata!', 'success');
            document.getElementById('melodyName').value = '';
            document.getElementById('melodyNotes').innerHTML = '';
            noteCounter = 0;
            loadMelodies();
        } else {
            showNotification('Errore nel salvataggio: ' + data.message, 'error');
        }
    });
}

function testCurrentMelody() {
    const noteRows = document.querySelectorAll('#melodyNotes .note-row');
    const notes = [];
    
    noteRows.forEach(row => {
        const bell = parseInt(row.querySelector('.note-bell').value);
        const duration = parseInt(row.querySelector('.note-duration').value);
        const delay = parseInt(row.querySelector('.note-delay').value);
        
        notes.push({bellNumber: bell, duration: duration, delay: delay});
    });
    
    if (notes.length === 0) {
        showNotification('Aggiungi almeno una nota per il test', 'warning');
        return;
    }
    
    fetch('/api/test-melody', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({notes: notes})
    })
    .then(response => response.json())
    .then(data => {
        showNotification('Test melodia avviato', 'info');
    });
}

// Programmazione settimanale
function loadWeeklySchedules() {
    fetch('/api/weekly-schedules')
        .then(response => response.json())
        .then(data => {
            weeklySchedules = (data && Array.isArray(data.schedules)) ? data.schedules : [];
            updateWeeklyScheduleList();
        })
        .catch(error => {
            console.warn('⚠️ Impossibile caricare programmazioni settimanali:', error);
            weeklySchedules = [];
            updateWeeklyScheduleList();
        });
}

function updateWeeklyScheduleList() {
    const list = document.getElementById('weeklyScheduleList');
    list.innerHTML = '';
    
    const days = ['Dom', 'Lun', 'Mar', 'Mer', 'Gio', 'Ven', 'Sab'];
    
    weeklySchedules.forEach(schedule => {
        // Validazione dei dati per evitare errori
        if (!schedule || schedule.hour === undefined || schedule.minute === undefined) {
            console.warn('⚠️ Dati programmazione settimanale non validi:', schedule);
            return;
        }
        
        const item = document.createElement('div');
        item.className = 'schedule-item' + (schedule.isActive ? '' : ' inactive');
        
        const melodyName = melodies[schedule.melodyIndex]?.name || 'Melodia non trovata';
        
        item.innerHTML = `
            <div class="item-info">
                <div class="item-title">${schedule.name || 'Senza nome'}</div>
                <div class="item-details">
                    ${days[schedule.dayOfWeek] || '?'} alle ${String(schedule.hour).padStart(2,'0')}:${String(schedule.minute).padStart(2,'0')} - 
                    Melodia: ${melodyName}
                </div>
            </div>
            <div class="item-actions">
                <button class="${schedule.isActive ? 'warning' : 'success'}" 
                        onclick="toggleWeeklySchedule(${schedule.id})">
                    ${schedule.isActive ? '⏸️' : '▶️'}
                </button>
                <button onclick="editWeeklySchedule(${schedule.id})">✏️</button>
                <button class="danger" onclick="deleteWeeklySchedule(${schedule.id})">🗑️</button>
            </div>
        `;
        list.appendChild(item);
    });
}

function addWeeklySchedule() {
    const name = document.getElementById('weeklyName').value;
    const day = parseInt(document.getElementById('weeklyDay').value);
    const time = document.getElementById('weeklyTime').value;
    const melodyIndex = parseInt(document.getElementById('weeklyMelody').value);
    
    if (!name || !time || isNaN(melodyIndex)) {
        showNotification('Compila tutti i campi', 'warning');
        return;
    }
    
    const [hour, minute] = time.split(':').map(Number);
    
    fetch('/api/add-weekly-schedule', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
            name: name,
            dayOfWeek: day,
            hour: hour,
            minute: minute,
            melodyIndex: melodyIndex
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showNotification('Programmazione aggiunta!', 'success');
            document.getElementById('weeklyName').value = '';
            document.getElementById('weeklyTime').value = '';
            loadWeeklySchedules();
        } else {
            showNotification('Errore: ' + data.message, 'error');
        }
    });
}

function deleteWeeklySchedule(id) {
    if (confirm('Eliminare questa programmazione?')) {
        fetch('/api/delete-weekly-schedule', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({id: id})
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                showNotification('Programmazione eliminata', 'success');
                loadWeeklySchedules();
            }
        });
    }
}

function toggleWeeklySchedule(id) {
    fetch('/api/toggle-weekly-schedule', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({id: id})
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showNotification('Programmazione ' + (data.active ? 'attivata' : 'disattivata'), 'success');
            loadWeeklySchedules();
        }
    });
}

// Funzioni per salvare modifiche programmazioni settimanali
function collectWeeklySchedules() {
    const schedules = [];
    document.querySelectorAll('#weeklyContainer .schedule-item').forEach((item, index) => {
        const schedule = { ...weeklySchedules[index] };
        
        item.querySelectorAll('[data-field]').forEach(input => {
            const field = input.getAttribute('data-field');
            if (field === 'time') {
                const [hour, minute] = input.value.split(':');
                schedule.hour = parseInt(hour) || 0;
                schedule.minute = parseInt(minute) || 0;
            } else if (field === 'dayOfWeek') {
                // Conversione esplicita per dayOfWeek da stringa a numero
                schedule[field] = parseInt(input.value) || 0;
            } else if (field === 'melodyIndex') {
                // Conversione esplicita per melodyIndex da stringa a numero
                schedule[field] = parseInt(input.value) || 0;
            } else if (input.type === 'checkbox') {
                schedule[field] = input.checked;
            } else if (input.type === 'number') {
                schedule[field] = parseInt(input.value) || 0;
            } else {
                schedule[field] = input.value;
            }
        });
        
        schedules.push(schedule);
    });
    return schedules;
}

function collectSpecialEvents() {
    const events = [];
    document.querySelectorAll('#specialContainer .schedule-item').forEach((item, index) => {
        const event = { ...specialEvents[index] };
        
        item.querySelectorAll('[data-field]').forEach(input => {
            const field = input.getAttribute('data-field');
            if (field === 'date') {
                const [year, month, day] = input.value.split('-');
                event.year = parseInt(year) || 2025;
                event.month = parseInt(month) || 1;
                event.day = parseInt(day) || 1;
            } else if (field === 'time') {
                const [hour, minute] = input.value.split(':');
                event.hour = parseInt(hour) || 0;
                event.minute = parseInt(minute) || 0;
            } else if (field === 'melodyIndex') {
                // Conversione esplicita per melodyIndex da stringa a numero
                event[field] = parseInt(input.value) || 0;
            } else if (input.type === 'checkbox') {
                event[field] = input.checked;
            } else if (input.type === 'number') {
                event[field] = parseInt(input.value) || 0;
            } else {
                event[field] = input.value;
            }
        });
        
        events.push(event);
    });
    return events;
}

function saveWeeklySchedules() {
    const schedules = collectWeeklySchedules();
    fetch('/api/weekly-schedules', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({schedules: schedules})
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showNotification('Programmazioni settimanali salvate!', 'success');
            weeklySchedules = schedules;
            loadWeeklySchedules();
        } else {
            showNotification('Errore salvataggio: ' + (data.message || ''), 'error');
        }
    })
    .catch(error => {
        showNotification('Errore di comunicazione', 'error');
        console.error('Error saving weekly schedules:', error);
    });
}

function saveSpecialEvents() {
    const events = collectSpecialEvents();
    fetch('/api/special-events', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({events: events})
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showNotification('Eventi speciali salvati!', 'success');
            specialEvents = events;
            loadSpecialEvents();
        } else {
            showNotification('Errore salvataggio: ' + (data.message || ''), 'error');
        }
    })
    .catch(error => {
        showNotification('Errore di comunicazione', 'error');
        console.error('Error saving special events:', error);
    });
}

// Eventi speciali
function loadSpecialEvents() {
    fetch('/api/special-events')
        .then(response => response.json())
        .then(data => {
            specialEvents = (data && Array.isArray(data.events)) ? data.events : [];
            updateSpecialEventsList();
        })
        .catch(error => {
            console.warn('⚠️ Impossibile caricare eventi speciali:', error);
            specialEvents = [];
            updateSpecialEventsList();
        });
}

function updateSpecialEventsList() {
    const list = document.getElementById('specialEventsList');
    list.innerHTML = '';
    
    const eventTypes = ['Messa', 'Angelus', 'Matrimonio', 'Funerale', 'Festa', 'Personalizzato'];
    
    specialEvents.forEach(event => {
        // Validazione dei dati per evitare errori
        if (!event || event.day === undefined || event.month === undefined || event.hour === undefined || event.minute === undefined) {
            console.warn('⚠️ Dati evento speciale non validi:', event);
            return;
        }
        
        const item = document.createElement('div');
        item.className = 'schedule-item' + (event.isActive ? '' : ' inactive');
        
        const melodyName = melodies[event.melodyIndex]?.name || 'Melodia non trovata';
        const eventDate = `${String(event.day).padStart(2,'0')}/${String(event.month).padStart(2,'0')}/${event.year || '?'}`;
        const eventTime = `${String(event.hour).padStart(2,'0')}:${String(event.minute).padStart(2,'0')}`;
        
        item.innerHTML = `
            <div class="item-info">
                <div class="item-title">${event.name || 'Senza nome'}</div>
                <div class="item-details">
                    ${eventTypes[event.type] || 'Tipo sconosciuto'} - ${eventDate} alle ${eventTime}
                    ${event.isRecurring ? '(annuale)' : ''}
                    <br>Melodia: ${melodyName}
                </div>
            </div>
            <div class="item-actions">
                <button class="${event.isActive ? 'warning' : 'success'}" 
                        onclick="toggleSpecialEvent(${event.id})">
                    ${event.isActive ? '⏸️' : '▶️'}
                </button>
                <button onclick="editSpecialEvent(${event.id})">✏️</button>
                <button class="danger" onclick="deleteSpecialEvent(${event.id})">🗑️</button>
            </div>
        `;
        list.appendChild(item);
    });
}

function addSpecialEvent() {
    const name = document.getElementById('eventName').value;
    const type = parseInt(document.getElementById('eventType').value);
    const date = document.getElementById('eventDate').value;
    const time = document.getElementById('eventTime').value;
    const melodyIndex = parseInt(document.getElementById('eventMelody').value);
    const recurring = document.getElementById('eventRecurring').checked;
    
    if (!name || !date || !time || isNaN(melodyIndex)) {
        showNotification('Compila tutti i campi', 'warning');
        return;
    }
    
    const [year, month, day] = date.split('-').map(Number);
    const [hour, minute] = time.split(':').map(Number);
    
    fetch('/api/add-special-event', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
            name: name,
            type: type,
            year: year,
            month: month,
            day: day,
            hour: hour,
            minute: minute,
            melodyIndex: melodyIndex,
            isRecurring: recurring
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showNotification('Evento aggiunto!', 'success');
            // Reset form
            document.getElementById('eventName').value = '';
            document.getElementById('eventDate').value = '';
            document.getElementById('eventTime').value = '';
            document.getElementById('eventRecurring').checked = false;
            loadSpecialEvents();
        } else {
            showNotification('Errore: ' + data.message, 'error');
        }
    });
}

function deleteSpecialEvent(id) {
    if (confirm('Eliminare questo evento?')) {
        fetch('/api/delete-special-event', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({id: id})
        })
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                showNotification('Evento eliminato', 'success');
                loadSpecialEvents();
            }
        });
    }
}

function toggleSpecialEvent(id) {
    fetch('/api/toggle-special-event', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({id: id})
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showNotification('Evento ' + (data.active ? 'attivato' : 'disattivato'), 'success');
            loadSpecialEvents();
        }
    });
}

// Funzioni per test relè (aggiunte)
function testRelay(relay, duration = 500) {
    appendLog(`Chiamata test-relay relay=${relay} duration=${duration}ms`);
    fetch(`/api/test-relay?relay=${encodeURIComponent(relay)}&duration=${encodeURIComponent(duration)}`)
        .then(response => response.json())
        .then(data => {
            if (data.success) {
                appendLog(`Relè ${relay} attivato per ${duration}ms`);
                showNotification(`Relè ${relay} attivato`, 'success');
                loadSystemStatus();
            } else {
                appendLog(`Errore test-relay: ${data.message || 'unknown'}`);
                showNotification('Errore test-relay', 'error');
            }
        })
        .catch(err => {
            appendLog('Errore comunicazione test-relay: ' + err);
            showNotification('Errore di comunicazione', 'error');
        });
}

function testRelayCustom() {
    const duration = parseInt(document.getElementById('duration')?.value || '500');
    // Scegli il relay 1 di default
    testRelay(1, isNaN(duration) ? 500 : duration);
}

function appendLog(msg) {
    const el = document.getElementById('log');
    if (!el) return;
    const t = new Date().toLocaleTimeString();
    el.textContent = `${t} - ${msg}\n` + el.textContent;
}

// Utility functions
function showNotification(message, type) {
    const notification = document.createElement('div');
    notification.className = `notification ${type}`;
    notification.textContent = message;
    
    document.body.appendChild(notification);
    
    setTimeout(() => notification.classList.add('show'), 100);
    
    setTimeout(() => {
        notification.classList.remove('show');
        setTimeout(() => notification.remove(), 300);
    }, 3000);
}

function changePassword() {
    const currentPassword = document.getElementById('adminPassword').value;
    const newPassword = document.getElementById('newPassword').value;
    
    if (!currentPassword || !newPassword) {
        showNotification('Inserisci entrambe le password', 'warning');
        return;
    }
    
    fetch('/api/change-password', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify({
            currentPassword: currentPassword,
            newPassword: newPassword
        })
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showNotification('Password cambiata con successo!', 'success');
            document.getElementById('adminPassword').value = '';
            document.getElementById('newPassword').value = '';
        } else {
            showNotification('Errore: ' + data.message, 'error');
        }
    });
}

function updateSystemTime() {
    const systemDate = document.getElementById('systemDate').value;
    const systemTime = document.getElementById('systemTime').value;
    
    if (!systemDate || !systemTime) {
        showNotification('Seleziona data e ora complete', 'warning');
        return;
    }
    
    // Combina data e ora
    const datetime = new Date(systemDate + 'T' + systemTime);
    
    // Preferisci ISO 8601, fallback al formato legacy se necessario
    const isoPayload = { dateTime: datetime.toISOString().split('.')[0] };
    fetch('/api/set-time', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(isoPayload)
    })
    .then(response => response.ok ? response : Promise.reject('non-ok'))
    .then(r => r.json())
    .then(data => {
        if (data && data.success) {
            showNotification('⏰ Orario aggiornato: ' + systemDate + ' ' + systemTime, 'success');
            loadSystemInfo();
        } else {
            throw new Error('Backend ha risposto senza success');
        }
    })
    .catch(() => {
        // Fallback al payload legacy
        const legacy = {
            year: datetime.getFullYear(),
            month: datetime.getMonth() + 1,
            day: datetime.getDate(),
            hour: datetime.getHours(),
            minute: datetime.getMinutes(),
            second: datetime.getSeconds()
        };
        return fetch('/api/set-time', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify(legacy)
        })
        .then(r => r.json())
        .then(data => {
            if (data && data.success) {
                showNotification('⏰ Orario aggiornato: ' + systemDate + ' ' + systemTime, 'success');
                loadSystemInfo();
            } else {
                showNotification('❌ Errore: ' + (data.message || 'Aggiornamento fallito'), 'error');
            }
        })
        .catch(() => showNotification('❌ Errore di comunicazione', 'error'));
    });
}

function configureWiFi() {
    const ssid = document.getElementById('wifiSSID').value.trim();
    const password = document.getElementById('wifiPassword').value;
    
    if (!ssid) {
        showNotification('Inserisci il nome della rete WiFi', 'warning');
        return;
    }
    
    if (password.length < 8) {
        showNotification('La password deve essere di almeno 8 caratteri', 'warning');
        return;
    }
    
    const wifiData = {
        ssid: ssid,
        password: password
    };
    
    showNotification('🔄 Configurazione WiFi in corso...', 'info');
    
    fetch('/api/configure-wifi', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        body: JSON.stringify(wifiData)
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showNotification('✅ WiFi configurato! Sistema in riavvio...', 'success');
            // Pulisci i campi per sicurezza
            document.getElementById('wifiSSID').value = '';
            document.getElementById('wifiPassword').value = '';
            
            // Mostra messaggio di attesa
            setTimeout(() => {
                showNotification('⏳ Attendere il riavvio del sistema (30-60 secondi)', 'info');
            }, 2000);
        } else {
            showNotification('❌ Errore: ' + (data.message || 'Configurazione fallita'), 'error');
        }
    })
    .catch(error => {
        console.error('Errore configurazione WiFi:', error);
        showNotification('❌ Errore di comunicazione', 'error');
    });
}

function selectNetwork(ssid) {
    document.getElementById('wifiSSID').value = ssid;
    showNotification(`📡 Rete selezionata: ${ssid}`, 'info');
}

function configureWiFi() {
    const ssid = document.getElementById('wifiSSID').value;
    const password = document.getElementById('wifiPassword').value;
    
    if (!ssid || !password) {
        showNotification('❌ Inserire SSID e password', 'error');
        return;
    }
    
    showNotification('🔄 Configurazione WiFi in corso...', 'info');
    
    const data = {
        ssid: ssid,
        password: password
    };
    
    fetch('/api/configure-wifi', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(data)
    })
    .then(response => response.json())
    .then(data => {
        if (data.success) {
            showNotification('✅ WiFi configurato! Sistema si riavvia...', 'success');
            // Pulisci i campi per sicurezza
            document.getElementById('wifiSSID').value = '';
            document.getElementById('wifiPassword').value = '';
            
            // Mostra messaggio di attesa
            setTimeout(() => {
                showNotification('⏳ Riavvio in corso, attendere...', 'info');
            }, 2000);
        } else {
            showNotification('❌ Errore configurazione WiFi: ' + (data.message || 'Errore sconosciuto'), 'error');
        }
    })
    .catch(error => {
        console.error('Errore configurazione WiFi:', error);
        showNotification('❌ Errore di comunicazione', 'error');
    });
}

function loadSystemInfo() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            // Aggiorna modalità rete
            const networkMode = document.getElementById('networkMode');
            const systemIP = document.getElementById('systemIP');
            const timezoneInfo = document.getElementById('timezoneInfo');
            const ntpSync = document.getElementById('ntpSync');
            
            if (data.apMode) {
                networkMode.textContent = '📡 Access Point (Backup)';
                networkMode.style.color = '#ed8936';
                systemIP.textContent = data.apIP || 'N/A';
            } else if (data.wifiConnected) {
                networkMode.textContent = '🌐 WiFi Connesso';
                networkMode.style.color = '#48bb78';
                systemIP.textContent = data.wifiIP || 'N/A';
            } else {
                networkMode.textContent = '❌ Disconnesso';
                networkMode.style.color = '#f56565';
                systemIP.textContent = 'N/A';
            }
            
            // Fuso orario
            if (data.timezoneDescription) {
                timezoneInfo.textContent = data.timezoneDescription;
                timezoneInfo.style.color = data.isDST ? '#4299e1' : '#38a169';
            } else {
                timezoneInfo.textContent = 'N/A';
            }
            
            // Sincronizzazione NTP
            if (data.ntpSynced) {
                ntpSync.textContent = '✅ Sincronizzato';
                ntpSync.style.color = '#48bb78';
            } else if (data.apMode) {
                ntpSync.textContent = '⚠️ Non disponibile (AP)';
                ntpSync.style.color = '#ed8936';
            } else {
                ntpSync.textContent = '❌ Non sincronizzato';
                ntpSync.style.color = '#f56565';
            }
            
            // Popola i campi di data/ora con l'ora corrente se in modalità AP
            if (data.apMode) {
                populateCurrentDateTime();
            }
        })
        .catch(error => {
            console.error('Errore caricamento info sistema:', error);
        });
}

function populateCurrentDateTime() {
    fetch('/api/time')
        .then(response => response.json())
        .then(data => {
            if (data.date && data.time) {
                // Converte il formato data da DD/MM/YYYY a YYYY-MM-DD
                const dateParts = data.date.split('/');
                if (dateParts.length === 3) {
                    const formattedDate = `${dateParts[2]}-${dateParts[1].padStart(2, '0')}-${dateParts[0].padStart(2, '0')}`;
                    document.getElementById('systemDate').value = formattedDate;
                }
                document.getElementById('systemTime').value = data.time;
            }
        })
        .catch(error => {
            console.error('Errore caricamento ora corrente:', error);
            // Fallback: usa l'ora del browser
            const now = new Date();
            const date = now.toISOString().split('T')[0];
            const time = now.toTimeString().split(' ')[0];
            document.getElementById('systemDate').value = date;
            document.getElementById('systemTime').value = time;
        });
}

function backupData() {
    fetch('/api/backup')
        .then(response => response.blob())
        .then(blob => {
            const url = window.URL.createObjectURL(blob);
            const a = document.createElement('a');
            a.href = url;
            a.download = 'church-bells-backup-' + new Date().toISOString().split('T')[0] + '.json';
            document.body.appendChild(a);
            a.click();
            window.URL.revokeObjectURL(url);
            document.body.removeChild(a);
            showNotification('Backup scaricato!', 'success');
        });
}

function resetAllData() {
    if (confirm('ATTENZIONE: Questo cancellerà tutti i dati! Sei sicuro?')) {
        if (confirm('Questa operazione è irreversibile. Confermi?')) {
            fetch('/api/reset-all', {method: 'POST'})
            .then(response => response.json())
            .then(data => {
                if (data.success) {
                    showNotification('Tutti i dati sono stati cancellati', 'info');
                    location.reload();
                }
            });
        }
    }
}
