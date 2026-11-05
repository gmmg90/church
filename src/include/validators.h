#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

namespace Validators {
    
    struct ValidationResult {
        bool valid;
        String error;
        
        // Helper per creazione rapida
        static ValidationResult success() {
            return {true, ""};
        }
        
        static ValidationResult failure(const String& errorMsg) {
            return {false, errorMsg};
        }
        
        // Operator bool per uso in if
        operator bool() const { return valid; }
    };
    
    // ===== MELODY VALIDATION =====
    ValidationResult validateMelody(const JsonObject& melody) {
        // Nome obbligatorio
        if (!melody.containsKey("name") || melody["name"].as<String>().length() == 0) {
            return ValidationResult::failure("Nome melodia obbligatorio");
        }
        
        String name = melody["name"].as<String>();
        if (name.length() > 50) {
            return ValidationResult::failure("Nome troppo lungo (max 50 caratteri)");
        }
        
        // Array note obbligatorio
        if (!melody.containsKey("notes")) {
            return ValidationResult::failure("Array note mancante");
        }
        
        JsonArray notes = melody["notes"].as<JsonArray>();
        if (notes.size() == 0) {
            return ValidationResult::failure("Melodia vuota (nessuna nota)");
        }
        
        if (notes.size() > 100) { // MAX_NOTES_PER_MELODY
            return ValidationResult::failure("Troppe note (max 100)");
        }
        
        // Valida ogni singola nota
        int noteIndex = 0;
        for (JsonVariant v : notes) {
            noteIndex++;
            
            if (!v.is<JsonObject>()) {
                return ValidationResult::failure("Nota #" + String(noteIndex) + " non valida");
            }
            
            JsonObject note = v.as<JsonObject>();
            
            // Verifica bellNumber
            if (!note.containsKey("bellNumber")) {
                return ValidationResult::failure("Nota #" + String(noteIndex) + ": bellNumber mancante");
            }
            
            uint8_t bell = note["bellNumber"] | 0;
            if (bell < 1 || bell > 2) {
                return ValidationResult::failure("Nota #" + String(noteIndex) + ": bellNumber invalido (" + String(bell) + "). Deve essere 1 o 2");
            }
            
            // Verifica duration
            if (!note.containsKey("duration")) {
                return ValidationResult::failure("Nota #" + String(noteIndex) + ": duration mancante");
            }
            
            uint16_t duration = note["duration"] | 0;
            if (duration < 50 || duration > 5000) {
                return ValidationResult::failure("Nota #" + String(noteIndex) + ": duration invalida (" + String(duration) + "ms). Range: 50-5000ms");
            }
            
            // Verifica delay (opzionale)
            if (note.containsKey("delay")) {
                uint16_t delay = note["delay"] | 0;
                if (delay > 10000) {
                    return ValidationResult::failure("Nota #" + String(noteIndex) + ": delay troppo lungo (" + String(delay) + "ms). Max: 10000ms");
                }
            }
        }
        
        return ValidationResult::success();
    }
    
    // ===== WEEKLY SCHEDULE VALIDATION =====
    ValidationResult validateSchedule(const JsonObject& schedule) {
        // Nome opzionale ma se presente deve essere valido
        if (schedule.containsKey("name")) {
            String name = schedule["name"].as<String>();
            if (name.length() > 50) {
                return ValidationResult::failure("Nome troppo lungo (max 50 caratteri)");
            }
        }
        
        // DayOfWeek obbligatorio (0-6)
        if (!schedule.containsKey("dayOfWeek")) {
            return ValidationResult::failure("Giorno settimana obbligatorio (dayOfWeek)");
        }
        
        int day = schedule["dayOfWeek"] | -1;
        if (day < 0 || day > 6) {
            return ValidationResult::failure("Giorno settimana invalido: " + String(day) + ". Range: 0 (Dom) - 6 (Sab)");
        }
        
        // Hour obbligatorio (0-23)
        if (!schedule.containsKey("hour")) {
            return ValidationResult::failure("Ora obbligatoria (hour)");
        }
        
        int hour = schedule["hour"] | -1;
        if (hour < 0 || hour > 23) {
            return ValidationResult::failure("Ora invalida: " + String(hour) + ". Range: 0-23");
        }
        
        // Minute obbligatorio (0-59)
        if (!schedule.containsKey("minute")) {
            return ValidationResult::failure("Minuto obbligatorio (minute)");
        }
        
        int minute = schedule["minute"] | -1;
        if (minute < 0 || minute > 59) {
            return ValidationResult::failure("Minuto invalido: " + String(minute) + ". Range: 0-59");
        }
        
        // MelodyIndex obbligatorio
        if (!schedule.containsKey("melodyIndex")) {
            return ValidationResult::failure("Indice melodia obbligatorio (melodyIndex)");
        }
        
        int melodyIdx = schedule["melodyIndex"] | -1;
        if (melodyIdx < 0 || melodyIdx >= 10) { // MAX_MELODIES
            return ValidationResult::failure("Indice melodia invalido: " + String(melodyIdx) + ". Range: 0-9");
        }
        
        return ValidationResult::success();
    }
    
    // ===== SPECIAL EVENT VALIDATION =====
    ValidationResult validateSpecialEvent(const JsonObject& event) {
        // Nome obbligatorio
        if (!event.containsKey("name") || event["name"].as<String>().length() == 0) {
            return ValidationResult::failure("Nome evento obbligatorio");
        }
        
        String name = event["name"].as<String>();
        if (name.length() > 50) {
            return ValidationResult::failure("Nome troppo lungo (max 50 caratteri)");
        }
        
        // Data obbligatoria (year, month, day)
        if (!event.containsKey("year") || !event.containsKey("month") || !event.containsKey("day")) {
            return ValidationResult::failure("Data completa obbligatoria (year, month, day)");
        }
        
        int year = event["year"] | 0;
        int month = event["month"] | 0;
        int day = event["day"] | 0;
        
        if (year < 2020 || year > 2100) {
            return ValidationResult::failure("Anno invalido: " + String(year) + ". Range: 2020-2100");
        }
        
        if (month < 1 || month > 12) {
            return ValidationResult::failure("Mese invalido: " + String(month) + ". Range: 1-12");
        }
        
        if (day < 1 || day > 31) {
            return ValidationResult::failure("Giorno invalido: " + String(day) + ". Range: 1-31");
        }
        
        // Validazione giorni per mese (semplificata)
        if (month == 2 && day > 29) {
            return ValidationResult::failure("Febbraio ha max 29 giorni");
        }
        if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30) {
            return ValidationResult::failure("Questo mese ha max 30 giorni");
        }
        
        // Ora obbligatoria
        if (!event.containsKey("hour") || !event.containsKey("minute")) {
            return ValidationResult::failure("Orario obbligatorio (hour, minute)");
        }
        
        int hour = event["hour"] | -1;
        int minute = event["minute"] | -1;
        
        if (hour < 0 || hour > 23) {
            return ValidationResult::failure("Ora invalida: " + String(hour) + ". Range: 0-23");
        }
        
        if (minute < 0 || minute > 59) {
            return ValidationResult::failure("Minuto invalido: " + String(minute) + ". Range: 0-59");
        }
        
        // MelodyIndex obbligatorio
        if (!event.containsKey("melodyIndex")) {
            return ValidationResult::failure("Melodia obbligatoria (melodyIndex)");
        }
        
        int melodyIdx = event["melodyIndex"] | -1;
        if (melodyIdx < 0 || melodyIdx >= 10) { // MAX_MELODIES
            return ValidationResult::failure("Indice melodia invalido: " + String(melodyIdx) + ". Range: 0-9");
        }
        
        return ValidationResult::success();
    }
    
    // ===== WIFI CONFIG VALIDATION =====
    ValidationResult validateWiFiConfig(const JsonObject& config) {
        if (!config.containsKey("ssid") || config["ssid"].as<String>().length() == 0) {
            return ValidationResult::failure("SSID obbligatorio");
        }
        
        String ssid = config["ssid"].as<String>();
        if (ssid.length() > 32) {
            return ValidationResult::failure("SSID troppo lungo (max 32 caratteri)");
        }
        
        if (!config.containsKey("password")) {
            return ValidationResult::failure("Password obbligatoria");
        }
        
        String password = config["password"].as<String>();
        if (password.length() < 8 || password.length() > 63) {
            return ValidationResult::failure("Password deve essere 8-63 caratteri");
        }
        
        return ValidationResult::success();
    }
    
    // ===== TIME VALIDATION =====
    ValidationResult validateDateTime(int year, int month, int day, int hour, int minute, int second) {
        if (year < 2020 || year > 2100) {
            return ValidationResult::failure("Anno invalido: " + String(year));
        }
        if (month < 1 || month > 12) {
            return ValidationResult::failure("Mese invalido: " + String(month));
        }
        if (day < 1 || day > 31) {
            return ValidationResult::failure("Giorno invalido: " + String(day));
        }
        if (hour < 0 || hour > 23) {
            return ValidationResult::failure("Ora invalida: " + String(hour));
        }
        if (minute < 0 || minute > 59) {
            return ValidationResult::failure("Minuto invalido: " + String(minute));
        }
        if (second < 0 || second > 59) {
            return ValidationResult::failure("Secondo invalido: " + String(second));
        }
        
        return ValidationResult::success();
    }
    
} // namespace Validators
