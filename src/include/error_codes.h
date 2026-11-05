#ifndef ERROR_CODES_H
#define ERROR_CODES_H

#include <Arduino.h>

/**
 * @brief Codici di errore per tutte le operazioni del sistema
 */
enum class ErrorCode {
    // Successo
    SUCCESS = 0,
    
    // Errori generici (1-99)
    UNKNOWN_ERROR = 1,
    INVALID_PARAMETER = 2,
    TIMEOUT = 3,
    NOT_INITIALIZED = 4,
    ALREADY_INITIALIZED = 5,
    
    // Errori hardware (100-199)
    HARDWARE_FAULT = 100,
    RTC_NOT_FOUND = 101,
    DISPLAY_ERROR = 102,
    RELAY_ERROR = 103,
    I2C_ERROR = 104,
    TEMPERATURE_CRITICAL = 105,
    
    // Errori rete (200-299)
    WIFI_NOT_CONNECTED = 200,
    WIFI_CONNECTION_FAILED = 201,
    NTP_SYNC_FAILED = 202,
    SERVER_ERROR = 203,
    
    // Errori filesystem (300-399)
    FS_MOUNT_FAILED = 300,
    FILE_NOT_FOUND = 301,
    FILE_OPEN_FAILED = 302,
    FILE_WRITE_FAILED = 303,
    FILE_READ_FAILED = 304,
    JSON_PARSE_ERROR = 305,
    
    // Errori melodie (400-499)
    MELODY_NOT_FOUND = 400,
    MELODY_INVALID = 401,
    MELODY_SLOTS_FULL = 402,
    MELODY_ALREADY_PLAYING = 403,
    MELODY_NOTES_EMPTY = 404,
    
    // Errori programmazione (500-599)
    SCHEDULE_NOT_FOUND = 500,
    SCHEDULE_INVALID_TIME = 501,
    SCHEDULE_INVALID_DATE = 502,
    SCHEDULE_FULL = 503,
    
    // Errori campane (600-699)
    BELLS_DISABLED = 600,
    BELL_INVALID_NUMBER = 601,
    BELL_INVALID_DURATION = 602,
    THERMAL_PROTECTION_ACTIVE = 603
};

/**
 * @brief Struttura risultato operazione con codice errore e messaggio
 */
struct Result {
    ErrorCode code;
    String message;
    
    Result() : code(ErrorCode::SUCCESS), message("") {}
    Result(ErrorCode c) : code(c), message(getErrorMessage(c)) {}
    Result(ErrorCode c, const String& msg) : code(c), message(msg) {}
    
    bool isSuccess() const { return code == ErrorCode::SUCCESS; }
    bool isError() const { return code != ErrorCode::SUCCESS; }
    
    operator bool() const { return isSuccess(); }
    
    static const char* getErrorMessage(ErrorCode code) {
        switch (code) {
            case ErrorCode::SUCCESS: return "Success";
            case ErrorCode::UNKNOWN_ERROR: return "Unknown error";
            case ErrorCode::INVALID_PARAMETER: return "Invalid parameter";
            case ErrorCode::TIMEOUT: return "Operation timeout";
            case ErrorCode::NOT_INITIALIZED: return "Component not initialized";
            case ErrorCode::ALREADY_INITIALIZED: return "Already initialized";
            
            case ErrorCode::HARDWARE_FAULT: return "Hardware fault";
            case ErrorCode::RTC_NOT_FOUND: return "RTC not found";
            case ErrorCode::DISPLAY_ERROR: return "Display error";
            case ErrorCode::RELAY_ERROR: return "Relay error";
            case ErrorCode::I2C_ERROR: return "I2C communication error";
            case ErrorCode::TEMPERATURE_CRITICAL: return "Temperature critical";
            
            case ErrorCode::WIFI_NOT_CONNECTED: return "WiFi not connected";
            case ErrorCode::WIFI_CONNECTION_FAILED: return "WiFi connection failed";
            case ErrorCode::NTP_SYNC_FAILED: return "NTP synchronization failed";
            case ErrorCode::SERVER_ERROR: return "Server error";
            
            case ErrorCode::FS_MOUNT_FAILED: return "Filesystem mount failed";
            case ErrorCode::FILE_NOT_FOUND: return "File not found";
            case ErrorCode::FILE_OPEN_FAILED: return "Cannot open file";
            case ErrorCode::FILE_WRITE_FAILED: return "Cannot write file";
            case ErrorCode::FILE_READ_FAILED: return "Cannot read file";
            case ErrorCode::JSON_PARSE_ERROR: return "JSON parse error";
            
            case ErrorCode::MELODY_NOT_FOUND: return "Melody not found";
            case ErrorCode::MELODY_INVALID: return "Melody invalid";
            case ErrorCode::MELODY_SLOTS_FULL: return "No free melody slots";
            case ErrorCode::MELODY_ALREADY_PLAYING: return "Melody already playing";
            case ErrorCode::MELODY_NOTES_EMPTY: return "Melody has no notes";
            
            case ErrorCode::SCHEDULE_NOT_FOUND: return "Schedule not found";
            case ErrorCode::SCHEDULE_INVALID_TIME: return "Invalid time";
            case ErrorCode::SCHEDULE_INVALID_DATE: return "Invalid date";
            case ErrorCode::SCHEDULE_FULL: return "Schedule storage full";
            
            case ErrorCode::BELLS_DISABLED: return "Bells disabled";
            case ErrorCode::BELL_INVALID_NUMBER: return "Invalid bell number";
            case ErrorCode::BELL_INVALID_DURATION: return "Invalid duration";
            case ErrorCode::THERMAL_PROTECTION_ACTIVE: return "Thermal protection active";
            
            default: return "Unknown error code";
        }
    }
};

#endif
