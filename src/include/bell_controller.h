#ifndef BELL_CONTROLLER_H
#define BELL_CONTROLLER_H

#include "config.h"
#include "error_codes.h"
#include "logger.h"
#include <esp_timer.h>

/**
 * @brief Controller per la gestione delle campane
 * 
 * Gestisce l'attivazione dei relè, la riproduzione di melodie
 * e il controllo dello stato delle campane.
 */
class BellController {
private:
    bool isPlaying;
    uint32_t lastNoteTime;
    uint8_t currentNoteIndex;
    uint8_t currentMelodyIndex;
    bool testMode;
    
    // Protezione picchi di tensione (v2.3.7)
    uint32_t lastRelayActivation;     // Timestamp ultima attivazione relè
    uint32_t powerStabilizationTime;  // Tempo per stabilizzare alimentazione
    bool softStartEnabled;             // Abilita soft-start graduale
    
    // Feedback visivo melodia (v2.3.7 - per SSR silenziosi)
    uint32_t lastLedBlinkTime;        // Timestamp ultimo cambio LED
    bool ledBlinkState;                // Stato lampeggio LED durante melodia

    // Spegnimento relè a tempo esatto, indipendente dal loop() (Telegram/HTTPS può bloccarlo per secondi)
    esp_timer_handle_t relayOffTimer;
    static void relayOffCallback(void* arg);

public:
    BellController();
    
    // Inizializzazione
    void begin();
    
    // Controllo campane con gestione errori
    Result ringBell(uint8_t bellNumber, uint16_t duration);
    Result playMelody(uint8_t melodyIndex);
    Result stopMelody();
    bool isPlayingMelody();
    uint8_t getCurrentMelodyIndex() const { return currentMelodyIndex; }
    uint8_t getCurrentNoteIndex() const { return currentNoteIndex; }

    // Test
    void testBell(uint8_t bellNumber);
    void enableTestMode(bool enable);
    
    // Protezione picchi di tensione (v2.3.7)
    bool canActivateRelay();           // Verifica se sicuro attivare relè
    void softStartRelay(uint8_t pin);  // Attivazione graduale relè
    void waitForPowerStable();         // Attende stabilizzazione alimentazione
    
    // Gestione melodie con gestione errori
    Result addMelody(const char* name, BellNote* notes, uint8_t noteCount);
    Result deleteMelody(uint8_t index);
    Result updateMelody(uint8_t index, const char* name, BellNote* notes, uint8_t noteCount);
    void loadDefaultMelodies();  // Carica melodie predefinite
    bool migrateLegacyMassCall();  // Aggiorna la vecchia CHIAMATA MESSA predefinita salvata su FS

    static constexpr int MASS_CALL_NOTES = 88;
    static void buildMassCall(BellNote* notes);
    
    // Getters per API
    int getMelodyCount();
    const char* getMelodyName(uint8_t index);
    uint32_t getMelodyDuration(uint8_t index);
    const BellNote* getMelodyNotes(uint8_t index);
    int getMelodyNoteCount(uint8_t index);
    
    // Update loop
    void update();
    
    // Sicurezza
    void emergencyStop();
    void setEnabled(bool enabled);
    bool isEnabled();
    
    // Status
    String getStatusJson();
};

extern BellController bellController;

#endif
