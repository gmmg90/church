#include "include/bell_controller.h"

// Definizione dell'array delle melodie
BellMelody melodies[10];

BellController bellController;

BellController::BellController() {
    isPlaying = false;
    lastNoteTime = 0;
    currentNoteIndex = 0;
    currentMelodyIndex = 0;
    testMode = false;
}

void BellController::begin() {
    Serial.println("[DEBUG] BellController::begin()");
    // Inizializza pin relè
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);
    
    // Stato iniziale: relè spenti (HIGH = spento per relè con logica invertita)
    digitalWrite(RELAY1_PIN, HIGH);
    digitalWrite(RELAY2_PIN, HIGH);
    digitalWrite(STATUS_LED_PIN, LOW);
    
    Serial.println("BellController: Inizializzato");
    
    // Carica melodie predefinite
    loadDefaultMelodies();
}

void BellController::ringBell(uint8_t bellNumber, uint16_t duration) {
    Serial.printf("[BELL] ringBell(campana=%d, durata=%dms)\n", bellNumber, duration);
    
    if (!systemStatus.bellsEnabled && !testMode) {
        Serial.printf("[BELL] SKIP: Campane disabilitate (enabled=%s, testMode=%s)\n", 
                     systemStatus.bellsEnabled ? "true" : "false", 
                     testMode ? "true" : "false");
        return;
    }
    
    // Validazione parametri
    if (bellNumber < 1 || bellNumber > 2) {
        Serial.printf("[BELL] ERRORE: Numero campana non valido: %d (deve essere 1 o 2)\n", bellNumber);
        return;
    }
    
    if (duration < BELL_MIN_PULSE || duration > BELL_MAX_PULSE) {
        Serial.printf("[BELL] ERRORE: Durata non valida: %dms (range: %d-%d)\n", 
                     duration, BELL_MIN_PULSE, BELL_MAX_PULSE);
        return;
    }
    
    uint8_t relayPin = (bellNumber == 1) ? RELAY1_PIN : RELAY2_PIN;
    
    // Attiva relè (LOW = attivo per logica invertita)
    digitalWrite(relayPin, LOW);
    digitalWrite(STATUS_LED_PIN, HIGH);
    
    Serial.printf("[BELL] *** CAMPANA %d ATTIVATA *** (pin %d -> LOW, durata %dms)\n", 
                 bellNumber, relayPin, duration);
    
    // Aggiorna statistiche
    systemStatus.lastBellTime = millis();
    systemStatus.totalBellRings++;
    
    // Nota: la disattivazione avviene in update() tramite timer non bloccante
}

void BellController::playMelody(uint8_t melodyIndex) {
    Serial.printf("[BELL] BellController::playMelody(melodyIndex=%d) chiamata\n", melodyIndex);
    
    // Validazione indice
    if (melodyIndex >= 10) {
        Serial.printf("[BELL] ERRORE: Indice melodia non valido: %d (max 9)\n", melodyIndex);
        return;
    }
    
    // Verifica che la melodia sia attiva
    if (!melodies[melodyIndex].isActive) {
        Serial.printf("[BELL] ERRORE: Melodia %d non attiva\n", melodyIndex);
        return;
    }
    
    // Verifica che ci siano note
    if (melodies[melodyIndex].noteCount == 0) {
        Serial.printf("[BELL] ERRORE: Melodia %d non ha note (noteCount=0)\n", melodyIndex);
        return;
    }
    
    // Verifica stato campane (eccetto modalità test)
    if (!systemStatus.bellsEnabled && !testMode) {
        Serial.printf("[BELL] AVVISO: Campane disabilitate e non in modalità test. Melodia %d non riprodotta.\n", melodyIndex);
        return;
    }
    
    // Ferma eventuale melodia in corso
    if (isPlaying) {
        Serial.printf("[BELL] Fermando melodia precedente (era: %d)\n", currentMelodyIndex);
        stopMelody();
    }
    
    // Inizializza riproduzione
    currentMelodyIndex = melodyIndex;
    currentNoteIndex = 0;
    isPlaying = true;
    lastNoteTime = millis();
    
    Serial.printf("[BELL] ==> AVVIO MELODIA: '%s' (ID: %d, Note: %d) <==\n", 
                 melodies[melodyIndex].name, melodyIndex, melodies[melodyIndex].noteCount);
    
    // Log delle note per debug
    if (DEBUG_MELODY_PLAYBACK) {
        Serial.printf("[BELL] Sequenza note per '%s':\n", melodies[melodyIndex].name);
        for (int i = 0; i < melodies[melodyIndex].noteCount; i++) {
            const BellNote& note = melodies[melodyIndex].notes[i];
            Serial.printf("  [%d] Campana %d: %dms suono + %dms pausa\n", 
                         i, note.bellNumber, note.duration, note.delay);
        }
    }
    
    systemStatus.activeMelody = melodyIndex;
}

void BellController::stopMelody() {
    Serial.println("[DEBUG] BellController::stopMelody()");
    if (isPlaying) {
        isPlaying = false;
        currentNoteIndex = 0;
        emergencyStop(); // Ferma eventuali campane attive
        
        // Se era in modalità test, disattivala automaticamente
        if (testMode) {
            testMode = false;
            Serial.println("BellController: Modalità test disattivata automaticamente");
        }
        
        Serial.println("BellController: Melodia fermata");
    }
}

bool BellController::isPlayingMelody() {
    return isPlaying;
}

void BellController::testBell(uint8_t bellNumber) {
    Serial.printf("[DEBUG] BellController::testBell(bellNumber=%d)\n", bellNumber);
    bool wasTestMode = testMode;
    testMode = true;
    ringBell(bellNumber, 500); // Test di 500ms
    testMode = wasTestMode;
}

void BellController::enableTestMode(bool enable) {
    Serial.printf("[DEBUG] BellController::enableTestMode(enable=%d)\n", enable);
    testMode = enable;
    Serial.printf("BellController: Modalità test %s\n", enable ? "attivata" : "disattivata");
}

void BellController::update() {
    static uint32_t bellStartTime = 0;
    static uint8_t activeBell = 0;
    static uint16_t bellDuration = 0;
    static bool bellActive = false;
    
    // Gestione timing per singoli colpi di campana
    if (bellActive && (millis() - bellStartTime >= bellDuration)) {
        // Disattiva relè
    digitalWrite(RELAY1_PIN, HIGH);
    digitalWrite(RELAY2_PIN, HIGH);
    digitalWrite(STATUS_LED_PIN, LOW);
        bellActive = false;
        Serial.printf("BellController: Campana %d disattivata\n", activeBell);
    }
    
    // Gestione melodie
    if (isPlaying && melodies[currentMelodyIndex].isActive) {
        BellMelody& melody = melodies[currentMelodyIndex];
        
        if (currentNoteIndex < melody.noteCount) {
            BellNote& note = melody.notes[currentNoteIndex];
            
            // È tempo di suonare la prossima nota?
            if (millis() - lastNoteTime >= (bellActive ? 0 : note.delay)) {
                if (!bellActive) {
                    // Inizia nuova nota
                    bellStartTime = millis();
                    activeBell = note.bellNumber;
                    bellDuration = note.duration;
                    bellActive = true;
                    
                    ringBell(note.bellNumber, note.duration);
                    lastNoteTime = millis();
                    currentNoteIndex++;
                }
            }
        } else {
            // Melodia completata
            Serial.printf("BellController: Melodia '%s' completata\n", melody.name);
            stopMelody();
        }
    }
}

void BellController::emergencyStop() {
    Serial.println("[DEBUG] BellController::emergencyStop()");
    digitalWrite(RELAY1_PIN, HIGH);
    digitalWrite(RELAY2_PIN, HIGH);
    digitalWrite(STATUS_LED_PIN, LOW);
    isPlaying = false;
    Serial.println("BellController: STOP DI EMERGENZA!");
}

void BellController::setEnabled(bool enabled) {
    Serial.printf("[DEBUG] BellController::setEnabled(enabled=%d)\n", enabled);
    systemStatus.bellsEnabled = enabled;
    if (!enabled) {
        emergencyStop();
    }
    Serial.printf("BellController: Campane %s\n", enabled ? "abilitate" : "disabilitate");
}

bool BellController::isEnabled() {
    return systemStatus.bellsEnabled;
}

bool BellController::addMelody(const char* name, BellNote* notes, uint8_t noteCount) {
    Serial.printf("[DEBUG] BellController::addMelody(name=%s, noteCount=%d)\n", name, noteCount);
    // Trova slot libero
    for (int i = 0; i < 10; i++) {
        if (!melodies[i].isActive) {
            strncpy(melodies[i].name, name, 31);
            melodies[i].name[31] = '\0';
            melodies[i].noteCount = min(noteCount, (uint8_t)MAX_MELODY_STEPS);
            
            for (int j = 0; j < melodies[i].noteCount; j++) {
                melodies[i].notes[j] = notes[j];
            }
            
            melodies[i].isActive = true;
            Serial.printf("BellController: Melodia '%s' aggiunta (slot %d)\n", name, i);
            return true;
        }
    }
    
    Serial.println("BellController: Nessuno slot libero per nuova melodia");
    return false;
}

bool BellController::deleteMelody(uint8_t index) {
    Serial.printf("[DEBUG] BellController::deleteMelody(index=%d)\n", index);
    if (index < 10 && melodies[index].isActive) {
        melodies[index].isActive = false;
        melodies[index].noteCount = 0;
        Serial.printf("BellController: Melodia slot %d eliminata\n", index);
        return true;
    }
    return false;
}

bool BellController::updateMelody(uint8_t index, const char* name, BellNote* notes, uint8_t noteCount) {
    Serial.printf("[DEBUG] BellController::updateMelody(index=%d, name=%s, noteCount=%d)\n", index, name, noteCount);
    if (index >= 10) return false;
    melodies[index].isActive = true;
    strncpy(melodies[index].name, name ? name : "Senza nome", 31);
    melodies[index].name[31] = '\0';
    melodies[index].noteCount = min(noteCount, (uint8_t)MAX_MELODY_STEPS);
    for (int j = 0; j < melodies[index].noteCount; j++) {
        melodies[index].notes[j] = notes[j];
    }
    // Se stiamo suonando proprio questa melodia, ricomincia dall'inizio con la nuova sequenza
    if (isPlaying && currentMelodyIndex == index) {
        currentNoteIndex = 0;
        lastNoteTime = millis();
    }
    Serial.printf("BellController: Melodia slot %d aggiornata (%s, %d note)\n", index, melodies[index].name, melodies[index].noteCount);
    return true;
}

void BellController::loadDefaultMelodies() {
    Serial.println("[DEBUG] BellController::loadDefaultMelodies()");
    // Predefinita: FUNERALE
    // Pattern: 3 colpi Campana1, poi 3 colpi Campana2, ripetuto per 10 terzine (tot 30 colpi)
    // ogni colpo: durata 300ms, pausa 2700ms
    BellNote funerale[30];
    for (int block = 0; block < 5; ++block) {
        int base = block * 6;
        funerale[base + 0] = {1, 300, 2700};
        funerale[base + 1] = {1, 300, 2700};
        funerale[base + 2] = {1, 300, 2700};
        funerale[base + 3] = {2, 300, 2700};
        funerale[base + 4] = {2, 300, 2700};
        funerale[base + 5] = {2, 300, 2700};
    }
    // Slot 0 dedicato a FUNERALE (se libero)
    if (!melodies[0].isActive) {
        updateMelody(0, "FUNERALE", funerale, 30);
    }

    // Predefinita: CHIAMATA MESSA (alternanza C1 e C2)
    // Ogni colpo: 300ms di suono + 400ms di pausa (scampanio fitto)
    // Con scheduler attuale, l'intervallo tra colpi = max(duration, delay) = 400ms.
    // Per ~40s totali servono ~100 colpi => 50 cicli (2 colpi per ciclo).
    const int cycles = 50;                     // 50 cicli -> 100 colpi ~ 40s
    const int totalNotes = cycles * 2;         // alternanza 1-2 per ciclo
    BellNote chiamata[totalNotes];
    for (int i = 0; i < totalNotes; ++i) {
        uint8_t bell = (i % 2 == 0) ? 1 : 2;   // alternanza 1,2,1,2...
        chiamata[i] = {bell, 300, 400};
    }
    // Slot 1 dedicato a CHIAMATA MESSA (se libero)
    if (!melodies[1].isActive) {
        updateMelody(1, "CHIAMATA MESSA", chiamata, totalNotes);
    }

    Serial.println("BellController: Melodie predefinite caricate (FUNERALE, CHIAMATA MESSA)");
}

String BellController::getStatusJson() {
    String json = "{";
    json += "\"isPlaying\":" + String(isPlaying ? "true" : "false") + ",";
    json += "\"enabled\":" + String(systemStatus.bellsEnabled ? "true" : "false") + ",";
    json += "\"activeMelody\":" + String(systemStatus.activeMelody) + ",";
    json += "\"totalRings\":" + String(systemStatus.totalBellRings) + ",";
    json += "\"lastRingTime\":" + String(systemStatus.lastBellTime) + ",";
    json += "\"testMode\":" + String(testMode ? "true" : "false");
    json += "}";
    return json;
}

// Metodi getter per API
int BellController::getMelodyCount() {
    int count = 0;
    for (int i = 0; i < 10; i++) {
        if (melodies[i].isActive) {
            count++;
        }
    }
    return count;
}

const char* BellController::getMelodyName(uint8_t index) {
    if (index < 10 && melodies[index].isActive) {
        return melodies[index].name;
    }
    return "Unknown";
}

uint32_t BellController::getMelodyDuration(uint8_t index) {
    if (index >= 10 || !melodies[index].isActive) {
        return 0;
    }
    
    uint32_t totalDuration = 0;
    for (int i = 0; i < melodies[index].noteCount; i++) {
        totalDuration += melodies[index].notes[i].duration;
    }
    return totalDuration;
}

const BellNote* BellController::getMelodyNotes(uint8_t index) {
    if (index < 10 && melodies[index].isActive) {
        return melodies[index].notes;
    }
    return nullptr;
}

int BellController::getMelodyNoteCount(uint8_t index) {
    if (index < 10 && melodies[index].isActive) {
        return melodies[index].noteCount;
    }
    return 0;
}
