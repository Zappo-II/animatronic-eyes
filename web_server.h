/*
 * Animatronic Eyes
 * Copyright (c) 2025 Zappo-II
 * Licensed under CC BY-NC-SA 4.0
 * https://github.com/Zappo-II/animatronic-eyes
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <IPAddress.h>

// Forward declarations
class AsyncWebServerRequest;
class AsyncWebSocketClient;

// Log buffer configuration
#define LOG_BUFFER_SIZE 50
#define LOG_LINE_MAX_LEN 128

// Admin auth tracking
#define MAX_AUTH_CLIENTS 8
#define MAX_RATE_LIMIT_ENTRIES 10

struct ClientAuthState {
    IPAddress ip;
    bool authenticated;
    unsigned long unlockTime;  // millis() when unlock expires (0 = no timeout, e.g., AP client)
    bool isAPClient;
};

struct RateLimitEntry {
    IPAddress ip;
    uint8_t failedAttempts;
    unsigned long lockoutUntil;  // millis() when lockout expires
};

class WebServer {
public:
    void begin();
    void loop();
    void requestBroadcast();  // Request broadcast from main loop (safe from async context)

    // UI file management
    bool checkUIFiles();           // Check if required UI files exist
    void loadUIVersionInfo();      // Load version info from version.json
    String getUIStatus();          // Get detailed UI compatibility status

    // Web console logging
    void log(const char* tag, const char* format, ...);  // Log with tag and format
    void sendLogHistory(AsyncWebSocketClient* client);   // Send buffered logs to new client

private:
    unsigned long _lastBroadcast = 0;
    volatile bool _broadcastRequested = false;  // Flag for deferred broadcast
    bool _uiFilesValid = false;
    String _uiVersion = "";
    String _uiMinFirmware = "";

    // Log ring buffer
    String _logBuffer[LOG_BUFFER_SIZE];
    int _logHead = 0;
    int _logCount = 0;

    void setupRoutes();
    void setupWebSocket();
    void broadcastState();
    void broadcastLog(const String& logLine);
    void handleWebSocketMessage(const char* data, AsyncWebSocketClient* client);
    void sendConfigToClient(AsyncWebSocketClient* client);
    void sendAvailableLists(AsyncWebSocketClient* client);  // Send available modes/impulses on connect
    void sendAdminState(AsyncWebSocketClient* client);      // Send per-client admin lock state
    void broadcastAdminStateToIP(IPAddress ip);             // Send admin state to all clients from same IP
    void sendAdminBlocked(AsyncWebSocketClient* client, const char* command);  // Notify client command was blocked
    void serveRecoveryPage(AsyncWebServerRequest* request);

    // Admin auth helpers
    bool isAPClient(AsyncWebSocketClient* client);
    bool isAPClientIP(IPAddress ip);  // Check IP directly (for HTTP requests)
    bool isHttpRequestAuthorized(AsyncWebServerRequest* request);  // For HTTP endpoints
    bool isClientAuthenticated(AsyncWebSocketClient* client);
    bool isClientLocked(AsyncWebSocketClient* client);
    int getRemainingSeconds(AsyncWebSocketClient* client);
    void authenticateClient(AsyncWebSocketClient* client);
    void authenticateIP(IPAddress ip);  // For HTTP-based unlock (recovery UI)
    void lockClient(AsyncWebSocketClient* client);
    bool checkRateLimit(IPAddress ip);
    int getRateLimitSeconds(IPAddress ip);  // Returns seconds remaining in lockout
    void recordFailedAttempt(IPAddress ip);
    void clearFailedAttempts(IPAddress ip);

    // Auth state tracking
    ClientAuthState _authClients[MAX_AUTH_CLIENTS];
    int _authClientCount = 0;
    RateLimitEntry _rateLimits[MAX_RATE_LIMIT_ENTRIES];
    int _rateLimitCount = 0;
};

extern WebServer webServer;

// Convenience macro for logging
#define WEB_LOG(tag, fmt, ...) webServer.log(tag, fmt, ##__VA_ARGS__)

#endif // WEB_SERVER_H
