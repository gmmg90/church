#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

// Forward declarations
class BellController;
struct SystemStatus;

/**
 * Bot Telegram eseguito in un task FreeRTOS dedicato (core 0).
 * Le richieste HTTPS (1-3 s ciascuna) non bloccano più il loop() né le melodie.
 * Tutti gli invii passano da una coda: il client TLS è usato da un solo task.
 */
class TelegramBotManager {
public:
    TelegramBotManager();

    // Inizializzazione del bot (avvia il task, non fa richieste di rete)
    bool begin(const char* token);

    // Mantenuto per compatibilità: il lavoro è svolto dal task del bot
    void update() {}

    // Invia notifica a tutti gli utenti autorizzati (accodata, non bloccante)
    void sendNotification(const String& message);

    // Invia messaggio a utente specifico (accodato, non bloccante)
    void sendMessage(const String& chatId, const String& message);

    // Attende che i messaggi accodati siano stati inviati (prima di un riavvio)
    void waitForOutbox(uint32_t timeoutMs);

    // Abilita/disabilita bot
    void setEnabled(bool enabled);
    bool isEnabled() const { return _enabled; }

    // Aggiungi/rimuovi utente autorizzato
    bool addAuthorizedUser(const String& chatId);
    bool removeAuthorizedUser(const String& chatId);
    bool isAuthorized(const String& chatId) const;

    // Ottieni lista utenti autorizzati
    String getAuthorizedUsers() const;

    // Statistiche
    uint32_t getMessageCount() const { return _messageCount; }
    uint32_t getCommandCount() const { return _commandCount; }
    unsigned long getLastUpdateTime() const { return _lastUpdateTime; }

    // Riferimenti ai componenti del sistema
    void setBellController(BellController* controller) { _bellController = controller; }
    void setSystemStatus(SystemStatus* status) { _systemStatus = status; }

private:
    WiFiClientSecure _client;
    long _nextUpdateId;   // offset per getUpdates (ultimo update_id + 1)

    volatile bool _enabled;
    bool _initialized;

    unsigned long _lastUpdateTime;
    static constexpr unsigned long BOT_CHECK_INTERVAL = 3000;      // 3 secondi tra un controllo e l'altro
    static constexpr unsigned long STARTUP_NOTIFY_DELAY = 20000;   // notifica avvio dopo 20 s di funzionamento stabile

    // Coda messaggi in uscita
    struct OutMessage {
        String chatId;   // vuoto = a tutti gli utenti autorizzati
        String text;
    };
    String _token;
    QueueHandle_t _outQueue;
    volatile bool _sending;
    TaskHandle_t _task;
    static void taskEntry(void* arg);
    void taskLoop();
    void enqueue(const String& chatId, const String& text);
    void flushOutQueue();
    void reply(const String& chatId, const String& text);

    // HTTP verso api.telegram.org: legge SEMPRE la risposta completa (Content-Length)
    // e cede la CPU mentre aspetta (niente watchdog). Ritorna il corpo, "" se nulla ricevuto.
    String apiRequest(const String& method, const String& jsonBody, bool* gotReply = nullptr);
    bool readLine(String& line, uint32_t deadline);
    int pollUpdates(bool skipOld);

    // Whitelist utenti autorizzati (max 10)
    String _authorizedUsers[10];
    uint8_t _authorizedCount;

    // Statistiche
    uint32_t _messageCount;
    uint32_t _commandCount;

    // Riferimenti componenti sistema
    BellController* _bellController;
    SystemStatus* _systemStatus;

    // Gestione messaggi
    void handleCommand(const String& chatId, const String& text, const String& userName);

    // Comandi disponibili
    void cmdStart(const String& chatId, const String& userName);
    void cmdHelp(const String& chatId);
    void cmdStatus(const String& chatId);
    void cmdEnable(const String& chatId);
    void cmdDisable(const String& chatId);
    void cmdRing(const String& chatId, const String& args);
    void cmdStop(const String& chatId);
    void cmdFuneral(const String& chatId);
    void cmdMass(const String& chatId);
    void cmdMelody(const String& chatId, const String& args);
    void cmdInfo(const String& chatId);
    void cmdAuth(const String& chatId, const String& userName);
    void cmdNTP(const String& chatId);
    void cmdTime(const String& chatId);

    // Utility
    String formatUptime() const;
    String buildStartupMessage() const;
};

#endif // TELEGRAM_BOT_H
