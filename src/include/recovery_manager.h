#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include "config.h"

// Pin factory reset (boot button GPIO0)
static constexpr uint8_t FACTORY_RESET_PIN = 0;

/**
 * RecoveryManager v2.3.8
 * Sistema di recovery multilivello per ESP32 Church Bell Controller
 * 
 * Funzionalità:
 * 1. Fallback automatico a DHCP se IP statico fallisce (30s)
 * 2. AP Mode dopo 5 tentativi falliti di connessione WiFi
 * 3. Factory Reset manuale tramite pulsante BOOT (10s hold)
 */

class RecoveryManager {
private:
    uint8_t wifiFailCount = 0;
    bool apModeActive = false;
    uint32_t factoryResetButtonPressStart = 0;
    bool factoryResetInProgress = false;
    
public:
    RecoveryManager() {}
    
    // Inizializza il sistema di recovery
    void begin() {
        pinMode(FACTORY_RESET_PIN, INPUT_PULLUP); // BOOT button GPIO0
        Serial.println("[Recovery] Sistema recovery inizializzato");
        Serial.println("[Recovery] Premere BOOT per 10s per factory reset");
    }
    
    // Controlla se il pulsante factory reset è premuto (chiamare in loop())
    void checkFactoryReset() {
        // Filtro anti-disturbo: il pin deve restare LOW per almeno 100 ms
        static uint32_t lowSince = 0;
        bool rawLow = digitalRead(FACTORY_RESET_PIN) == LOW;
        if (!rawLow) {
            lowSince = 0;
        } else if (lowSince == 0) {
            lowSince = millis() | 1;
        }
        if (rawLow && millis() - lowSince >= 100) {
            // Pulsante premuto
            if (factoryResetButtonPressStart == 0) {
                factoryResetButtonPressStart = millis();
                Serial.println("[Recovery] ⚠️ Pulsante factory reset premuto...");
            }
            
            uint32_t pressDuration = millis() - factoryResetButtonPressStart;
            
            // Feedback progressivo ogni 1 secondo
            static uint32_t lastFeedback = 0;
            if (pressDuration - lastFeedback >= 1000) {
                uint32_t remaining = (10000 - pressDuration) / 1000;
                if (remaining > 0) {
                    Serial.printf("[Recovery] ⏳ Factory reset tra %lu secondi...\n", remaining);
                }
                lastFeedback = pressDuration;
            }
            
            // Factory reset dopo 10 secondi
            if (pressDuration >= 10000 && !factoryResetInProgress) {
                factoryResetInProgress = true;
                performFactoryReset();
            }
        } else {
            // Pulsante rilasciato
            if (factoryResetButtonPressStart > 0) {
                uint32_t pressDuration = millis() - factoryResetButtonPressStart;
                if (pressDuration < 10000) {
                    Serial.printf("[Recovery] Factory reset annullato (rilasciato dopo %lus)\n", 
                                 pressDuration/1000);
                }
                factoryResetButtonPressStart = 0;
            }
        }
    }
    
    // Incrementa contatore fallimenti WiFi
    void reportWiFiFailure() {
        wifiFailCount++;
        Serial.printf("[Recovery] ⚠️ WiFi connection failed (attempt %d/5)\n", wifiFailCount);
        
        if (wifiFailCount >= 5) {
            Serial.println("[Recovery] ❌ Max WiFi retry attempts reached!");
            Serial.println("[Recovery] 🆘 Starting AP Recovery Mode...");
            startAPMode();
        }
    }
    
    // Reset contatore fallimenti (chiamato quando connessione OK)
    void resetWiFiFailCount() {
        if (wifiFailCount > 0) {
            Serial.printf("[Recovery] ✅ WiFi connesso - Reset fail counter (era %d)\n", wifiFailCount);
            wifiFailCount = 0;
        }
    }
    
    // Avvia Access Point mode per recovery
    void startAPMode() {
        if (apModeActive) return; // Già attivo
        
        Serial.println("");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("🆘 RECOVERY MODE - ACCESS POINT ATTIVO");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        
        // Disconnetti WiFi station
        WiFi.disconnect(true);
        delay(100);
        
        // Configura IP statico per AP
        IPAddress local_IP(192, 168, 4, 1);
        IPAddress gateway(192, 168, 4, 1);
        IPAddress subnet(255, 255, 255, 0);
        
        WiFi.softAPConfig(local_IP, gateway, subnet);
        
        // Avvia Access Point
        const char* apSSID = "ChurchBells_Recovery";
        const char* apPassword = "recovery123";
        
        bool apStarted = WiFi.softAP(apSSID, apPassword);
        
        if (apStarted) {
            apModeActive = true;
            IPAddress IP = WiFi.softAPIP();
            
            Serial.printf("✅ Access Point avviato con successo!\n");
            Serial.printf("📡 SSID: %s\n", apSSID);
            Serial.printf("🔑 Password: %s\n", apPassword);
            Serial.printf("🌐 IP: %s\n", IP.toString().c_str());
            Serial.printf("🔗 URL: http://%s\n", IP.toString().c_str());
            Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            Serial.println("📱 Connettiti all'Access Point e riconfigura WiFi");
            Serial.println("🔄 Il sistema si riavvierà dopo la configurazione");
            Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            Serial.println("");
        } else {
            Serial.println("[Recovery] ❌ Errore: impossibile avviare Access Point!");
        }
    }
    
    // Factory reset completo - elimina tutte le configurazioni
    void performFactoryReset() {
        Serial.println("");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("⚠️  FACTORY RESET IN CORSO");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        
        // Elimina tutti i file di configurazione
        if (SPIFFS.begin(true)) {
            Serial.println("[Factory Reset] Eliminazione configurazioni...");
            
            // Lista file da eliminare
            const char* filesToDelete[] = {
                Config::FileSystem::NETWORK_FILE,
                "/wifi_config.json",  // Usa percorso diretto invece di WIFI_FILE
                Config::FileSystem::SCHEDULES_FILE,
                Config::FileSystem::MELODIES_FILE,
                Config::FileSystem::EVENTS_FILE
            };
            
            for (const char* file : filesToDelete) {
                if (SPIFFS.exists(file)) {
                    bool deleted = SPIFFS.remove(file);
                    Serial.printf("[Factory Reset] %s %s\n", 
                                 deleted ? "✓ Eliminato:" : "✗ Errore:", file);
                } else {
                    Serial.printf("[Factory Reset] ⊘ Non trovato: %s\n", file);
                }
            }
            
            Serial.println("[Factory Reset] ✅ Configurazioni eliminate");
            SPIFFS.end();
        } else {
            Serial.println("[Factory Reset] ❌ Errore apertura SPIFFS!");
        }
        
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("✅ FACTORY RESET COMPLETATO");
        Serial.println("🔄 RIAVVIO IN 3 SECONDI...");
        Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        Serial.println("");
        
        delay(3000);
        ESP.restart();
    }
    
    bool isAPModeActive() const {
        return apModeActive;
    }
    
    uint8_t getWiFiFailCount() const {
        return wifiFailCount;
    }
};
