#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
#define MAX_TELEGRAM_USERS       10
#define TELEGRAM_POLL_INTERVAL   3000UL       // ms between getUpdates polls
#define TELEGRAM_LOG_INTERVAL    3600000UL    // ms between hourly log broadcasts (1 h)

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------
struct TelegramUser {
    char chatId[20];
    char name[32];
    bool isActive;
};

// ---------------------------------------------------------------------------
// TelegramBotController
// ---------------------------------------------------------------------------
class TelegramBotController {
public:
    // Lifecycle
    bool   begin();
    void   update();

    // Messaging
    bool   sendMessage(String chatId, String text, String parseMode = "Markdown");
    bool   sendMessageWithKeyboard(String chatId, String text, String keyboard);
    void   sendMessageToAll(String text);
    void   sendHourlyLog();

    // User management
    bool   addUser(String chatId, String name);
    bool   removeUser(String chatId);
    int    getUserCount() const;
    const TelegramUser* getUser(int index) const;

    // Configuration
    void   setToken(const char* newToken);
    String getTokenMasked() const;
    bool   isEnabled() const;
    void   setEnabled(bool enable);
    bool   getHourlyLogEnabled() const;
    void   setHourlyLogEnabled(bool enable);
    bool   loadConfig();
    bool   saveConfig();

private:
    char          token[64];
    TelegramUser  users[MAX_TELEGRAM_USERS];
    bool          enabled;
    bool          hourlyLogEnabled;
    uint32_t      lastPollTime;
    uint32_t      lastLogTime;
    long          lastUpdateId;

    // Internal helpers
    bool   pollUpdates();
    void   handleTextMessage(String chatId, String fromName, String text);
    void   handleCallback(String chatId, String fromName, String data);
    void   sendStatus(String chatId);
    void   sendMelodyList(String chatId);
    void   sendHelpKeyboard(String chatId);
    bool   isAuthorized(String chatId) const;
    String escapeJson(const String& s) const;
};

// ---------------------------------------------------------------------------
// Global instance (defined in telegram_bot.cpp)
// ---------------------------------------------------------------------------
extern TelegramBotController telegramBot;
