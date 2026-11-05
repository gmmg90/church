#include "include/telegram_bot.h"
#include "include/bell_controller.h"
#include "include/config.h"
#include "include/logger.h"
#include "include/command_handler.h"
#include <esp_system.h>
#include <ArduinoJson.h>
#include <lwip/dns.h>

// Variabili globali esterne (solo quelle senza equivalente in SystemStatus)
extern bool testMode;
extern bool schedulerActive;
extern bool apMode;

// Dichiarazioni forward per accesso a variabili globali
extern SystemStatus systemStatus;

// Tutti i messaggi sono in testo semplice (niente Markdown): con il Markdown un
// carattere come "_" nel nome WiFi o in un nome utente fa rifiutare il messaggio
// da Telegram e la libreria non segnala nulla -> "non succede niente".

TelegramBotManager::TelegramBotManager()
    : _nextUpdateId(0),
      _enabled(false),
      _initialized(false),
      _lastUpdateTime(0),
      _outQueue(nullptr),
      _sending(false),
      _task(nullptr),
      _authorizedCount(0),
      _messageCount(0),
      _commandCount(0),
      _bellController(nullptr),
      _systemStatus(nullptr) {
}

static const char* TG_HOST = "api.telegram.org";

// Tastiera con pulsanti mostrata in chat agli utenti autorizzati.
// Ogni pulsante invia il suo testo: buttonToCommand() lo traduce nel comando.
struct TgButton { const char* label; const char* command; };
static const TgButton TG_BUTTONS[] = {
    {"⛪ Messa",      "/messa"},
    {"⚫ Funerale",   "/funerale"},
    {"🔔 Campana 1",  "/suona 1"},
    {"🔔 Campana 2",  "/suona 2"},
    {"⏹ STOP",       "/stop"},
    {"📊 Stato",      "/stato"},
    {"✅ Attiva",     "/attiva"},
    {"⛔ Disattiva",  "/disattiva"},
    {"🕐 Ora",        "/ora"},
    {"📚 Aiuto",      "/help"},
};
static const char* TG_KEYBOARD =
    "{\"keyboard\":["
    "[\"⛪ Messa\",\"⚫ Funerale\"],"
    "[\"🔔 Campana 1\",\"🔔 Campana 2\"],"
    "[\"⏹ STOP\",\"📊 Stato\"],"
    "[\"✅ Attiva\",\"⛔ Disattiva\"],"
    "[\"🕐 Ora\",\"📚 Aiuto\"]"
    "],\"resize_keyboard\":true,\"is_persistent\":true}";

static String buttonToCommand(const String& text) {
    for (const TgButton& b : TG_BUTTONS) {
        if (text == b.label) return b.command;
    }
    return "";
}

bool TelegramBotManager::begin(const char* token) {
    if (!token || strlen(token) == 0) {
        Logger::error("TelegramBot", "Token vuoto o invalido");
        return false;
    }

    // NOTA SICUREZZA: setInsecure() disabilita la verifica del certificato TLS.
    // Accettabile su rete locale fidata. Per verificarlo usare _client.setCACert(...)
    // con il certificato root di api.telegram.org.
    _client.setInsecure();
    _token = token;

    _outQueue = xQueueCreate(8, sizeof(OutMessage*));
    if (!_outQueue) {
        Logger::error("TelegramBot", "Impossibile creare coda messaggi");
        return false;
    }

    _initialized = true;
    _enabled = true;

    // Task su core 0 (il loop() gira sul core 1): stack ampio per l'handshake TLS
    if (xTaskCreatePinnedToCore(taskEntry, "telegram", 12288, this, 1, &_task, 0) != pdPASS) {
        Logger::error("TelegramBot", "Impossibile avviare il task del bot");
        _initialized = false;
        return false;
    }

    Logger::info("TelegramBot", "Bot inizializzato (task dedicato)");
    return true;
}

void TelegramBotManager::taskEntry(void* arg) {
    static_cast<TelegramBotManager*>(arg)->taskLoop();
}

void TelegramBotManager::taskLoop() {
    // Al primo collegamento si saltano i comandi rimasti in sospeso (Telegram li
    // conserva 24 h): dopo un riavvio non devono partire melodie chieste ore prima.
    bool flushed = false;
    bool startupSent = !TELEGRAM_NOTIFICATIONS_ENABLED;

    for (;;) {
        // Si sveglia ogni BOT_CHECK_INTERVAL, o subito se c'è un messaggio da inviare
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(BOT_CHECK_INTERVAL));

        if (!_enabled || WiFi.status() != WL_CONNECTED) {
            continue;
        }

        if (!flushed) {
            if (pollUpdates(true) >= 0) {
                flushed = true;
            }
            continue;
        }

        // Notifica di avvio solo dopo un periodo di funzionamento stabile:
        // se l'alimentazione fa ripartire l'ESP subito dopo l'accensione non arrivano doppioni
        if (!startupSent && millis() >= STARTUP_NOTIFY_DELAY) {
            sendNotification(buildStartupMessage());
            startupSent = true;
        }

        flushOutQueue();

        if (pollUpdates(false) > 0) {
            _lastUpdateTime = millis();
            flushOutQueue();
        }
    }
}

void TelegramBotManager::enqueue(const String& chatId, const String& text) {
    if (!_initialized || !_outQueue) {
        return;
    }
    OutMessage* msg = new OutMessage{chatId, text};
    if (xQueueSend(_outQueue, &msg, 0) != pdTRUE) {
        Logger::warn("TelegramBot", "Coda messaggi piena, messaggio scartato");
        delete msg;
        return;
    }
    if (_task) {
        xTaskNotifyGive(_task);
    }
}

void TelegramBotManager::waitForOutbox(uint32_t timeoutMs) {
    if (!_initialized || !_outQueue) {
        return;
    }
    uint32_t start = millis();
    while ((uxQueueMessagesWaiting(_outQueue) > 0 || _sending) && millis() - start < timeoutMs) {
        delay(50);
    }
}

void TelegramBotManager::flushOutQueue() {
    OutMessage* msg = nullptr;
    while (xQueueReceive(_outQueue, &msg, 0) == pdTRUE) {
        _sending = true;
        if (msg->chatId.length() == 0) {
            for (uint8_t i = 0; i < _authorizedCount; i++) {
                reply(_authorizedUsers[i], msg->text);
            }
        } else {
            reply(msg->chatId, msg->text);
        }
        delete msg;
        _sending = false;
    }
}

// --- HTTP verso Telegram -----------------------------------------------------
// Sostituisce UniversalTelegramBot, che smetteva di leggere la risposta appena i
// dati facevano una pausa: risposta a metà -> comandi mai letti e messaggi
// creduti falliti e rimandati (doppioni).

bool TelegramBotManager::readLine(String& line, uint32_t deadline) {
    line = "";
    while ((int32_t)(deadline - millis()) > 0) {
        while (_client.available()) {
            char c = _client.read();
            if (c == '\n') return true;
            if (c != '\r') line += c;
        }
        if (!_client.connected()) return false;
        delay(5);  // cede la CPU: niente watchdog
    }
    return false;
}

String TelegramBotManager::apiRequest(const String& method, const String& jsonBody, bool* gotReply) {
    if (gotReply) *gotReply = false;

    // Connessione mantenuta aperta tra un controllo e l'altro; se è caduta si riapre
    if (!_client.connected()) {
        _client.stop();

        // Il DNS del router a volte non risponde (e ogni tentativo blocca ~15 s):
        // Google DNS come riserva, e l'indirizzo di Telegram tenuto in memoria.
        static IPAddress tgIp;
        static uint32_t tgIpTime = 0;
        const IPAddress none(0, 0, 0, 0);
        if (tgIp == none || millis() - tgIpTime > 3600000UL) {
            ip_addr_t backup;
            IP_ADDR4(&backup, 8, 8, 8, 8);
            dns_setserver(1, &backup);
            IPAddress ip;
            if (WiFi.hostByName(TG_HOST, ip) == 1 && ip != none) {
                tgIp = ip;
                tgIpTime = millis();
            } else if (tgIp == none) {
                return "";
            }
        }
        if (!_client.connect(tgIp, 443, TG_HOST, nullptr, nullptr, nullptr)) {
            tgIp = none;  // indirizzo forse cambiato: al prossimo giro si risolve di nuovo
            return "";
        }
    }

    String req = "POST /bot" + _token + "/" + method + " HTTP/1.1\r\n";
    req += "Host: api.telegram.org\r\n";
    req += "Content-Type: application/json\r\n";
    req += "Connection: keep-alive\r\n";
    req += "Content-Length: " + String(jsonBody.length()) + "\r\n\r\n";
    req += jsonBody;
    if (_client.print(req) != req.length()) {
        _client.stop();
        return "";
    }

    const uint32_t deadline = millis() + 10000;
    String line;

    // Riga di stato + intestazioni
    if (!readLine(line, deadline)) {
        _client.stop();
        return "";
    }
    if (gotReply) *gotReply = true;  // Telegram ha ricevuto la richiesta

    int contentLength = -1;
    bool keepAlive = true;
    while (readLine(line, deadline)) {
        if (line.length() == 0) break;  // fine intestazioni
        String l = line;
        l.toLowerCase();
        if (l.startsWith("content-length:")) contentLength = l.substring(15).toInt();
        if (l.startsWith("connection:") && l.indexOf("close") >= 0) keepAlive = false;
    }

    // Corpo: esattamente Content-Length byte (o fino alla chiusura)
    String body;
    if (contentLength > 0) body.reserve(contentLength);
    while ((int32_t)(deadline - millis()) > 0) {
        while (_client.available() && (contentLength < 0 || (int)body.length() < contentLength)) {
            body += (char)_client.read();
        }
        if (contentLength >= 0 && (int)body.length() >= contentLength) break;
        if (!_client.connected() && !_client.available()) break;
        delay(5);
    }

    if (!keepAlive || (contentLength >= 0 && (int)body.length() < contentLength)) {
        _client.stop();
    }
    return body;
}

// Legge i nuovi messaggi e li esegue. skipOld = salta quelli in sospeso.
// Ritorna il numero di messaggi elaborati, -1 se la richiesta è fallita.
int TelegramBotManager::pollUpdates(bool skipOld) {
    String body = skipOld
        ? apiRequest("getUpdates", "{\"offset\":-1,\"limit\":1,\"timeout\":0}")
        : apiRequest("getUpdates", "{\"offset\":" + String(_nextUpdateId) + ",\"limit\":5,\"timeout\":0}");
    if (body.length() == 0) {
        return -1;
    }

    // Solo i campi necessari: memoria contenuta anche con messaggi lunghi
    StaticJsonDocument<256> filter;
    filter["ok"] = true;
    filter["result"][0]["update_id"] = true;
    filter["result"][0]["message"]["chat"]["id"] = true;
    filter["result"][0]["message"]["text"] = true;
    filter["result"][0]["message"]["from"]["first_name"] = true;

    DynamicJsonDocument doc(3072);
    DeserializationError err = deserializeJson(doc, body, DeserializationOption::Filter(filter));
    if (err || !(doc["ok"] | true)) {
        Logger::warn("TelegramBot", "Risposta getUpdates non valida (%s)", err.c_str());
        return -1;
    }

    JsonArray results = doc["result"].as<JsonArray>();
    int handled = 0;
    for (JsonObject r : results) {
        long id = r["update_id"] | 0L;
        if (id >= _nextUpdateId) _nextUpdateId = id + 1;
        if (skipOld) continue;

        JsonObject m = r["message"];
        if (m.isNull()) continue;
        String chatId = String((long long)(m["chat"]["id"] | 0LL));
        String text = m["text"] | "";
        String userName = m["from"]["first_name"] | "";

        _messageCount++;
        handled++;
        Logger::info("TelegramBot", "Messaggio da %s (%s): %s", userName.c_str(), chatId.c_str(), text.c_str());

        String fromButton = buttonToCommand(text);
        if (fromButton.length() > 0) {
            text = fromButton;
        }

        if (text.startsWith("/")) {
            _commandCount++;
            handleCommand(chatId, text, userName);
        } else {
            reply(chatId, "Comando non riconosciuto. Usa /help per vedere i comandi.");
        }
    }

    if (skipOld && results.size() > 0) {
        Logger::info("TelegramBot", "Saltati i messaggi in sospeso");
    }
    return handled;
}

// Il messaggio parte UNA volta: si riprova solo se Telegram non ha ricevuto nulla
// (connessione non aperta o caduta prima della risposta), quindi niente doppioni.
void TelegramBotManager::reply(const String& chatId, const String& text) {
    DynamicJsonDocument payload(text.length() + 128);
    payload["chat_id"] = chatId;
    payload["text"] = text;
    if (isAuthorized(chatId)) {
        payload["reply_markup"] = serialized(TG_KEYBOARD);  // tastiera con i pulsanti
    }
    String json;
    serializeJson(payload, json);

    for (uint8_t attempt = 0; attempt < 2; attempt++) {
        bool gotReply = false;
        String body = apiRequest("sendMessage", json, &gotReply);
        if (gotReply) {
            if (body.indexOf("\"ok\":true") < 0) {
                Logger::warn("TelegramBot", "Invio a %s rifiutato: %s", chatId.c_str(), body.c_str());
            }
            return;
        }
        delay(500);
    }
    Logger::warn("TelegramBot", "Invio a %s fallito (connessione)", chatId.c_str());
}

void TelegramBotManager::handleCommand(const String& chatId, const String& text, const String& userName) {
    // Estrai comando e argomenti
    String command = text;
    String args = "";

    int spaceIndex = text.indexOf(' ');
    if (spaceIndex > 0) {
        command = text.substring(0, spaceIndex);
        args = text.substring(spaceIndex + 1);
    }

    // "/help@NomeBot" (comando cliccato in un gruppo) -> "/help"
    int atIndex = command.indexOf('@');
    if (atIndex > 0) {
        command = command.substring(0, atIndex);
    }

    command.toLowerCase();

    // /start e /auth sono sempre permessi (per primo accesso)
    if (command == "/start") {
        cmdStart(chatId, userName);
        return;
    }

    if (command == "/auth") {
        cmdAuth(chatId, userName);
        return;
    }

    // Altri comandi richiedono autorizzazione
    if (!isAuthorized(chatId)) {
        reply(chatId, "🚫 Non sei autorizzato. Usa /auth per richiedere l'accesso.");
        Logger::warn("TelegramBot", "Tentativo accesso non autorizzato da %s (%s)", userName.c_str(), chatId.c_str());
        return;
    }

    // Gestione comandi
    if (command == "/help" || command == "/aiuto") {
        cmdHelp(chatId);
    } else if (command == "/status" || command == "/stato") {
        cmdStatus(chatId);
    } else if (command == "/enable" || command == "/attiva") {
        cmdEnable(chatId);
    } else if (command == "/disable" || command == "/disattiva") {
        cmdDisable(chatId);
    } else if (command == "/ring" || command == "/suona") {
        cmdRing(chatId, args);
    } else if (command == "/stop" || command == "/ferma") {
        cmdStop(chatId);
    } else if (command == "/funeral" || command == "/funerale") {
        cmdFuneral(chatId);
    } else if (command == "/mass" || command == "/messa") {
        cmdMass(chatId);
    } else if (command == "/melody" || command == "/melodia") {
        cmdMelody(chatId, args);
    } else if (command == "/info") {
        cmdInfo(chatId);
    } else if (command == "/ntp") {
        cmdNTP(chatId);
    } else if (command == "/time" || command == "/ora") {
        cmdTime(chatId);
    } else {
        reply(chatId, "Comando sconosciuto. Usa /help per la lista comandi.");
    }
}

void TelegramBotManager::cmdStart(const String& chatId, const String& userName) {
    String welcome = "🔔 Sistema Campane\n\nCiao " + userName + "!\n\n";
    if (!isAuthorized(chatId)) {
        welcome += "Non sei ancora autorizzato.\n";
        welcome += "Il tuo Chat ID è: " + chatId + "\n";
        welcome += "Comunicalo all'amministratore.";
    } else {
        welcome += "Sei autorizzato. Usa /help per i comandi.";
    }
    reply(chatId, welcome);
}

void TelegramBotManager::cmdHelp(const String& chatId) {
    String help = "📚 Comandi\n\n";
    help += "/messa - Chiamata messa\n";
    help += "/funerale - Scampanio funerale\n";
    help += "/melodia N - Melodia numero N (0-9)\n";
    help += "/stop - Ferma la melodia\n";
    help += "/suona 1 - Un colpo sulla campana 1 (o 2)\n\n";
    help += "/attiva - Abilita le campane\n";
    help += "/disattiva - Disabilita le campane\n\n";
    help += "/stato - Stato del sistema\n";
    help += "/ora - Ora del sistema\n";
    help += "/ntp - Risincronizza l'ora\n";
    help += "/info - Info tecniche";
    reply(chatId, help);
}

void TelegramBotManager::cmdStatus(const String& chatId) {
    bool enabled = _systemStatus ? _systemStatus->bellsEnabled : false;
    String status = "📊 Stato\n\n";
    status += "Campane: " + String(enabled ? "ABILITATE" : "DISABILITATE") + "\n";
    status += "Programmazione: " + String(schedulerActive ? "attiva" : "ferma") + "\n";
    if (_bellController && _bellController->isPlayingMelody()) {
        status += "In corso: " + String(_bellController->getMelodyName(_bellController->getCurrentMelodyIndex())) + "\n";
    }
    if (testMode) {
        status += "Modalità test: ON\n";
    }
    status += "\n";

    if (WiFi.status() == WL_CONNECTED) {
        status += "WiFi: " + String(WiFi.SSID()) + " (" + String(WiFi.RSSI()) + " dBm)\n";
        status += "IP: " + WiFi.localIP().toString() + "\n";
    } else {
        status += "WiFi: disconnesso\n";
    }
    status += "Acceso da: " + formatUptime() + "\n";
    if (_systemStatus && _systemStatus->esp32Temperature > 0) {
        status += "Temperatura ESP32: " + String(_systemStatus->esp32Temperature, 1) + "°C\n";
    }
    reply(chatId, status);
}

void TelegramBotManager::cmdEnable(const String& chatId) {
    CommandHandler::CommandResult result = CommandHandler::getInstance().toggleBells(true);
    reply(chatId, result.success ? "✅ Campane ABILITATE" : "❌ Errore: " + result.message);
    Logger::info("TelegramBot", "Campane attivate da Telegram");
}

void TelegramBotManager::cmdDisable(const String& chatId) {
    CommandHandler::CommandResult result = CommandHandler::getInstance().toggleBells(false);
    reply(chatId, result.success ? "⛔ Campane DISABILITATE" : "❌ Errore: " + result.message);
    Logger::info("TelegramBot", "Campane disattivate da Telegram");
}

void TelegramBotManager::cmdRing(const String& chatId, const String& args) {
    // /suona <campana> [durata ms] - impulso breve: il martello deve solo battere
    int bellNumber = 1;
    int duration = 200;

    if (args.length() > 0) {
        int spaceIdx = args.indexOf(' ');
        if (spaceIdx > 0) {
            bellNumber = args.substring(0, spaceIdx).toInt();
            duration = args.substring(spaceIdx + 1).toInt();
        } else {
            bellNumber = args.toInt();
        }
    }

    if (bellNumber < 1 || bellNumber > 2) {
        reply(chatId, "❌ Campana non valida (1 o 2)");
        return;
    }
    if (duration < BELL_MIN_PULSE || duration > BELL_MAX_PULSE) {
        reply(chatId, "❌ Durata non valida (" + String(BELL_MIN_PULSE) + "-" + String(BELL_MAX_PULSE) + " ms)");
        return;
    }

    CommandHandler::CommandResult result = CommandHandler::getInstance().ringBell((uint8_t)bellNumber, (uint16_t)duration);
    reply(chatId, result.success ? "🔔 " + result.message : "❌ Errore: " + result.message);
}

void TelegramBotManager::cmdStop(const String& chatId) {
    CommandHandler::CommandResult result = CommandHandler::getInstance().stopMelody();
    reply(chatId, result.success ? "⏹ Melodia fermata" : "❌ " + result.message);
}

void TelegramBotManager::cmdFuneral(const String& chatId) {
    CommandHandler::CommandResult result = CommandHandler::getInstance().playFuneral();
    if (result.success) {
        reply(chatId, "⚫ Scampanio funerale avviato");
        Logger::info("TelegramBot", "Scampanio funerale avviato da Telegram");
    } else {
        reply(chatId, "❌ Errore: " + result.message);
    }
}

void TelegramBotManager::cmdMass(const String& chatId) {
    CommandHandler::CommandResult result = CommandHandler::getInstance().playMass();
    if (result.success) {
        reply(chatId, "⛪ Chiamata messa avviata");
        Logger::info("TelegramBot", "Chiamata messa avviata da Telegram");
    } else {
        reply(chatId, "❌ Errore: " + result.message);
    }
}

void TelegramBotManager::cmdMelody(const String& chatId, const String& args) {
    if (!_bellController) {
        reply(chatId, "❌ Controller campane non disponibile");
        return;
    }
    if (args.length() == 0) {
        reply(chatId, "Indica il numero della melodia. Esempio: /melodia 2");
        return;
    }

    int melodyIndex = args.toInt();
    if (melodyIndex < 0 || melodyIndex > 9 || _bellController->getMelodyNoteCount(melodyIndex) == 0) {
        reply(chatId, "❌ Melodia " + args + " non configurata");
        return;
    }

    CommandHandler::CommandResult result = CommandHandler::getInstance().playMelody((uint8_t)melodyIndex);
    if (result.success) {
        reply(chatId, "🎵 " + String(_bellController->getMelodyName(melodyIndex)) + " avviata");
        Logger::info("TelegramBot", "Melodia %d avviata da Telegram", melodyIndex);
    } else {
        reply(chatId, "❌ Errore: " + result.message);
    }
}

void TelegramBotManager::cmdInfo(const String& chatId) {
    String info = "ℹ️ Info\n\n";
    info += "Firmware: " + String(Config::FIRMWARE_VERSION) + "\n";
    info += "Memoria libera: " + String(ESP.getFreeHeap() / 1024) + " KB\n";
    info += "Messaggi ricevuti: " + String(_messageCount) + "\n";
    info += "Utenti autorizzati: " + String(_authorizedCount);
    reply(chatId, info);
}

void TelegramBotManager::cmdAuth(const String& chatId, const String& userName) {
    if (isAuthorized(chatId)) {
        reply(chatId, "✅ Sei già autorizzato");
        return;
    }
    String msg = "🔐 Richiesta di accesso\n\n";
    msg += "Nome: " + userName + "\n";
    msg += "Chat ID: " + chatId + "\n\n";
    msg += "L'amministratore deve aggiungere questo Chat ID dalla pagina web (Impostazioni > Telegram).";
    reply(chatId, msg);
    Logger::info("TelegramBot", "Richiesta autorizzazione da %s (%s)", userName.c_str(), chatId.c_str());
}

void TelegramBotManager::cmdNTP(const String& chatId) {
    CommandHandler::CommandResult result = CommandHandler::getInstance().resyncNTP();
    reply(chatId, result.success ? "🕐 Risincronizzazione ora avviata" : "❌ Errore: " + result.message);
}

void TelegramBotManager::cmdTime(const String& chatId) {
    CommandHandler::CommandResult result = CommandHandler::getInstance().getCurrentTime();
    if (result.success && result.jsonData) {
        JsonDocument& doc = *result.jsonData;
        char buf[40];
        snprintf(buf, sizeof(buf), "🕐 %02d:%02d - %02d/%02d/%04d",
                 doc["hour"].as<int>(), doc["min"].as<int>(),
                 doc["day"].as<int>(), doc["month"].as<int>(), doc["year"].as<int>());
        reply(chatId, buf);
    } else {
        reply(chatId, "❌ Ora non sincronizzata");
    }
}

void TelegramBotManager::sendNotification(const String& message) {
    enqueue("", message);
}

void TelegramBotManager::sendMessage(const String& chatId, const String& message) {
    enqueue(chatId, message);
}

void TelegramBotManager::setEnabled(bool enabled) {
    _enabled = enabled;
    Logger::info("TelegramBot", enabled ? "Bot abilitato" : "Bot disabilitato");
}

bool TelegramBotManager::addAuthorizedUser(const String& chatId) {
    if (isAuthorized(chatId)) {
        return true; // Già presente
    }
    if (_authorizedCount >= 10) {
        Logger::error("TelegramBot", "Limite massimo utenti raggiunto (10)");
        return false;
    }
    _authorizedUsers[_authorizedCount++] = chatId;
    Logger::info("TelegramBot", "Aggiunto utente autorizzato: %s", chatId.c_str());
    return true;
}

bool TelegramBotManager::removeAuthorizedUser(const String& chatId) {
    for (uint8_t i = 0; i < _authorizedCount; i++) {
        if (_authorizedUsers[i] == chatId) {
            // Sposta gli elementi successivi
            for (uint8_t j = i; j < _authorizedCount - 1; j++) {
                _authorizedUsers[j] = _authorizedUsers[j + 1];
            }
            _authorizedCount--;
            Logger::info("TelegramBot", "Rimosso utente autorizzato: %s", chatId.c_str());
            return true;
        }
    }
    return false;
}

bool TelegramBotManager::isAuthorized(const String& chatId) const {
    for (uint8_t i = 0; i < _authorizedCount; i++) {
        if (_authorizedUsers[i] == chatId) {
            return true;
        }
    }
    return false;
}

String TelegramBotManager::getAuthorizedUsers() const {
    String users = "";
    for (uint8_t i = 0; i < _authorizedCount; i++) {
        if (i > 0) users += ", ";
        users += _authorizedUsers[i];
    }
    return users;
}

String TelegramBotManager::formatUptime() const {
    unsigned long uptime = millis() / 1000;
    unsigned long days = uptime / 86400;
    unsigned long hours = (uptime % 86400) / 3600;
    unsigned long minutes = (uptime % 3600) / 60;

    String result = "";
    if (days > 0) result += String(days) + "g ";
    if (hours > 0 || days > 0) result += String(hours) + "h ";
    result += String(minutes) + "m";
    return result;
}

String TelegramBotManager::buildStartupMessage() const {
    const char* reason;
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON:  reason = "accensione"; break;
        case ESP_RST_SW:       reason = "riavvio software"; break;
        case ESP_RST_PANIC:    reason = "errore firmware"; break;
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
        case ESP_RST_WDT:      reason = "watchdog"; break;
        case ESP_RST_BROWNOUT: reason = "calo di tensione"; break;
        case ESP_RST_EXT:      reason = "pulsante reset"; break;
        default:               reason = "altro"; break;
    }
    String msg = "🔔 Sistema campane avviato\n\n";
    msg += "Motivo: " + String(reason) + "\n";
    msg += "IP: " + WiFi.localIP().toString() + "\n";
    msg += "Firmware: " + String(Config::FIRMWARE_VERSION) + "\n\n";
    msg += "/help per i comandi";
    return msg;
}
