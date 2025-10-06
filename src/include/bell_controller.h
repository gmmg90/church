#ifndef BELL_CONTROLLER_H
#define BELL_CONTROLLER_H

#include "config.h"

class BellController {
private:
    bool isPlaying;
    uint32_t lastNoteTime;
    uint8_t currentNoteIndex;
    uint8_t currentMelodyIndex;
    bool testMode;

public:
    BellController();
    
    // Inizializzazione
    void begin();
    
    // Controllo campane
    void ringBell(uint8_t bellNumber, uint16_t duration);
    void playMelody(uint8_t melodyIndex);
    void stopMelody();
    bool isPlayingMelody();
    
    // Test
    void testBell(uint8_t bellNumber);
    void enableTestMode(bool enable);
    
    // Gestione melodie
    bool addMelody(const char* name, BellNote* notes, uint8_t noteCount);
    bool deleteMelody(uint8_t index);
    bool updateMelody(uint8_t index, const char* name, BellNote* notes, uint8_t noteCount);
    void loadDefaultMelodies();  // Carica melodie predefinite
    
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
