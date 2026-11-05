#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include "config.h"

class WiFiReconnectManager {
private:
    uint32_t lastAttemptTime = 0;
    uint32_t backoffTime = 5000;
    const uint32_t MIN_BACKOFF = 5000;
    const uint32_t MAX_BACKOFF = 300000; // Max 5 minuti
    const uint32_t BACKOFF_MULTIPLIER = 2;
    // Timeout dopo il quale un tentativo in corso è considerato fallito
    static constexpr uint32_t CONNECT_TIMEOUT_MS = 15000; // 15 secondi

    uint16_t reconnectAttempts = 0;
    uint16_t totalReconnects = 0;

    String currentSSID;
    String currentPassword;

    // Stato macchina a stati non-bloccante
    enum class State { IDLE, CONNECTING };
    State _state = State::IDLE;
    uint32_t _connectStartTime = 0;
    bool _lastAttemptFailed = false; // true dopo un timeout definitivo

    // ✅ v2.3.8: Supporto fallback DHCP
    bool staticIPFailed = false;
    uint8_t staticIPFailCount = 0;

public:
    WiFiReconnectManager() {}

    void setCredentials(const String& ssid, const String& password) {
        currentSSID = ssid;
        currentPassword = password;
    }

    bool isConnected() {
        return WiFi.status() == WL_CONNECTED;
    }

    // Ritorna true se connesso, false altrimenti (non bloccante).
    // Usare hadLastAttemptFailed() per sapere se un tentativo è definitivamente fallito.
    bool tryReconnect() {
        _lastAttemptFailed = false;
        uint32_t now = millis();

        // Se nel frattempo si è connesso (es. riconnessione automatica WiFi stack)
        if (isConnected()) {
            if (_state == State::CONNECTING) {
                totalReconnects++;
                Serial.printf("[WiFiManager] ✅ WiFi connesso! IP: %s (totale reconnect: %d)\n",
                              WiFi.localIP().toString().c_str(), totalReconnects);
            }
            _state = State::IDLE;
            reconnectAttempts = 0;
            backoffTime = MIN_BACKOFF;
            return true;
        }

        if (_state == State::CONNECTING) {
            // Verifica timeout del tentativo in corso
            if (now - _connectStartTime >= CONNECT_TIMEOUT_MS) {
                WiFi.disconnect(true);
                backoffTime = min(backoffTime * BACKOFF_MULTIPLIER, MAX_BACKOFF);
                lastAttemptTime = now;
                reconnectAttempts++;
                _state = State::IDLE;
                _lastAttemptFailed = true;
                Serial.printf("[WiFiManager] ❌ Timeout riconnessione #%d. Prossimo in %ds\n",
                              reconnectAttempts, backoffTime / 1000);
            }
            // Ancora in connessione: non fare nulla, ritorna false
            return false;
        }

        // State::IDLE — controlla backoff
        if (now - lastAttemptTime < backoffTime) {
            return false;
        }

        // Avvia tentativo asincrono
        _connectStartTime = now;
        lastAttemptTime = now;
        _state = State::CONNECTING;

        WiFi.disconnect(true);
        WiFi.mode(WIFI_STA);
        WiFi.begin(currentSSID.c_str(), currentPassword.c_str());

        Serial.printf("[WiFiManager] 🔄 Tentativo riconnessione (backoff: %ds)...\n",
                      backoffTime / 1000);
        return false;
    }

    // Ritorna true se l'ultimo tentativo è definitivamente fallito per timeout
    bool hadLastAttemptFailed() const {
        return _lastAttemptFailed;
    }
    
    void forceReconnect() {
        Serial.println("[WiFiManager] 🔄 Forzatura riconnessione WiFi...");
        backoffTime = MIN_BACKOFF;
        lastAttemptTime = 0;
        reconnectAttempts = 0;
        _state = State::IDLE;
        _lastAttemptFailed = false;
    }
    
    // ✅ v2.3.8: Gestione fallimento IP statico
    void reportStaticIPFailure() {
        staticIPFailed = true;
        staticIPFailCount++;
        Serial.printf("[WiFiManager] ⚠️ IP statico fallito (%d volte) - Prossimo tentativo userà DHCP\n", 
                     staticIPFailCount);
    }
    
    bool hasStaticIPFailed() const {
        return staticIPFailed;
    }
    
    void resetStaticIPFailure() {
        if (staticIPFailed) {
            Serial.println("[WiFiManager] ✅ Reset flag IP statico fallito");
            staticIPFailed = false;
        }
    }
    
    uint8_t getStaticIPFailCount() const {
        return staticIPFailCount;
    }
    
    uint16_t getTotalReconnects() const {
        return totalReconnects;
    }
    
    uint32_t getBackoffTime() const {
        return backoffTime;
    }
    
    uint32_t getNextRetryIn() const {
        uint32_t now = millis();
        if (now - lastAttemptTime >= backoffTime) {
            return 0;
        }
        return backoffTime - (now - lastAttemptTime);
    }
    
    uint16_t getReconnectAttempts() const {
        return reconnectAttempts;
    }
};
