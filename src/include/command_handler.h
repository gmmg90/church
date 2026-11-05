#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <Arduino.h>
#include <ArduinoJson.h>

// Forward declarations
class BellController;
struct SystemStatus;

/**
 * CommandHandler — Unification layer per comandi Telegram + HTTP API
 * 
 * Racchiude tutta la logica di business, usata da:
 * - TelegramBotManager (comandi chat)
 * - main.cpp server.on() (API REST)
 * Elimina duplicazione e garantisce comportamento coerente.
 */
class CommandHandler {
public:
    // Risultato operazione
    struct CommandResult {
        bool success;
        String message;
        DynamicJsonDocument* jsonData; // `nullptr` se non applicabile
        
        CommandResult(bool s = false, const String& msg = "")
            : success(s), message(msg), jsonData(nullptr) {}

        CommandResult(const CommandResult&) = delete;
        CommandResult& operator=(const CommandResult&) = delete;

        CommandResult(CommandResult&& other) noexcept
            : success(other.success),
              message(other.message),
              jsonData(other.jsonData) {
            other.jsonData = nullptr;
        }

        CommandResult& operator=(CommandResult&& other) noexcept {
            if (this != &other) {
                if (jsonData) {
                    delete jsonData;
                }
                success = other.success;
                message = other.message;
                jsonData = other.jsonData;
                other.jsonData = nullptr;
            }
            return *this;
        }
        
        ~CommandResult() {
            if (jsonData) delete jsonData;
        }
    };
    
    // Singleton
    static CommandHandler& getInstance();
    
    // Inizializzazione
    void init(BellController* bellController, SystemStatus* systemStatus);
    
    // === COMANDI BASE ===
    CommandResult getStatus();
    CommandResult toggleBells(bool enable);
    CommandResult ringBell(uint8_t bellNumber, uint16_t duration);
    
    // === SCHEDULE ===
    CommandResult getWeeklySchedules();
    CommandResult setWeeklySchedule(const String& jsonPayload);
    CommandResult getSpecialEvents();
    CommandResult setSpecialEvent(const String& jsonPayload);
    
    // === MELODIE ===
    CommandResult getMelodiesList();
    CommandResult getMelodyDetails(uint8_t melodyIndex);
    CommandResult playMelody(uint8_t melodyIndex);
    CommandResult testMelody(const String& jsonPayload);
    CommandResult saveMelody(const String& jsonPayload);
    CommandResult updateMelody(const String& jsonPayload);
    CommandResult deleteMelody(uint8_t melodyIndex);
    CommandResult stopMelody();
    
    // === CONFIGURAZIONE ===
    CommandResult getCurrentTime();
    CommandResult setTime(const String& jsonPayload);
    CommandResult configureWiFi(const String& jsonPayload);
    CommandResult getNetworkConfig();
    CommandResult resyncNTP();
    
    // === DIAGNOSTICA ===
    CommandResult scanI2C();
    CommandResult getBackup();
    CommandResult restoreBackup(const String& jsonPayload);
    
    // === SCAMPANATE SPECIALI ===
    CommandResult playFuneral();
    CommandResult playMass();
    
private:
    CommandHandler();
    ~CommandHandler();
    
    // Singleton instance
    static CommandHandler* _instance;
    
    // Riferimenti ai componenti
    BellController* _bellController;
    SystemStatus* _systemStatus;
    
    // Utility per validazione
    bool validateMelodyIndex(uint8_t index, String& errorMsg) const;
    bool validateDuration(uint16_t duration, String& errorMsg) const;
    bool validateScheduleJson(const JsonDocument& doc, String& errorMsg) const;
};

#endif // COMMAND_HANDLER_H
