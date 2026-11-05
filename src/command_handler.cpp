#include "include/command_handler.h"
#include "include/bell_controller.h"
#include "include/config.h"
#include "include/logger.h"
#include <WiFi.h>
#include <time.h>

extern bool testMode;
extern bool schedulerActive;
extern bool apMode;
extern int currentTimezoneOffset;
extern bool isDST;
extern void initSNTP(bool forceResync);
extern bool saveAllMelodiesToFS();

// Singleton instance
CommandHandler* CommandHandler::_instance = nullptr;

CommandHandler& CommandHandler::getInstance() {
    if (!_instance) {
        _instance = new CommandHandler();
    }
    return *_instance;
}

CommandHandler::CommandHandler()
    : _bellController(nullptr), _systemStatus(nullptr) {
}

CommandHandler::~CommandHandler() {
}

void CommandHandler::init(BellController* bellController, SystemStatus* systemStatus) {
    _bellController = bellController;
    _systemStatus = systemStatus;
    Logger::info("CommandHandler", "Inizializzato");
}

// ============= COMANDI BASE =============

CommandHandler::CommandResult CommandHandler::getStatus() {
    CommandResult result(true, "Status retrieved");
    
    if (!_systemStatus) {
        result.success = false;
        result.message = "SystemStatus non disponibile";
        return result;
    }
    
    result.jsonData = new DynamicJsonDocument(1024);
    JsonDocument& doc = *result.jsonData;
    
    // Stato base
    doc["bellsEnabled"] = _systemStatus->bellsEnabled;
    doc["wifiConnected"] = _systemStatus->wifiConnected;
    doc["ntpSynced"] = _systemStatus->ntpSynced;
    doc["rtcConnected"] = _systemStatus->rtcConnected;
    doc["testMode"] = testMode;
    doc["schedulerActive"] = schedulerActive;
    doc["apMode"] = apMode;
    doc["timezoneOffset"] = currentTimezoneOffset;
    doc["isDST"] = isDST;
    doc["firmwareVersion"] = Config::FIRMWARE_VERSION;
    
    // Uptime
    doc["uptimeMs"] = millis();
    
    // Temperatura
    if (_systemStatus->esp32Temperature > 0) {
        doc["esp32Temperature"] = _systemStatus->esp32Temperature;
    }
    doc["temperatureWarning"] = _systemStatus->temperatureWarning;
    doc["thermalProtection"] = _systemStatus->thermalProtection;
    
    // Bell stats
    doc["totalBellRings"] = _systemStatus->totalBellRings;
    doc["lastBellTime"] = _systemStatus->lastBellTime;

    // Rete
    if (WiFi.status() == WL_CONNECTED) {
        String ip = WiFi.localIP().toString();
        doc["ipAddress"] = ip;
        doc["wifiIP"] = ip;
        doc["rssi"] = WiFi.RSSI();
    } else {
        String apIp = WiFi.softAPIP().toString();
        doc["ipAddress"] = apIp;
        doc["apIP"] = apIp;
    }
    
    return result;
}

CommandHandler::CommandResult CommandHandler::toggleBells(bool enable) {
    if (!_systemStatus || !_bellController) {
        return CommandResult(false, "Componenti non disponibili");
    }
    
    _systemStatus->bellsEnabled = enable;
    _bellController->setEnabled(enable);
    
    String msg = enable ? "Campane attivate" : "Campane disattivate";
    Logger::info("CommandHandler", msg.c_str());
    
    return CommandResult(true, msg);
}

CommandHandler::CommandResult CommandHandler::ringBell(uint8_t bellNumber, uint16_t duration) {
    String errorMsg;
    
    if (!_bellController) {
        return CommandResult(false, "BellController non disponibile");
    }
    
    if (!validateDuration(duration, errorMsg)) {
        return CommandResult(false, errorMsg);
    }
    
    if (bellNumber < 1 || bellNumber > 2) {
        return CommandResult(false, "Numero campana non valido (1-2)");
    }
    
    // Verifica se le campane sono abilitate
    if (!_systemStatus || !_systemStatus->bellsEnabled) {
        return CommandResult(false, "Le campane sono disabilitate. Attiva con /enable");
    }
    
    Result res = _bellController->ringBell(bellNumber, duration);
    
    if (res.isSuccess()) {
        String msg = "Campana " + String(bellNumber) + " suonata per " + String(duration) + "ms";
        Logger::info("CommandHandler", msg.c_str());
        return CommandResult(true, msg);
    } else {
        return CommandResult(false, String("Errore: ") + res.message);
    }
}

// ============= MELODIE =============

CommandHandler::CommandResult CommandHandler::getMelodiesList() {
    CommandResult result(true, "Melodie retrieved");
    
    if (!_bellController) {
        result.success = false;
        result.message = "BellController non disponibile";
        return result;
    }
    
    result.jsonData = new DynamicJsonDocument(4096);
    JsonDocument& doc = *result.jsonData;
    JsonArray melodiesArr = doc.createNestedArray("melodies");
    
    // Itera tutte le melodie disponibili
    uint8_t melodyCount = 0;
    for (uint8_t i = 0; i < Config::Limits::MAX_MELODIES; i++) {
        uint16_t noteCount = _bellController->getMelodyNoteCount(i);
        if (noteCount > 0) {
            JsonObject melodyObj = melodiesArr.createNestedObject();
            melodyObj["id"] = i;
            melodyObj["index"] = i;
            melodyObj["name"] = _bellController->getMelodyName(i);
            melodyObj["noteCount"] = noteCount;
            melodyObj["duration"] = _bellController->getMelodyDuration(i);
            melodyObj["isActive"] = true;
            melodyCount++;
        }
    }
    
    doc["totalMelodies"] = melodyCount;
    return result;
}

CommandHandler::CommandResult CommandHandler::getMelodyDetails(uint8_t melodyIndex) {
    String errorMsg;
    if (!validateMelodyIndex(melodyIndex, errorMsg)) {
        return CommandResult(false, errorMsg);
    }
    
    if (!_bellController) {
        return CommandResult(false, "BellController non disponibile");
    }
    
    CommandResult result(true, "Melody details retrieved");
    result.jsonData = new DynamicJsonDocument(6144);
    JsonDocument& doc = *result.jsonData;
    
    doc["index"] = melodyIndex;
    doc["name"] = _bellController->getMelodyName(melodyIndex);
    doc["noteCount"] = _bellController->getMelodyNoteCount(melodyIndex);
    doc["duration"] = _bellController->getMelodyDuration(melodyIndex);

    JsonArray notes = doc.createNestedArray("notes");
    const BellNote* melodyNotes = _bellController->getMelodyNotes(melodyIndex);
    int noteCount = _bellController->getMelodyNoteCount(melodyIndex);
    for (int i = 0; i < noteCount; i++) {
        JsonObject n = notes.createNestedObject();
        n["bellNumber"] = melodyNotes[i].bellNumber;
        n["duration"] = melodyNotes[i].duration;
        n["delay"] = melodyNotes[i].delay;
    }
    
    return result;
}

CommandHandler::CommandResult CommandHandler::playMelody(uint8_t melodyIndex) {
    String errorMsg;
    if (!validateMelodyIndex(melodyIndex, errorMsg)) {
        return CommandResult(false, errorMsg);
    }
    
    if (!_bellController) {
        return CommandResult(false, "BellController non disponibile");
    }
    
    if (_bellController->isPlayingMelody()) {
        return CommandResult(false, "Una melodia è già in esecuzione");
    }
    
    Result res = _bellController->playMelody(melodyIndex);
    
    if (res.isSuccess()) {
        String msg = "Melodia " + String(melodyIndex) + " in esecuzione";
        Logger::info("CommandHandler", msg.c_str());
        return CommandResult(true, msg);
    } else {
        return CommandResult(false, String("Errore: ") + res.message);
    }
}

CommandHandler::CommandResult CommandHandler::stopMelody() {
    if (!_bellController) {
        return CommandResult(false, "BellController non disponibile");
    }
    
    _bellController->stopMelody();
    return CommandResult(true, "Melodia interrotta");
}

// ============= CONFIGURAZIONE =============

CommandHandler::CommandResult CommandHandler::getCurrentTime() {
    CommandResult result(true, "Current time retrieved");
    result.jsonData = new DynamicJsonDocument(256);
    JsonDocument& doc = *result.jsonData;
    
    time_t now = time(nullptr);
    if (now > 86400) { // Sanity check: dopo il 1970
        struct tm* timeinfo = localtime(&now);
        char bufTime[20];
        char bufDate[20];
        snprintf(bufTime, sizeof(bufTime), "%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
        snprintf(bufDate, sizeof(bufDate), "%02d/%02d/%04d", timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);

        doc["unixtime"] = (uint32_t)now;
        doc["year"] = timeinfo->tm_year + 1900;
        doc["month"] = timeinfo->tm_mon + 1;
        doc["day"] = timeinfo->tm_mday;
        doc["hour"] = timeinfo->tm_hour;
        doc["min"] = timeinfo->tm_min;
        doc["sec"] = timeinfo->tm_sec;
        doc["day_of_week"] = timeinfo->tm_wday;
        doc["time"] = bufTime;
        doc["date"] = bufDate;
    } else {
        result.success = false;
        result.message = "Tempo non sincronizzato";
    }
    
    return result;
}

CommandHandler::CommandResult CommandHandler::resyncNTP() {
    if (!_systemStatus || !_systemStatus->wifiConnected) {
        return CommandResult(false, "WiFi non connesso");
    }

    initSNTP(true);
    return CommandResult(_systemStatus->ntpSynced, _systemStatus->ntpSynced ? "NTP sincronizzato" : "Sincronizzazione NTP fallita");
}

CommandHandler::CommandResult CommandHandler::getBackup() {
    // TODO: generare backup JSON
    CommandResult result(true, "Backup generated");
    result.jsonData = new DynamicJsonDocument(4096);
    return result;
}

// ============= SCAMPANATE SPECIALI =============

CommandHandler::CommandResult CommandHandler::playFuneral() {
    if (!_bellController) {
        return CommandResult(false, "BellController non disponibile");
    }
    
    if (_bellController->getMelodyNoteCount(0) == 0) {
        return CommandResult(false, "Melodia funerale non configurata");
    }
    
    if (_bellController->isPlayingMelody()) {
        return CommandResult(false, "Una melodia è già in esecuzione");
    }
    
    Result res = _bellController->playMelody(0); // Index 0 = funeral
    
    if (res.isSuccess()) {
        Logger::info("CommandHandler", "Scampanio funerale avviato");
        return CommandResult(true, "Scampanio funerale in esecuzione");
    } else {
        return CommandResult(false, String("Errore: ") + res.message);
    }
}

CommandHandler::CommandResult CommandHandler::playMass() {
    if (!_bellController) {
        return CommandResult(false, "BellController non disponibile");
    }
    
    if (_bellController->getMelodyNoteCount(1) == 0) {
        return CommandResult(false, "Melodia messa non configurata");
    }
    
    if (_bellController->isPlayingMelody()) {
        return CommandResult(false, "Una melodia è già in esecuzione");
    }
    
    Result res = _bellController->playMelody(1); // Index 1 = mass
    
    if (res.isSuccess()) {
        Logger::info("CommandHandler", "Scampanio messa avviato");
        return CommandResult(true, "Scampanio messa in esecuzione");
    } else {
        return CommandResult(false, String("Errore: ") + res.message);
    }
}

// ============= UTILITY =============

bool CommandHandler::validateMelodyIndex(uint8_t index, String& errorMsg) const {
    if (!_bellController) {
        errorMsg = "BellController non disponibile";
        return false;
    }
    
    if (index >= Config::Limits::MAX_MELODIES) {
        errorMsg = "Indice melodia non valido (0-9)";
        return false;
    }
    
    if (_bellController->getMelodyNoteCount(index) == 0) {
        errorMsg = "Melodia non configurata";
        return false;
    }
    
    return true;
}

bool CommandHandler::validateDuration(uint16_t duration, String& errorMsg) const {
    // Stessi limiti del controller: un martello non deve restare premuto più di BELL_MAX_PULSE
    if (duration < BELL_MIN_PULSE || duration > BELL_MAX_PULSE) {
        errorMsg = "Durata non valida (" + String(BELL_MIN_PULSE) + "-" + String(BELL_MAX_PULSE) + " ms)";
        return false;
    }
    return true;
}

// Placeholder per metodi non implementati ancora
CommandHandler::CommandResult CommandHandler::getWeeklySchedules() {
    return CommandResult(true, "Non implementato");
}

CommandHandler::CommandResult CommandHandler::setWeeklySchedule(const String& jsonPayload) {
    return CommandResult(false, "Non implementato");
}

CommandHandler::CommandResult CommandHandler::getSpecialEvents() {
    return CommandResult(true, "Non implementato");
}

CommandHandler::CommandResult CommandHandler::setSpecialEvent(const String& jsonPayload) {
    return CommandResult(false, "Non implementato");
}

CommandHandler::CommandResult CommandHandler::testMelody(const String& jsonPayload) {
    if (!_bellController) {
        return CommandResult(false, "BellController non disponibile");
    }

    DynamicJsonDocument doc(12288);
    DeserializationError derr = deserializeJson(doc, jsonPayload);
    if (derr) {
        return CommandResult(false, "JSON non valido");
    }

    if (doc.containsKey("notes") && doc["notes"].is<JsonArray>()) {
        JsonArray ns = doc["notes"].as<JsonArray>();
        const size_t maxSteps = min((size_t)MAX_MELODY_STEPS, ns.size());
        BellNote tempNotes[MAX_MELODY_STEPS];
        uint8_t count = 0;

        for (size_t i = 0; i < maxSteps; i++) {
            JsonObject n = ns[i];
            int b = n["bellNumber"] | 1;
            int d = n["duration"] | 300;
            int dl = n["delay"] | 800;
            if (b < 1 || b > 2) {
                continue;
            }
            tempNotes[count++] = { (uint8_t)b, (uint16_t)d, (uint16_t)dl };
        }

        if (count == 0) {
            return CommandResult(false, "Nessuna nota valida");
        }

        // Slot di prova unico: riusa quello "Test" già esistente, altrimenti il primo libero
        // (prima ogni prova occupava un nuovo slot fino a riempirli tutti)
        int playIdx = -1;
        for (int i = 0; i < Config::Limits::MAX_MELODIES; i++) {
            if (_bellController->getMelodyNoteCount(i) > 0 && String(_bellController->getMelodyName(i)) == "Test") {
                playIdx = i;
                break;
            }
        }
        for (int i = 0; playIdx < 0 && i < Config::Limits::MAX_MELODIES; i++) {
            if (_bellController->getMelodyNoteCount(i) == 0) {
                playIdx = i;
            }
        }
        if (playIdx < 0) {
            playIdx = Config::Limits::MAX_MELODIES - 1;
        }

        Result addRes = _bellController->updateMelody((uint8_t)playIdx, "Test", tempNotes, count);
        if (addRes.isError()) {
            return CommandResult(false, String("Impossibile preparare melodia test: ") + addRes.message);
        }

        Result playRes = _bellController->playMelody((uint8_t)playIdx);
        if (playRes.isError()) {
            return CommandResult(false, String("Test melodia fallito: ") + playRes.message);
        }

        CommandResult result(true, "Test melodia ad-hoc avviato");
        result.jsonData = new DynamicJsonDocument(128);
        (*result.jsonData)["index"] = playIdx;
        return result;
    }

    int melodyId = doc["melodyId"] | -1;
    if (melodyId < 0 || melodyId >= Config::Limits::MAX_MELODIES) {
        return CommandResult(false, "ID melodia non valido");
    }

    return playMelody((uint8_t)melodyId);
}

CommandHandler::CommandResult CommandHandler::saveMelody(const String& jsonPayload) {
    if (!_bellController) {
        return CommandResult(false, "BellController non disponibile");
    }

    DynamicJsonDocument doc(12288);
    DeserializationError err = deserializeJson(doc, jsonPayload);
    if (err) {
        return CommandResult(false, "JSON non valido");
    }

    String name = doc["name"] | "Senza nome";
    JsonArray ns = doc["notes"].as<JsonArray>();
    if (!ns || ns.size() == 0) {
        return CommandResult(false, "Note mancanti");
    }

    if (name.length() > 50) {
        return CommandResult(false, "Nome troppo lungo (max 50 caratteri)");
    }

    BellNote temp[MAX_MELODY_STEPS];
    uint8_t count = 0;
    for (JsonObject n : ns) {
        int b = n["bellNumber"] | 1;
        int d = n["duration"] | 300;
        int dl = n["delay"] | 800;
        if (b < 1 || b > 2) {
            continue;
        }
        temp[count++] = { (uint8_t)b, (uint16_t)d, (uint16_t)dl };
        if (count >= MAX_MELODY_STEPS) {
            break;
        }
    }

    if (count == 0) {
        return CommandResult(false, "Nessuna nota valida");
    }

    Result addRes = _bellController->addMelody(name.c_str(), temp, count);
    if (addRes.isError()) {
        return CommandResult(false, addRes.message);
    }

    int assigned = -1;
    for (int i = 0; i < Config::Limits::MAX_MELODIES; i++) {
        if (_bellController->getMelodyNoteCount(i) == count && String(_bellController->getMelodyName(i)) == name) {
            assigned = i;
            break;
        }
    }

    if (!saveAllMelodiesToFS()) {
        return CommandResult(false, "Melodia aggiunta ma salvataggio su filesystem fallito");
    }

    CommandResult result(true, "Melodia salvata");
    result.jsonData = new DynamicJsonDocument(128);
    (*result.jsonData)["index"] = assigned;
    return result;
}

CommandHandler::CommandResult CommandHandler::updateMelody(const String& jsonPayload) {
    if (!_bellController) {
        return CommandResult(false, "BellController non disponibile");
    }

    DynamicJsonDocument doc(12288);
    DeserializationError err = deserializeJson(doc, jsonPayload);
    if (err) {
        return CommandResult(false, "JSON non valido");
    }

    int idx = doc["index"] | -1;
    if (idx < 0 || idx >= Config::Limits::MAX_MELODIES) {
        return CommandResult(false, "index non valido");
    }

    String name = doc["name"] | "Senza nome";
    JsonArray ns = doc["notes"].as<JsonArray>();
    if (!ns || ns.size() == 0) {
        return CommandResult(false, "Note mancanti");
    }

    if (name.length() > 50) {
        return CommandResult(false, "Nome troppo lungo (max 50 caratteri)");
    }

    BellNote temp[MAX_MELODY_STEPS];
    uint8_t count = 0;
    for (JsonObject n : ns) {
        int b = n["bellNumber"] | 1;
        int d = n["duration"] | 300;
        int dl = n["delay"] | 800;
        if (b < 1 || b > 2) {
            continue;
        }
        temp[count++] = { (uint8_t)b, (uint16_t)d, (uint16_t)dl };
        if (count >= MAX_MELODY_STEPS) {
            break;
        }
    }

    if (count == 0) {
        return CommandResult(false, "Nessuna nota valida");
    }

    Result updateRes = _bellController->updateMelody((uint8_t)idx, name.c_str(), temp, count);
    if (updateRes.isError()) {
        return CommandResult(false, updateRes.message);
    }

    if (!saveAllMelodiesToFS()) {
        return CommandResult(false, "Melodia aggiornata ma salvataggio su filesystem fallito");
    }

    return CommandResult(true, "Melodia aggiornata");
}

CommandHandler::CommandResult CommandHandler::deleteMelody(uint8_t melodyIndex) {
    if (!_bellController) {
        return CommandResult(false, "BellController non disponibile");
    }

    if (melodyIndex >= Config::Limits::MAX_MELODIES) {
        return CommandResult(false, "index non valido");
    }

    Result deleteRes = _bellController->deleteMelody(melodyIndex);
    if (deleteRes.isError()) {
        return CommandResult(false, deleteRes.message);
    }

    if (!saveAllMelodiesToFS()) {
        return CommandResult(false, "Melodia eliminata ma salvataggio su filesystem fallito");
    }

    return CommandResult(true, "Melodia eliminata");
}

CommandHandler::CommandResult CommandHandler::setTime(const String& jsonPayload) {
    return CommandResult(false, "Non implementato");
}

CommandHandler::CommandResult CommandHandler::configureWiFi(const String& jsonPayload) {
    return CommandResult(false, "Non implementato");
}

CommandHandler::CommandResult CommandHandler::getNetworkConfig() {
    return CommandResult(true, "Non implementato");
}

CommandHandler::CommandResult CommandHandler::scanI2C() {
    return CommandResult(true, "Non implementato");
}

CommandHandler::CommandResult CommandHandler::restoreBackup(const String& jsonPayload) {
    return CommandResult(false, "Non implementato");
}
