#pragma once
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "config.h"

class WatchdogManager {
private:
    uint32_t lastFeedTime = 0;
    const uint32_t FEED_INTERVAL = Config::Watchdog::FEED_INTERVAL_MS;
    uint32_t feedCount = 0;
    uint32_t autoFeedCount = 0;
    uint32_t manualFeedCount = 0;
    
public:
    WatchdogManager() {
        lastFeedTime = millis();
    }
    
    // Feed se necessario (automatico)
    void feedIfNeeded() {
        uint32_t now = millis();
        if (now - lastFeedTime >= FEED_INTERVAL) {
            esp_task_wdt_reset();
            yield();
            lastFeedTime = now;
            feedCount++;
            autoFeedCount++;
            
            // Log periodico ogni 1000 feed (circa ogni 100 secondi)
            if (autoFeedCount % 1000 == 0) {
                Serial.printf("[WatchdogManager] Auto-feed count: %d (Total: %d, Manual: %d)\n", 
                             autoFeedCount, feedCount, manualFeedCount);
            }
        }
    }
    
    // Feed forzato (manuale)
    void forceFeed() {
        esp_task_wdt_reset();
        yield();
        lastFeedTime = millis();
        feedCount++;
        manualFeedCount++;
    }
    
    // Feed con delay integrato (per operazioni lunghe)
    // Divide il delay in chunk e fa feed tra ogni chunk
    void feedWithDelay(uint32_t delayMs) {
        if (delayMs == 0) return;
        
        uint32_t chunks = delayMs / FEED_INTERVAL;
        uint32_t remainder = delayMs % FEED_INTERVAL;
        
        // Feed periodicamente durante il delay
        for (uint32_t i = 0; i < chunks; i++) {
            delay(FEED_INTERVAL);
            forceFeed();
        }
        
        // Delay rimanente
        if (remainder > 0) {
            delay(remainder);
            forceFeed();
        }
    }
    
    // Statistiche
    uint32_t getFeedCount() const {
        return feedCount;
    }
    
    uint32_t getAutoFeedCount() const {
        return autoFeedCount;
    }
    
    uint32_t getManualFeedCount() const {
        return manualFeedCount;
    }
    
    uint32_t getTimeSinceLastFeed() const {
        return millis() - lastFeedTime;
    }
    
    // Check se siamo vicini al timeout (per diagnostica)
    bool isNearTimeout() const {
        uint32_t timeSinceFeed = getTimeSinceLastFeed();
        uint32_t timeoutMs = Config::Watchdog::TIMEOUT_SECONDS * 1000;
        return timeSinceFeed > (timeoutMs * 0.8); // 80% del timeout
    }
    
    void printStats() {
        Serial.println("[WatchdogManager] === Statistiche Watchdog ===");
        Serial.printf("  Feed totali: %d\n", feedCount);
        Serial.printf("  Feed automatici: %d\n", autoFeedCount);
        Serial.printf("  Feed manuali: %d\n", manualFeedCount);
        Serial.printf("  Ultimo feed: %d ms fa\n", getTimeSinceLastFeed());
        Serial.printf("  Near timeout: %s\n", isNearTimeout() ? "SI ⚠️" : "NO ✅");
    }
};
