#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>

/**
 * @brief Sistema di logging avanzato con livelli configurabili
 * 
 * Permette di controllare il verbosity del logging per ridurre
 * l'overhead della seriale e facilitare il debug.
 */
class Logger {
public:
    enum Level {
        DEBUG = 0,   // Dettagli tecnici per debugging
        INFO = 1,    // Informazioni operative normali
        WARN = 2,    // Situazioni anomale ma gestibili
        ERROR = 3,   // Errori che richiedono attenzione
        CRITICAL = 4 // Errori critici di sistema
    };

    /**
     * @brief Imposta il livello minimo di logging
     * @param level Livello minimo (solo messaggi >= level vengono stampati)
     */
    static void setLevel(Level level) {
        currentLevel = level;
    }

    /**
     * @brief Ottiene il livello corrente
     */
    static Level getLevel() {
        return currentLevel;
    }

    /**
     * @brief Log generico con livello specificato
     */
    static void log(Level level, const char* tag, const char* format, ...) {
        if (level < currentLevel || !Serial) return;

        // Timestamp
        char timeStr[12];
        unsigned long ms = millis();
        snprintf(timeStr, sizeof(timeStr), "%10lu", ms);

        // Level string
        const char* levelStr = getLevelString(level);

        // Prefix
        Serial.printf("[%s][%s][%s] ", timeStr, levelStr, tag);

        // Message con varargs
        va_list args;
        va_start(args, format);
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        char line[320];
        snprintf(line, sizeof(line), "[%s][%s][%s] %s", timeStr, levelStr, tag, buffer);
        Serial.println(line);
        appendRecentLog(line);
    }

    static String getRecentLogs(uint8_t maxLines = 8);

    // Convenience methods
    static void debug(const char* tag, const char* format, ...) {
        if (DEBUG < currentLevel) return;
        va_list args;
        va_start(args, format);
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        log(DEBUG, tag, "%s", buffer);
    }

    static void info(const char* tag, const char* format, ...) {
        if (INFO < currentLevel) return;
        va_list args;
        va_start(args, format);
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        log(INFO, tag, "%s", buffer);
    }

    static void warn(const char* tag, const char* format, ...) {
        if (WARN < currentLevel) return;
        va_list args;
        va_start(args, format);
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        log(WARN, tag, "%s", buffer);
    }

    static void error(const char* tag, const char* format, ...) {
        if (ERROR < currentLevel) return;
        va_list args;
        va_start(args, format);
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        log(ERROR, tag, "%s", buffer);
    }

    static void critical(const char* tag, const char* format, ...) {
        va_list args;
        va_start(args, format);
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        log(CRITICAL, tag, "%s", buffer);
    }

private:
    static Level currentLevel;
    static void appendRecentLog(const char* line);
    static constexpr uint8_t RECENT_LOG_CAPACITY = 12;
    static constexpr size_t RECENT_LOG_LINE_SIZE = 160;
    static char recentLogs[RECENT_LOG_CAPACITY][RECENT_LOG_LINE_SIZE];
    static uint8_t recentLogCount;
    static uint8_t recentLogWriteIndex;

    static const char* getLevelString(Level level) {
        switch (level) {
            case DEBUG:    return "DEBUG";
            case INFO:     return "INFO ";
            case WARN:     return "WARN ";
            case ERROR:    return "ERROR";
            case CRITICAL: return "CRIT ";
            default:       return "UNKN ";
        }
    }
};

// Macro per semplificare l'uso
#define LOG_DEBUG(tag, ...) Logger::debug(tag, __VA_ARGS__)
#define LOG_INFO(tag, ...) Logger::info(tag, __VA_ARGS__)
#define LOG_WARN(tag, ...) Logger::warn(tag, __VA_ARGS__)
#define LOG_ERROR(tag, ...) Logger::error(tag, __VA_ARGS__)
#define LOG_CRITICAL(tag, ...) Logger::critical(tag, __VA_ARGS__)

#endif
