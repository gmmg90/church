#ifndef DIAGNOSTICS_H
#define DIAGNOSTICS_H

#include <Arduino.h>
#include "logger.h"
#include "utils.h"

/**
 * @brief Sistema di diagnostica e monitoraggio
 */
class Diagnostics {
public:
    struct MemoryInfo {
        size_t freeHeap;
        size_t minFreeHeap;
        size_t maxAllocHeap;
        float fragmentationPercent;
    };
    
    struct SystemInfo {
        uint32_t uptimeMs;
        uint32_t resetReason;
        uint32_t cpuFreqMHz;
        String sdkVersion;
        float temperature;
    };
    
    /**
     * @brief Ottieni informazioni sulla memoria
     */
    static MemoryInfo getMemoryInfo() {
        MemoryInfo info;
        info.freeHeap = ESP.getFreeHeap();
        info.minFreeHeap = ESP.getMinFreeHeap();
        info.maxAllocHeap = ESP.getMaxAllocHeap();
        
        // Calcola frammentazione
        if (info.freeHeap > 0) {
            info.fragmentationPercent = 100.0f * (1.0f - (float)info.maxAllocHeap / (float)info.freeHeap);
        } else {
            info.fragmentationPercent = 0.0f;
        }
        
        return info;
    }
    
    /**
     * @brief Ottieni informazioni di sistema
     */
    static SystemInfo getSystemInfo() {
        SystemInfo info;
        info.uptimeMs = millis();
        info.resetReason = esp_reset_reason();
        info.cpuFreqMHz = ESP.getCpuFreqMHz();
        info.sdkVersion = ESP.getSdkVersion();
        info.temperature = temperatureRead();
        
        return info;
    }
    
    /**
     * @brief Stampa report diagnostico completo
     */
    static void printReport() {
        LOG_INFO("Diagnostics", "=== REPORT DIAGNOSTICO ===");
        
        // Memoria
        MemoryInfo mem = getMemoryInfo();
        LOG_INFO("Diagnostics", "Memoria libera: %s", Utils::formatMemory(mem.freeHeap).c_str());
        LOG_INFO("Diagnostics", "Memoria minima: %s", Utils::formatMemory(mem.minFreeHeap).c_str());
        LOG_INFO("Diagnostics", "Max allocabile: %s", Utils::formatMemory(mem.maxAllocHeap).c_str());
        LOG_INFO("Diagnostics", "Frammentazione: %.1f%%", mem.fragmentationPercent);
        
        // Sistema
        SystemInfo sys = getSystemInfo();
        LOG_INFO("Diagnostics", "Uptime: %s", Utils::formatUptime(sys.uptimeMs).c_str());
        LOG_INFO("Diagnostics", "CPU: %d MHz", sys.cpuFreqMHz);
        LOG_INFO("Diagnostics", "SDK: %s", sys.sdkVersion.c_str());
        LOG_INFO("Diagnostics", "Temperatura: %s", Utils::formatTemperature(sys.temperature).c_str());
        
        LOG_INFO("Diagnostics", "========================");
    }
    
    /**
     * @brief Verifica e avvisa se memoria bassa
     */
    static bool checkMemoryHealth(size_t minFree = 10240) {
        MemoryInfo mem = getMemoryInfo();
        
        if (mem.freeHeap < minFree) {
            LOG_WARN("Diagnostics", "MEMORIA BASSA: %s liberi (min: %s)", 
                    Utils::formatMemory(mem.freeHeap).c_str(),
                    Utils::formatMemory(minFree).c_str());
            return false;
        }
        
        if (mem.fragmentationPercent > 50.0f) {
            LOG_WARN("Diagnostics", "FRAMMENTAZIONE ELEVATA: %.1f%%", mem.fragmentationPercent);
            return false;
        }
        
        return true;
    }
    
    /**
     * @brief Crea JSON con diagnostica (senza DynamicJsonDocument)
     */
    static String toJson() {
        MemoryInfo mem = getMemoryInfo();
        SystemInfo sys = getSystemInfo();
        
        String json = "{";
        json += "\"memory\":{";
        json += "\"free\":" + String(mem.freeHeap) + ",";
        json += "\"minFree\":" + String(mem.minFreeHeap) + ",";
        json += "\"maxAlloc\":" + String(mem.maxAllocHeap) + ",";
        json += "\"fragmentation\":" + String(mem.fragmentationPercent, 1);
        json += "},";
        json += "\"system\":{";
        json += "\"uptime\":" + String(sys.uptimeMs) + ",";
        json += "\"cpuMHz\":" + String(sys.cpuFreqMHz) + ",";
        json += "\"temperature\":" + String(sys.temperature, 1) + ",";
        json += "\"sdkVersion\":\"" + sys.sdkVersion + "\"";
        json += "}";
        json += "}";
        
        return json;
    }
};

#endif
