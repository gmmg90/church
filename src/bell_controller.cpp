#include "include/bell_controller.h"
#include "include/watchdog_manager.h"
#include <esp_task_wdt.h>

// Tag per logging
static const char* TAG = "BellCtrl";

// Definizione dell'array delle melodie
BellMelody melodies[10];

BellController bellController;

// Istanza esterna WatchdogManager (definita in main.cpp)
extern WatchdogManager wdManager;

BellController::BellController() {
    isPlaying = false;
    lastNoteTime = 0;
    currentNoteIndex = 0;
    currentMelodyIndex = 0;
    testMode = false;
    
    // Protezione picchi di tensione (v2.3.7)
    lastRelayActivation = 0;
    powerStabilizationTime = 0;
    softStartEnabled = false;  // Disabilitato per SSR G3MB-202P (switching istantaneo)
                               // Impostare a true se si usano relè meccanici
    
    // Feedback visivo melodia (v2.3.7)
    lastLedBlinkTime = 0;
    ledBlinkState = false;
    relayOffTimer = nullptr;
}

// Stato relè generale (contatti in serie all'alimentazione dei martelli)
static volatile bool masterOn = false;
static volatile uint16_t lastPulseMs = 0;

static void setMaster(bool on) {
    digitalWrite(MASTER_RELAY_PIN, on ? Hardware::MASTER_ON : Hardware::MASTER_OFF);
    masterOn = on;
}

// Timestamp (µs) per verificare nel log la durata reale dell'impulso
static volatile int64_t relayOnUs = 0;
static volatile int64_t relayOffUs = 0;

void BellController::relayOffCallback(void* arg) {
    digitalWrite(RELAY1_PIN, RELAY_OFF);
    digitalWrite(RELAY2_PIN, RELAY_OFF);
    digitalWrite(STATUS_LED_PIN, LOW);
    relayOffUs = esp_timer_get_time();
}

void BellController::begin() {
    LOG_INFO(TAG, "Inizializzazione BellController...");
    
    // Inizializza pin relè
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);
    pinMode(STATUS_LED_PIN, OUTPUT);
    pinMode(MASTER_RELAY_PIN, Hardware::MASTER_PIN_MODE);
    
    // Stato iniziale: relè spenti (livello RELAY_OFF, vedi Hardware::RELAY_ACTIVE_HIGH)
    digitalWrite(RELAY1_PIN, RELAY_OFF);
    digitalWrite(RELAY2_PIN, RELAY_OFF);
    digitalWrite(STATUS_LED_PIN, LOW);
    setMaster(false);

    const esp_timer_create_args_t timerArgs = {
        .callback = &BellController::relayOffCallback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "relay_off",
        .skip_unhandled_events = false
    };
    if (esp_timer_create(&timerArgs, &relayOffTimer) != ESP_OK) {
        LOG_ERROR(TAG, "Creazione timer spegnimento relè fallita");
        relayOffTimer = nullptr;
    }

    LOG_INFO(TAG, "Pin configurati: RELAY1=%d, RELAY2=%d, LED=%d", 
             RELAY1_PIN, RELAY2_PIN, STATUS_LED_PIN);
    
    // Attendi stabilizzazione alimentazione dopo boot (v2.3.7)
    // Previene riavvii da picchi di tensione durante inizializzazione
    LOG_INFO(TAG, "Stabilizzazione alimentazione (500ms)...");
    delay(500);
    powerStabilizationTime = millis();
    
    // Carica melodie predefinite
    loadDefaultMelodies();
    
    LOG_INFO(TAG, "BellController inizializzato con successo");
}

Result BellController::ringBell(uint8_t bellNumber, uint16_t duration) {
    LOG_DEBUG(TAG, "ringBell(campana=%d, durata=%dms)", bellNumber, duration);
    
    // Feed watchdog prima di operazione critica (v2.3.5)
    wdManager.forceFeed();
    
    // Verifica protezione termica
    if (systemStatus.thermalProtection) {
        LOG_ERROR(TAG, "Protezione termica attiva - operazione bloccata");
        return Result(ErrorCode::THERMAL_PROTECTION_ACTIVE);
    }
    
    // Verifica se campane abilitate (eccetto modalità test)
    if (!systemStatus.bellsEnabled && !testMode) {
        LOG_WARN(TAG, "Campane disabilitate (enabled=%d, testMode=%d)", 
                 systemStatus.bellsEnabled, testMode);
        return Result(ErrorCode::BELLS_DISABLED);
    }
    
    // Validazione numero campana
    if (bellNumber < 1 || bellNumber > 2) {
        LOG_ERROR(TAG, "Numero campana non valido: %d (range: 1-2)", bellNumber);
        return Result(ErrorCode::BELL_INVALID_NUMBER);
    }
    
    // Validazione durata
    if (duration < BELL_MIN_PULSE || duration > BELL_MAX_PULSE) {
        LOG_ERROR(TAG, "Durata non valida: %dms (range: %d-%d)", 
                  duration, BELL_MIN_PULSE, BELL_MAX_PULSE);
        return Result(ErrorCode::BELL_INVALID_DURATION);
    }
    
    // Protezione picchi di tensione (v2.3.7)
    if (!canActivateRelay()) {
        LOG_WARN(TAG, "Attivazione relè ritardata per protezione tensione");
        waitForPowerStable();
    }
    
    uint8_t relayPin = (bellNumber == 1) ? RELAY1_PIN : RELAY2_PIN;

    // Chiude il relè generale prima del colpo (a SSR spenti, contatti senza carico)
    // Timestamp aggiornati PRIMA: ringBell può girare nel task web mentre loop() esegue update()
    lastRelayActivation = millis();
    lastPulseMs = duration + Hardware::MASTER_ON_DELAY_MS;
    if (!masterOn) {
        setMaster(true);
        LOG_INFO(TAG, "Relè generale CHIUSO");
        delay(Hardware::MASTER_ON_DELAY_MS);
    }
    
    // Attiva relè con soft-start per ridurre picchi di corrente
    if (softStartEnabled) {
        softStartRelay(relayPin);
    } else {
        digitalWrite(relayPin, RELAY_ON);
    }
    digitalWrite(STATUS_LED_PIN, HIGH);

    // Spegnimento garantito dopo 'duration' ms anche se il loop() è bloccato
    if (relayOffTimer) {
        esp_timer_stop(relayOffTimer);  // ignora ESP_ERR_INVALID_STATE se non attivo
        relayOnUs = esp_timer_get_time();
        esp_timer_start_once(relayOffTimer, (uint64_t)duration * 1000ULL);
    }

    // Aggiorna timestamp ultima attivazione
    lastRelayActivation = millis();
    lastPulseMs = duration;
    
    LOG_INFO(TAG, "Campana %d ATTIVATA (pin %d, durata %dms, soft-start=%d)", 
             bellNumber, relayPin, duration, softStartEnabled);
    
    // Aggiorna statistiche
    systemStatus.lastBellTime = millis();
    systemStatus.totalBellRings++;
    
    // Nota: la disattivazione avviene in update() tramite timer non bloccante
    return Result(ErrorCode::SUCCESS);
}

Result BellController::playMelody(uint8_t melodyIndex) {
    LOG_DEBUG(TAG, "playMelody(melodyIndex=%d)", melodyIndex);
    
    // Validazione indice
    if (melodyIndex >= 10) {
        LOG_ERROR(TAG, "Indice melodia non valido: %d (max 9)", melodyIndex);
        return Result(ErrorCode::MELODY_NOT_FOUND);
    }
    
    // Verifica che la melodia sia attiva
    if (!melodies[melodyIndex].isActive) {
        LOG_ERROR(TAG, "Melodia %d non attiva", melodyIndex);
        return Result(ErrorCode::MELODY_INVALID);
    }
    
    // Verifica che ci siano note
    if (melodies[melodyIndex].noteCount == 0) {
        LOG_ERROR(TAG, "Melodia %d non ha note", melodyIndex);
        return Result(ErrorCode::MELODY_NOTES_EMPTY);
    }
    
    // Verifica protezione termica
    if (systemStatus.thermalProtection) {
        LOG_ERROR(TAG, "Protezione termica attiva - riproduzione bloccata");
        return Result(ErrorCode::THERMAL_PROTECTION_ACTIVE);
    }
    
    // Verifica stato campane (eccetto modalità test)
    if (!systemStatus.bellsEnabled && !testMode) {
        LOG_WARN(TAG, "Campane disabilitate - melodia %d non riprodotta", melodyIndex);
        return Result(ErrorCode::BELLS_DISABLED);
    }
    
    // Ferma eventuale melodia in corso
    if (isPlaying) {
        LOG_INFO(TAG, "Fermando melodia precedente (era: %d)", currentMelodyIndex);
        stopMelody();
    }
    
    // Inizializza riproduzione
    currentMelodyIndex = melodyIndex;
    currentNoteIndex = 0;
    isPlaying = true;
    lastNoteTime = millis();
    
    LOG_INFO(TAG, "AVVIO MELODIA: '%s' (ID: %d, Note: %d)", 
             melodies[melodyIndex].name, melodyIndex, melodies[melodyIndex].noteCount);
    
    // Log dettagliato delle note se in DEBUG
    if (Logger::getLevel() <= Logger::DEBUG) {
        LOG_DEBUG(TAG, "Sequenza note per '%s':", melodies[melodyIndex].name);
        for (int i = 0; i < melodies[melodyIndex].noteCount; i++) {
            const BellNote& note = melodies[melodyIndex].notes[i];
            LOG_DEBUG(TAG, "  [%d] Campana %d: %dms suono + %dms pausa", 
                     i, note.bellNumber, note.duration, note.delay);
        }
    }
    
    systemStatus.activeMelody = melodyIndex;
    return Result(ErrorCode::SUCCESS);
}

Result BellController::stopMelody() {
    LOG_DEBUG(TAG, "stopMelody()");
    
    if (isPlaying) {
        isPlaying = false;
        currentNoteIndex = 0;
        emergencyStop(); // Ferma eventuali campane attive e apre il relè generale
        
        // Se era in modalità test, disattivala automaticamente
        if (testMode) {
            testMode = false;
            LOG_INFO(TAG, "Modalità test disattivata automaticamente");
        }
        
        LOG_INFO(TAG, "Melodia fermata");
        return Result(ErrorCode::SUCCESS);
    }
    
    LOG_DEBUG(TAG, "Nessuna melodia in riproduzione");
    return Result(ErrorCode::SUCCESS);
}

bool BellController::isPlayingMelody() {
    return isPlaying;
}

void BellController::testBell(uint8_t bellNumber) {
    LOG_INFO(TAG, "testBell(bellNumber=%d)", bellNumber);
    bool wasTestMode = testMode;
    testMode = true;
    Result result = ringBell(bellNumber, 500); // Test di 500ms
    testMode = wasTestMode;
    
    if (!result.isSuccess()) {
        LOG_ERROR(TAG, "Test campana %d fallito: %s", bellNumber, result.message.c_str());
    }
}

void BellController::enableTestMode(bool enable) {
    LOG_INFO(TAG, "enableTestMode(enable=%d)", enable);
    testMode = enable;
    LOG_INFO(TAG, "Modalità test %s", enable ? "attivata" : "disattivata");
}

void BellController::update() {
    static uint32_t bellStartTime = 0;
    static uint8_t activeBell = 0;
    static uint16_t bellDuration = 0;
    static bool bellActive = false;
    
    // === WATCHDOG AUTO-FEED (v2.3.5) - WatchdogManager gestisce automaticamente ===
    wdManager.feedIfNeeded(); // Auto-feed se necessario (ogni 100ms)
    
    // Gestione timing per singoli colpi di campana
    // +5ms: il timer hardware ha già spento il relè, qui si aggiorna solo lo stato (e il log è coerente)
    if (bellActive && (millis() - bellStartTime >= (uint32_t)bellDuration + 5)) {
        // Disattiva relè
    digitalWrite(RELAY1_PIN, RELAY_OFF);
    digitalWrite(RELAY2_PIN, RELAY_OFF);
    digitalWrite(STATUS_LED_PIN, LOW);
        bellActive = false;
        Serial.printf("BellController: Campana %d disattivata (impulso reale %lld ms)\n",
                      activeBell, (relayOffUs - relayOnUs) / 1000);
    }
    
    // Gestione melodie
    if (isPlaying && melodies[currentMelodyIndex].isActive) {
        // Feed watchdog esplicito durante melodie lunghe
        wdManager.forceFeed();
        
        
        BellMelody& melody = melodies[currentMelodyIndex];
        
        if (currentNoteIndex < melody.noteCount) {
            BellNote& note = melody.notes[currentNoteIndex];
            
            // È tempo di suonare la prossima nota?
            if (!bellActive) {
                // Attendi il delay prima della prossima nota
                if (millis() - lastNoteTime >= note.delay) {
                    // Inizia nuova nota
                    bellStartTime = millis();
                    activeBell = note.bellNumber;
                    bellDuration = note.duration;
                    bellActive = true;
                    
                    ringBell(note.bellNumber, note.duration);
                    lastNoteTime = millis();
                    currentNoteIndex++;
                    
                    // Yield per prevenire watchdog timeout
                    yield();
                }
            }
        } else if (!bellActive) {
            // Melodia completata (dopo la fine dell'ultimo colpo)
            LOG_INFO(TAG, "Melodia '%s' completata", melody.name);
            stopMelody();
        }
    }

    // Apre il relè generale quando non si suona più (SSR già spenti da tempo)
    if (masterOn && !isPlaying && !bellActive &&
        millis() - lastRelayActivation >= (uint32_t)lastPulseMs + Hardware::MASTER_OFF_DELAY_MS) {
        setMaster(false);
        LOG_INFO(TAG, "Relè generale APERTO");
    }
}

void BellController::emergencyStop() {
    LOG_CRITICAL(TAG, "STOP DI EMERGENZA ATTIVATO!");
    digitalWrite(RELAY1_PIN, RELAY_OFF);
    digitalWrite(RELAY2_PIN, RELAY_OFF);
    digitalWrite(STATUS_LED_PIN, LOW);
    setMaster(false);
    isPlaying = false;
    LOG_INFO(TAG, "Tutti i relè disattivati");
}

void BellController::setEnabled(bool enabled) {
    LOG_INFO(TAG, "setEnabled(enabled=%d)", enabled);
    systemStatus.bellsEnabled = enabled;
    if (!enabled) {
        emergencyStop();
    }
    LOG_INFO(TAG, "Campane %s", enabled ? "ABILITATE" : "DISABILITATE");
}

bool BellController::isEnabled() {
    return systemStatus.bellsEnabled;
}

Result BellController::addMelody(const char* name, BellNote* notes, uint8_t noteCount) {
    LOG_DEBUG(TAG, "addMelody(name=%s, noteCount=%d)", name, noteCount);
    
    // Validazione parametri
    if (!name || !notes || noteCount == 0) {
        LOG_ERROR(TAG, "Parametri non validi per addMelody");
        return Result(ErrorCode::INVALID_PARAMETER);
    }
    
    if (noteCount > MAX_MELODY_STEPS) {
        LOG_WARN(TAG, "Note count %d eccede il massimo %d, verrà troncato", 
                 noteCount, MAX_MELODY_STEPS);
        noteCount = MAX_MELODY_STEPS;
    }
    
    // Trova slot libero
    for (int i = 0; i < 10; i++) {
        if (!melodies[i].isActive) {
            strncpy(melodies[i].name, name, 31);
            melodies[i].name[31] = '\0';
            melodies[i].noteCount = noteCount;
            
            for (int j = 0; j < noteCount; j++) {
                melodies[i].notes[j] = notes[j];
            }
            
            melodies[i].isActive = true;
            LOG_INFO(TAG, "Melodia '%s' aggiunta in slot %d (%d note)", name, i, noteCount);
            return Result(ErrorCode::SUCCESS);
        }
    }
    
    LOG_ERROR(TAG, "Nessuno slot libero per nuova melodia");
    return Result(ErrorCode::MELODY_SLOTS_FULL);
}

Result BellController::deleteMelody(uint8_t index) {
    LOG_DEBUG(TAG, "deleteMelody(index=%d)", index);
    
    if (index >= 10) {
        LOG_ERROR(TAG, "Indice melodia non valido: %d", index);
        return Result(ErrorCode::MELODY_NOT_FOUND);
    }
    
    if (!melodies[index].isActive) {
        LOG_WARN(TAG, "Melodia %d già inattiva", index);
        return Result(ErrorCode::MELODY_NOT_FOUND);
    }
    
    melodies[index].isActive = false;
    melodies[index].noteCount = 0;
    LOG_INFO(TAG, "Melodia slot %d eliminata", index);
    return Result(ErrorCode::SUCCESS);
}

Result BellController::updateMelody(uint8_t index, const char* name, BellNote* notes, uint8_t noteCount) {
    LOG_DEBUG(TAG, "updateMelody(index=%d, name=%s, noteCount=%d)", index, name, noteCount);
    
    if (index >= 10) {
        LOG_ERROR(TAG, "Indice melodia non valido: %d", index);
        return Result(ErrorCode::MELODY_NOT_FOUND);
    }
    
    if (!name || !notes || noteCount == 0) {
        LOG_ERROR(TAG, "Parametri non validi per updateMelody");
        return Result(ErrorCode::INVALID_PARAMETER);
    }
    
    if (noteCount > MAX_MELODY_STEPS) {
        LOG_WARN(TAG, "Note count %d eccede il massimo %d, verrà troncato", 
                 noteCount, MAX_MELODY_STEPS);
        noteCount = MAX_MELODY_STEPS;
    }
    
    melodies[index].isActive = true;
    strncpy(melodies[index].name, name, 31);
    melodies[index].name[31] = '\0';
    melodies[index].noteCount = noteCount;
    
    for (int j = 0; j < noteCount; j++) {
        melodies[index].notes[j] = notes[j];
    }
    
    // Se stiamo suonando proprio questa melodia, ricomincia dall'inizio con la nuova sequenza
    if (isPlaying && currentMelodyIndex == index) {
        currentNoteIndex = 0;
        lastNoteTime = millis();
        LOG_INFO(TAG, "Melodia in riproduzione aggiornata, riavvio sequenza");
    }
    
    LOG_INFO(TAG, "Melodia slot %d aggiornata (%s, %d note)", 
             index, melodies[index].name, noteCount);
    return Result(ErrorCode::SUCCESS);
}

// CHIAMATA MESSA ricavata dalla registrazione dello scampanio (WhatsApp 2026-10-06).
// Due celle da 4 colpi che si alternano:
//   A: C1 . C2 . C1 C2   (pausa)
//   B: C1 . C1 . C1 C2   (pausa)
// Intervalli misurati tra i colpi: 470ms, 440ms, 240ms, poi 700ms prima della cella successiva.
// L'intervallo reale tra due colpi è max(duration precedente, delay successivo):
// la durata impulso (200ms) deve restare sotto l'intervallo più breve (240ms).
// 22 celle (11 coppie A+B) = 88 colpi, circa 40s.
void BellController::buildMassCall(BellNote* notes) {
    static const uint8_t cellA[4] = {1, 2, 1, 2};
    static const uint8_t cellB[4] = {1, 1, 1, 2};
    static const uint16_t delays[4] = {700, 470, 440, 240};  // pausa PRIMA di ciascun colpo
    const uint16_t pulse = 200;

    for (int i = 0; i < MASS_CALL_NOTES; ++i) {
        int cell = i / 4;
        int pos = i % 4;
        uint8_t bell = (cell % 2 == 0) ? cellA[pos] : cellB[pos];
        uint16_t delay = (i == 0) ? 0 : delays[pos];
        notes[i] = {bell, pulse, delay};
    }
}

// Sostituisce la vecchia CHIAMATA MESSA predefinita (100 colpi alternati 1-2, 300/400ms)
// se è ancora quella salvata su FS e non è stata personalizzata. Ritorna true se aggiornata.
bool BellController::migrateLegacyMassCall() {
    BellMelody& m = melodies[1];
    if (!m.isActive || strcmp(m.name, "CHIAMATA MESSA") != 0 || m.noteCount != 100) {
        return false;
    }
    for (int i = 0; i < m.noteCount; ++i) {
        const BellNote& n = m.notes[i];
        if (n.bellNumber != ((i % 2 == 0) ? 1 : 2) || n.duration != 300 || n.delay != 400) {
            return false;  // personalizzata dall'utente: non toccare
        }
    }
    BellNote chiamata[MASS_CALL_NOTES];
    buildMassCall(chiamata);
    updateMelody(1, "CHIAMATA MESSA", chiamata, MASS_CALL_NOTES);
    LOG_INFO(TAG, "CHIAMATA MESSA aggiornata al nuovo scampanio registrato");
    return true;
}

void BellController::loadDefaultMelodies() {
    LOG_INFO(TAG, "Caricamento melodie predefinite...");
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

    // Slot 1 dedicato a CHIAMATA MESSA (se libero)
    if (!melodies[1].isActive) {
        BellNote chiamata[MASS_CALL_NOTES];
        buildMassCall(chiamata);
        updateMelody(1, "CHIAMATA MESSA", chiamata, MASS_CALL_NOTES);
    }

    LOG_INFO(TAG, "Melodie predefinite caricate: FUNERALE (slot 0), CHIAMATA MESSA (slot 1)");
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

// ============================================================================
// PROTEZIONE PICCHI DI TENSIONE (v2.3.7)
// ============================================================================

/**
 * @brief Verifica se è sicuro attivare un relè
 * 
 * Controlla che sia passato tempo sufficiente dall'ultima attivazione
 * e che l'alimentazione sia stabile dopo il boot.
 * 
 * @return true se sicuro attivare, false altrimenti
 */
bool BellController::canActivateRelay() {
    uint32_t now = millis();
    
    // Verifica tempo minimo dopo boot (primi 2 secondi critici)
    if (now - powerStabilizationTime < 2000) {
        LOG_DEBUG(TAG, "Alimentazione ancora in stabilizzazione (%lums da boot)", 
                 now - powerStabilizationTime);
        return false;
    }
    
    // Verifica intervallo minimo tra attivazioni (100ms)
    // Previene attivazioni troppo ravvicinate che causano picchi cumulativi
    if (lastRelayActivation > 0 && (now - lastRelayActivation) < 100) {
        LOG_DEBUG(TAG, "Attivazione troppo ravvicinata (%lums da ultima)", 
                 now - lastRelayActivation);
        return false;
    }
    
    return true;
}

/**
 * @brief Attivazione graduale del relè (soft-start)
 * 
 * Attiva il relè con una sequenza PWM graduale per ridurre
 * il picco di corrente iniziale. Questo previene cali di tensione
 * che possono causare brownout e riavvii dell'ESP32.
 * 
 * @param pin Pin del relè da attivare
 */
void BellController::softStartRelay(uint8_t pin) {
    LOG_DEBUG(TAG, "Soft-start relè pin %d", pin);
    
    // Sequenza di attivazione graduale:
    // - 3 impulsi brevi crescenti per "pre-caricare" il circuito
    // - Riduce picco corrente iniziale del ~40%
    
    // Impulso 1: 2ms ON, 10ms OFF
    digitalWrite(pin, RELAY_ON);
    delayMicroseconds(2000);
    digitalWrite(pin, RELAY_OFF);
    delay(10);
    
    // Impulso 2: 5ms ON, 8ms OFF
    digitalWrite(pin, RELAY_ON);
    delay(5);
    digitalWrite(pin, RELAY_OFF);
    delay(8);
    
    // Impulso 3: 10ms ON, 5ms OFF
    digitalWrite(pin, RELAY_ON);
    delay(10);
    digitalWrite(pin, RELAY_OFF);
    delay(5);
    
    // Attivazione finale continua
    digitalWrite(pin, RELAY_ON);
    
    LOG_DEBUG(TAG, "Soft-start completato, relè attivo");
}

/**
 * @brief Attende stabilizzazione alimentazione
 * 
 * Inserisce un delay per permettere ai condensatori di stabilizzarsi
 * prima di attivare il relè. Usato quando canActivateRelay() ritorna false.
 */
void BellController::waitForPowerStable() {
    uint32_t now = millis();
    uint32_t timeSinceBoot = now - powerStabilizationTime;
    uint32_t timeSinceLastActivation = lastRelayActivation > 0 ? now - lastRelayActivation : 0;
    
    // Calcola delay necessario
    uint32_t delayNeeded = 0;
    
    if (timeSinceBoot < 2000) {
        delayNeeded = 2000 - timeSinceBoot;
        LOG_INFO(TAG, "Attesa stabilizzazione post-boot: %lums", delayNeeded);
    } else if (timeSinceLastActivation < 100) {
        delayNeeded = 100 - timeSinceLastActivation;
        LOG_INFO(TAG, "Attesa intervallo minimo tra relè: %lums", delayNeeded);
    }
    
    if (delayNeeded > 0) {
        delay(delayNeeded);
    }
}
