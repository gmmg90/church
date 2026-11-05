#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * @brief Funzioni di utilità comuni per il progetto
 */
namespace Utils {
    
    /**
     * @brief Safe JSON serialization con gestione errori
     */
    template<typename T>
    bool safeSerializeJson(const T& doc, String& output) {
        output.clear();
        size_t size = serializeJson(doc, output);
        return size > 0;
    }
    
    /**
     * @brief Formatta uptime in stringa leggibile
     */
    inline String formatUptime(uint32_t milliseconds) {
        uint32_t seconds = milliseconds / 1000;
        uint32_t minutes = seconds / 60;
        uint32_t hours = minutes / 60;
        uint32_t days = hours / 24;
        
        char buffer[32];
        if (days > 0) {
            snprintf(buffer, sizeof(buffer), "%dd %dh %dm", 
                     days, hours % 24, minutes % 60);
        } else if (hours > 0) {
            snprintf(buffer, sizeof(buffer), "%dh %dm %ds", 
                     hours, minutes % 60, seconds % 60);
        } else if (minutes > 0) {
            snprintf(buffer, sizeof(buffer), "%dm %ds", 
                     minutes, seconds % 60);
        } else {
            snprintf(buffer, sizeof(buffer), "%ds", seconds);
        }
        return String(buffer);
    }
    
    /**
     * @brief Formatta memoria in formato leggibile
     */
    inline String formatMemory(size_t bytes) {
        if (bytes < 1024) {
            return String(bytes) + " B";
        } else if (bytes < 1024 * 1024) {
            return String(bytes / 1024.0, 2) + " KB";
        } else {
            return String(bytes / (1024.0 * 1024.0), 2) + " MB";
        }
    }
    
    /**
     * @brief Verifica memoria disponibile e avvisa se bassa
     */
    inline bool checkMemory(size_t minFree = 10240) {
        size_t free = ESP.getFreeHeap();
        if (free < minFree) {
            return false;
        }
        return true;
    }
    
    /**
     * @brief Yield intelligente con contatore
     */
    inline void smartYield(int& counter, int interval = 10) {
        counter++;
        if (counter % interval == 0) {
            yield();
            delay(0);
        }
    }
    
    /**
     * @brief Clamp valore tra min e max
     */
    template<typename T>
    inline T clamp(T value, T minVal, T maxVal) {
        if (value < minVal) return minVal;
        if (value > maxVal) return maxVal;
        return value;
    }
    
    /**
     * @brief Valida range di valori
     */
    inline bool inRange(int value, int minVal, int maxVal) {
        return value >= minVal && value <= maxVal;
    }
    
    /**
     * @brief Formatta temperatura con unità
     */
    inline String formatTemperature(float temp) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%.1f°C", temp);
        return String(buffer);
    }
    
    /**
     * @brief Truncate string a lunghezza massima
     */
    inline void truncateString(char* str, size_t maxLen) {
        if (strlen(str) >= maxLen) {
            str[maxLen - 1] = '\0';
        }
    }
}

#endif
