// =============================================================================
// telegram_bot.cpp
// Bot Telegram per il controllo campane della chiesa (ESP32 / Arduino framework)
// Polling HTTP puro: HTTPClient + WiFiClientSecure + ArduinoJson
// =============================================================================

#include "include/telegram_bot.h"
#include "include/config.h"
#include "include/bell_controller.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <time.h>

// ---------------------------------------------------------------------------
// Extern declarations
// ---------------------------------------------------------------------------
extern SystemStatus   systemStatus;
extern BellController bellController;

// Forward-declare log helpers defined in main.cpp
extern void   addLog(const String& msg);
extern String getRecentLogs(int n);

// ---------------------------------------------------------------------------
// Global instance
// ---------------------------------------------------------------------------
TelegramBotController telegramBot;

// ---------------------------------------------------------------------------
// Telegram API base
// ---------------------------------------------------------------------------
static const char* TG_HOST = "https://api.telegram.org/bot";

// =============================================================================
// PUBLIC — Lifecycle
// =============================================================================

bool TelegramBotController::begin() {
    loadConfig();

    if (strlen(token) < 10) {
        Serial.println("[TG] Token non configurato o troppo corto — bot disabilitato.");
        enabled = false;
        return false;
    }

    enabled      = true;
    lastUpdateId = 0;
    lastPollTime = 0;
    lastLogTime  = 0;

    Serial.printf("[TG] Bot avviato. Token: %s\n", getTokenMasked().c_str());
    return true;
}

void TelegramBotController::update() {
    if (!enabled) return;
    if (WiFi.status() != WL_CONNECTED) return;

    uint32_t now = millis();

    // Hourly log broadcast
    if (hourlyLogEnabled && lastLogTime > 0 &&
        (now - lastLogTime) >= TELEGRAM_LOG_INTERVAL) {
        sendHourlyLog();
        lastLogTime = now;
    }
    // First-time init of log timer
    if (lastLogTime == 0) {
        lastLogTime = now;
    }

    // Poll for new messages
    if ((now - lastPollTime) >= TELEGRAM_POLL_INTERVAL) {
        pollUpdates();
        lastPollTime = now;
    }
}

// =============================================================================
// PRIVATE — Poll & dispatch
// =============================================================================

bool TelegramBotController::pollUpdates() {
    WiFiClientSecure wifiClient;
    wifiClient.setInsecure(); // No certificate validation on embedded device

    HTTPClient http;
    String url = String(TG_HOST) + token +
                 "/getUpdates?offset=" + String(lastUpdateId + 1) +
                 "&timeout=0";

    http.begin(wifiClient, url);
    http.setTimeout(5000);

    int httpCode = http.GET();
    if (httpCode != 200) {
        if (httpCode > 0) {
            Serial.printf("[TG] pollUpdates HTTP %d\n", httpCode);
        }
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, payload);
    if (err || !doc["ok"].as<bool>()) {
        Serial.printf("[TG] JSON parse error: %s\n", err.c_str());
        return false;
    }

    JsonArray results = doc["result"].as<JsonArray>();
    for (JsonObject update : results) {
        long updateId = update["update_id"].as<long>();
        if (updateId > lastUpdateId) lastUpdateId = updateId;

        // --- Text message ---
        if (update.containsKey("message")) {
            JsonObject msg    = update["message"];
            String chatId     = msg["chat"]["id"].as<String>();
            String text       = msg["text"] | "";
            String firstName  = msg["from"]["first_name"] | "";
            String lastName   = msg["from"]["last_name"]  | "";
            String fromName   = (lastName.length() > 0)
                                ? firstName + " " + lastName
                                : firstName;
            if (text.length() > 0) {
                handleTextMessage(chatId, fromName, text);
            }
        }

        // --- Callback query (inline keyboard) ---
        if (update.containsKey("callback_query")) {
            JsonObject cb = update["callback_query"];
            String chatId    = cb["message"]["chat"]["id"].as<String>();
            String data      = cb["data"] | "";
            String firstName = cb["from"]["first_name"] | "";
            String lastName  = cb["from"]["last_name"]  | "";
            String fromName  = (lastName.length() > 0)
                               ? firstName + " " + lastName
                               : firstName;

            // Answer the callback to remove the "loading" spinner in the app
            {
                WiFiClientSecure wc2;
                wc2.setInsecure();
                HTTPClient ha;
                String answerUrl = String(TG_HOST) + token + "/answerCallbackQuery";
                ha.begin(wc2, answerUrl);
                ha.addHeader("Content-Type", "application/json");
                String cbId = cb["id"] | "";
                ha.POST("{\"callback_query_id\":\"" + cbId + "\"}");
                ha.end();
            }

            if (data.length() > 0) {
                handleCallback(chatId, fromName, data);
            }
        }
    }
    return true;
}

// =============================================================================
// PRIVATE — Command handlers
// =============================================================================

void TelegramBotController::handleTextMessage(String chatId,
                                              String fromName,
                                              String text) {
    text.trim();

    if (!isAuthorized(chatId)) {
        sendMessage(chatId,
            "⛔ Non autorizzato\\. Il tuo Chat ID: `" + chatId + "`",
            "MarkdownV2");
        return;
    }

    // Log (troncato a 30 caratteri)
    String logText = text.substring(0, min((int)text.length(), 30));
    addLog("TG[" + fromName + "]: " + logText);

    // --- Command dispatch ---
    if (text == "/start" || text == "/help") {
        sendHelpKeyboard(chatId);

    } else if (text == "/stato") {
        sendStatus(chatId);

    } else if (text == "/melodie") {
        sendMelodyList(chatId);

    } else if (text.startsWith("/suona ")) {
        int n = text.substring(7).toInt();
        bellController.enableTestMode(true);
        bellController.playMelody((uint8_t)n);
        sendMessage(chatId, "🔔 Riproduco melodia " + String(n));

    } else if (text == "/funerale") {
        bellController.enableTestMode(true);
        bellController.playMelody(0);
        sendMessage(chatId, "⚫ Riproduco melodia FUNERALE");

    } else if (text == "/messa") {
        bellController.enableTestMode(true);
        bellController.playMelody(1);
        sendMessage(chatId, "⛪ Riproduco melodia CHIAMATA MESSA");

    } else if (text == "/stop") {
        bellController.emergencyStop();
        sendMessage(chatId, "🛑 Stop emergenza eseguito\\!");

    } else if (text == "/campane on") {
        bellController.setEnabled(true);
        sendMessage(chatId, "✅ Campane *abilitate*");

    } else if (text == "/campane off") {
        bellController.setEnabled(false);
        sendMessage(chatId, "🔇 Campane *disabilitate*");

    } else if (text == "/test 1") {
        bellController.enableTestMode(true);
        bellController.ringBell(1, 500);
        sendMessage(chatId, "🔔 Test Campana 1 eseguito");

    } else if (text == "/test 2") {
        bellController.enableTestMode(true);
        bellController.ringBell(2, 500);
        sendMessage(chatId, "🔔 Test Campana 2 eseguito");

    } else if (text == "/log") {
        sendHourlyLog();

    } else if (text == "/chatid") {
        sendMessage(chatId, "🆔 Il tuo Chat ID: `" + chatId + "`");

    } else {
        sendMessage(chatId,
            "❓ Comando non riconosciuto\\. Usa /help per vedere i comandi disponibili\\.");
    }
}

void TelegramBotController::handleCallback(String chatId,
                                           String fromName,
                                           String data) {
    if (!isAuthorized(chatId)) {
        sendMessage(chatId, "⛔ Non autorizzato. Il tuo Chat ID: " + chatId);
        return;
    }

    addLog("TG[" + fromName + "]: cb:" + data.substring(0, min((int)data.length(), 30)));

    if (data == "play_0") {
        bellController.enableTestMode(true);
        bellController.playMelody(0);
        sendMessage(chatId, "⚫ Riproduco melodia FUNERALE");

    } else if (data == "play_1") {
        bellController.enableTestMode(true);
        bellController.playMelody(1);
        sendMessage(chatId, "⛪ Riproduco melodia CHIAMATA MESSA");

    } else if (data.startsWith("play_")) {
        int n = data.substring(5).toInt();
        bellController.enableTestMode(true);
        bellController.playMelody((uint8_t)n);
        sendMessage(chatId, "🔔 Riproduco melodia " + String(n));

    } else if (data == "stop") {
        bellController.emergencyStop();
        sendMessage(chatId, "🛑 Stop emergenza eseguito!");

    } else if (data == "stato") {
        sendStatus(chatId);

    } else if (data == "bells_on") {
        bellController.setEnabled(true);
        sendMessage(chatId, "✅ Campane *abilitate*");

    } else if (data == "bells_off") {
        bellController.setEnabled(false);
        sendMessage(chatId, "🔇 Campane *disabilitate*");

    } else if (data == "test1") {
        bellController.enableTestMode(true);
        bellController.ringBell(1, 500);
        sendMessage(chatId, "🔔 Test Campana 1 eseguito");

    } else if (data == "test2") {
        bellController.enableTestMode(true);
        bellController.ringBell(2, 500);
        sendMessage(chatId, "🔔 Test Campana 2 eseguito");

    } else if (data == "melodie") {
        sendMelodyList(chatId);

    } else {
        sendMessage(chatId, "❓ Azione non riconosciuta: " + data);
    }
}

// =============================================================================
// PRIVATE — Composed messages
// =============================================================================

void TelegramBotController::sendHelpKeyboard(String chatId) {
    String text =
        "🔔 *Bot Campane Chiesa*\n\n"
        "Comandi disponibili:\n"
        "/stato \\- Stato del sistema\n"
        "/melodie \\- Lista melodie disponibili\n"
        "/funerale \\- Suona melodia funerale\n"
        "/messa \\- Suona chiamata messa\n"
        "/suona N \\- Suona melodia numero N\n"
        "/stop \\- Stop emergenza\n"
        "/campane on \\- Abilita campane\n"
        "/campane off \\- Disabilita campane\n"
        "/test 1 \\- Test campana 1\n"
        "/test 2 \\- Test campana 2\n"
        "/log \\- Report di stato\n"
        "/chatid \\- Mostra il tuo Chat ID\n"
        "/help \\- Mostra questo messaggio";

    // Build inline keyboard JSON string
    String keyboard =
        "["
          "[{\"text\":\"⚫ FUNERALE\",\"callback_data\":\"play_0\"},"
           "{\"text\":\"⛪ MESSA\",\"callback_data\":\"play_1\"}],"
          "[{\"text\":\"📊 Stato\",\"callback_data\":\"stato\"},"
           "{\"text\":\"🎵 Melodie\",\"callback_data\":\"melodie\"}],"
          "[{\"text\":\"🔔 Test C1\",\"callback_data\":\"test1\"},"
           "{\"text\":\"🔔 Test C2\",\"callback_data\":\"test2\"}],"
          "[{\"text\":\"✅ Campane ON\",\"callback_data\":\"bells_on\"},"
           "{\"text\":\"🔇 OFF\",\"callback_data\":\"bells_off\"}],"
          "[{\"text\":\"🛑 STOP EMERGENZA\",\"callback_data\":\"stop\"}]"
        "]";

    sendMessageWithKeyboard(chatId, text, keyboard);
}

void TelegramBotController::sendStatus(String chatId) {
    bool playing      = bellController.isPlayingMelody();
    String melodyName = playing
                        ? String(bellController.getMelodyName(systemStatus.activeMelody))
                        : "—";

    // Get current time
    time_t now = time(nullptr);
    struct tm* tinfo = localtime(&now);
    char timeBuf[20];
    strftime(timeBuf, sizeof(timeBuf), "%d/%m/%Y %H:%M", tinfo);

    String msg =
        "📊 *Stato Sistema Campane*\n"
        "🕐 " + String(timeBuf) + "\n\n"
        "📶 WiFi: "        + (systemStatus.wifiConnected   ? "✅" : "❌") + "\n"
        "🕰 RTC: "         + (systemStatus.rtcConnected     ? "✅" : "❌") + "\n"
        "🌐 NTP: "         + (systemStatus.ntpSynced        ? "✅" : "❌") + "\n"
        "🔔 Campane: "     + (systemStatus.bellsEnabled     ? "✅ Abilitate" : "🔇 Disabilitate") + "\n"
        "🧪 Test mode: "   + (systemStatus.testMode         ? "🟡 Attivo" : "⚫ Inattivo") + "\n"
        "🎵 Melodia: "     + (playing ? "▶ " + melodyName : "⏹ Nessuna") + "\n"
        "🔢 Suonate tot: " + String(systemStatus.totalBellRings) + "\n"
        "🌡 Temperatura: " + String(systemStatus.esp32Temperature, 1) + "°C"
        + (systemStatus.thermalProtection ? " 🛑 PROTEZIONE ATTIVA" :
           systemStatus.temperatureWarning ? " ⚠ AVVISO CALORE" : " ✅") + "\n";

    sendMessage(chatId, msg);
}

void TelegramBotController::sendMelodyList(String chatId) {
    int total = bellController.getMelodyCount();
    String msg = "🎵 *Melodie disponibili:*\n\n";

    // Build message text and inline keyboard rows
    String keyboard = "[";
    bool firstRow = true;
    bool openRow  = false;
    int colCount  = 0;

    for (int i = 0; i < total && i < 10; i++) {
        int noteCount = bellController.getMelodyNoteCount((uint8_t)i);
        if (noteCount <= 0) continue;

        const char* name = bellController.getMelodyName((uint8_t)i);
        msg += String(i) + ". " + String(name) + "\n";

        // Add button: max 2 per row
        if (colCount == 0) {
            if (!firstRow) keyboard += ",";
            keyboard += "[";
            openRow   = true;
            firstRow  = false;
        } else {
            keyboard += ",";
        }
        keyboard += "{\"text\":\"" + String(i) + ". " + String(name) +
                    "\",\"callback_data\":\"play_" + String(i) + "\"}";
        colCount++;
        if (colCount >= 2) {
            keyboard += "]";
            openRow   = false;
            colCount  = 0;
        }
    }
    if (openRow) keyboard += "]";
    keyboard += "]";

    if (msg == "🎵 *Melodie disponibili:*\n\n") {
        sendMessage(chatId, "🎵 Nessuna melodia configurata.");
        return;
    }

    sendMessageWithKeyboard(chatId, msg, keyboard);
}

void TelegramBotController::sendHourlyLog() {
    time_t now = time(nullptr);
    struct tm* tinfo = localtime(&now);
    char timeBuf[20];
    strftime(timeBuf, sizeof(timeBuf), "%d/%m/%Y %H:%M", tinfo);

    bool playing      = bellController.isPlayingMelody();
    String melodyName = playing
                        ? String(bellController.getMelodyName(systemStatus.activeMelody))
                        : "—";

    String msg =
        "📊 *Report Orario \\- Campane*\n"
        "🕐 " + String(timeBuf) + "\n\n"

        "📶 WiFi: "        + (systemStatus.wifiConnected   ? "✅" : "❌") + "\n"
        "🕰 RTC: "         + (systemStatus.rtcConnected     ? "✅" : "❌") + "\n"
        "🌐 NTP: "         + (systemStatus.ntpSynced        ? "✅" : "❌") + "\n"
        "🔔 Campane: "     + (systemStatus.bellsEnabled     ? "✅ Abilitate" : "🔇 Disabilitate") + "\n"
        "🧪 Test mode: "   + (systemStatus.testMode         ? "🟡 Attivo" : "⚫ Inattivo") + "\n"
        "🎵 Melodia: "     + (playing ? "▶ " + melodyName : "⏹ Nessuna") + "\n"
        "🔢 Suonate tot: " + String(systemStatus.totalBellRings) + "\n"
        "🌡 Temperatura: " + String(systemStatus.esp32Temperature, 1) + "°C"
        + (systemStatus.thermalProtection ? " 🛑 PROTEZIONE ATTIVA" :
           systemStatus.temperatureWarning ? " ⚠ AVVISO CALORE" : " ✅") + "\n\n"

        "📝 *Ultimi eventi:*\n" +
        getRecentLogs(15);

    sendMessageToAll(msg);
}

// =============================================================================
// PUBLIC — Messaging
// =============================================================================

bool TelegramBotController::sendMessage(String chatId,
                                        String text,
                                        String parseMode) {
    if (!enabled || strlen(token) < 10) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure wifiClient;
    wifiClient.setInsecure();

    HTTPClient http;
    String url = String(TG_HOST) + token + "/sendMessage";
    http.begin(wifiClient, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    DynamicJsonDocument doc(512);
    doc["chat_id"]    = chatId;
    doc["text"]       = text;
    doc["parse_mode"] = parseMode;

    String body;
    serializeJson(doc, body);

    int httpCode = http.POST(body);
    http.end();

    if (httpCode != 200) {
        Serial.printf("[TG] sendMessage HTTP %d\n", httpCode);
        return false;
    }
    return true;
}

bool TelegramBotController::sendMessageWithKeyboard(String chatId,
                                                    String text,
                                                    String keyboard) {
    if (!enabled || strlen(token) < 10) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure wifiClient;
    wifiClient.setInsecure();

    HTTPClient http;
    String url = String(TG_HOST) + token + "/sendMessage";
    http.begin(wifiClient, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(5000);

    // Build JSON body manually to avoid ArduinoJson nesting limitations
    String body = "{\"chat_id\":\"" + chatId + "\","
                  "\"text\":\""     + escapeJson(text) + "\","
                  "\"parse_mode\":\"MarkdownV2\","
                  "\"reply_markup\":{\"inline_keyboard\":" + keyboard + "}}";

    int httpCode = http.POST(body);
    http.end();

    if (httpCode != 200) {
        Serial.printf("[TG] sendMessageWithKeyboard HTTP %d\n", httpCode);
        return false;
    }
    return true;
}

void TelegramBotController::sendMessageToAll(String text) {
    for (int i = 0; i < MAX_TELEGRAM_USERS; i++) {
        if (users[i].isActive) {
            sendMessage(String(users[i].chatId), text);
        }
    }
}

// =============================================================================
// PUBLIC — User management
// =============================================================================

bool TelegramBotController::addUser(String chatId, String name) {
    // Check if already present — update name
    for (int i = 0; i < MAX_TELEGRAM_USERS; i++) {
        if (users[i].isActive &&
            String(users[i].chatId) == chatId) {
            name.toCharArray(users[i].name, sizeof(users[i].name));
            saveConfig();
            return true;
        }
    }
    // Find free slot
    for (int i = 0; i < MAX_TELEGRAM_USERS; i++) {
        if (!users[i].isActive) {
            chatId.toCharArray(users[i].chatId, sizeof(users[i].chatId));
            name.toCharArray(users[i].name,   sizeof(users[i].name));
            users[i].isActive = true;
            saveConfig();
            return true;
        }
    }
    Serial.println("[TG] addUser: lista utenti piena");
    return false;
}

bool TelegramBotController::removeUser(String chatId) {
    for (int i = 0; i < MAX_TELEGRAM_USERS; i++) {
        if (users[i].isActive &&
            String(users[i].chatId) == chatId) {
            users[i].isActive = false;
            saveConfig();
            return true;
        }
    }
    return false;
}

int TelegramBotController::getUserCount() const {
    int count = 0;
    for (int i = 0; i < MAX_TELEGRAM_USERS; i++) {
        if (users[i].isActive) count++;
    }
    return count;
}

const TelegramUser* TelegramBotController::getUser(int index) const {
    if (index < 0 || index >= MAX_TELEGRAM_USERS) return nullptr;
    return &users[index];
}

// =============================================================================
// PUBLIC — Configuration
// =============================================================================

void TelegramBotController::setToken(const char* newToken) {
    strncpy(token, newToken, sizeof(token) - 1);
    token[sizeof(token) - 1] = '\0';
}

String TelegramBotController::getTokenMasked() const {
    int len = strlen(token);
    if (len < 10) return "****";
    return String(token).substring(0, 6) + "****" +
           String(token).substring(len - 4);
}

bool TelegramBotController::isEnabled() const {
    return enabled;
}

void TelegramBotController::setEnabled(bool enable) {
    enabled = enable;
    saveConfig();
}

bool TelegramBotController::getHourlyLogEnabled() const {
    return hourlyLogEnabled;
}

void TelegramBotController::setHourlyLogEnabled(bool enable) {
    hourlyLogEnabled = enable;
    saveConfig();
}

bool TelegramBotController::loadConfig() {
    if (!SPIFFS.exists("/telegram.json")) {
        Serial.println("[TG] /telegram.json non trovato — configurazione vuota.");
        memset(token, 0, sizeof(token));
        enabled          = false;
        hourlyLogEnabled = true;
        memset(users, 0, sizeof(users));
        return false;
    }

    File f = SPIFFS.open("/telegram.json", "r");
    if (!f) {
        Serial.println("[TG] Impossibile aprire /telegram.json");
        return false;
    }

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[TG] loadConfig JSON error: %s\n", err.c_str());
        return false;
    }

    const char* tok = doc["token"] | "";
    strncpy(token, tok, sizeof(token) - 1);
    token[sizeof(token) - 1] = '\0';

    enabled          = doc["enabled"]    | false;
    hourlyLogEnabled = doc["hourlyLog"]  | true;

    // Clear users array
    memset(users, 0, sizeof(users));

    JsonArray arr = doc["users"].as<JsonArray>();
    int idx = 0;
    for (JsonObject u : arr) {
        if (idx >= MAX_TELEGRAM_USERS) break;
        const char* cid  = u["chatId"] | "";
        const char* cnam = u["name"]   | "";
        strncpy(users[idx].chatId, cid,  sizeof(users[idx].chatId) - 1);
        strncpy(users[idx].name,   cnam, sizeof(users[idx].name)   - 1);
        users[idx].isActive = true;
        idx++;
    }

    Serial.printf("[TG] Configurazione caricata. Utenti: %d\n", idx);
    return true;
}

bool TelegramBotController::saveConfig() {
    DynamicJsonDocument doc(2048);
    doc["token"]     = token;
    doc["enabled"]   = enabled;
    doc["hourlyLog"] = hourlyLogEnabled;

    JsonArray arr = doc.createNestedArray("users");
    for (int i = 0; i < MAX_TELEGRAM_USERS; i++) {
        if (users[i].isActive) {
            JsonObject u = arr.createNestedObject();
            u["chatId"] = users[i].chatId;
            u["name"]   = users[i].name;
        }
    }

    File f = SPIFFS.open("/telegram.json", "w");
    if (!f) {
        Serial.println("[TG] saveConfig: impossibile aprire /telegram.json in scrittura");
        return false;
    }
    serializeJson(doc, f);
    f.close();

    Serial.println("[TG] Configurazione salvata.");
    return true;
}

// =============================================================================
// PRIVATE — Helpers
// =============================================================================

bool TelegramBotController::isAuthorized(String chatId) const {
    for (int i = 0; i < MAX_TELEGRAM_USERS; i++) {
        if (users[i].isActive &&
            String(users[i].chatId) == chatId) {
            return true;
        }
    }
    return false;
}

String TelegramBotController::escapeJson(const String& s) const {
    String out;
    out.reserve(s.length() + 16);
    for (unsigned int i = 0; i < s.length(); i++) {
        char c = s.charAt(i);
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}
