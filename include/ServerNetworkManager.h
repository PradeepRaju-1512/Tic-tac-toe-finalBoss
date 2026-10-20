/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: include/ServerNetworkManager.h
 * 
 * [OOP PILLAR APPLIED: ABSTRACTION]
 * ServerNetworkManager encapsulates the entire networking sub-system:
 *  - TCP port binding and HTTP/WebSocket server initialization.
 *  - Crow route registration (/api/register, /api/login, /ws).
 *  - Connection lifecycle tracking (on_open, on_message, on_close, on_error).
 *  - Serialization and deserialization of JSON payloads.
 * 
 * The main() entry point simply instantiates ServerNetworkManager, configures
 * the port, and calls run(). It does not need to handle raw socket file
 * descriptors, HTTP headers, or WebSocket protocol framing opcodes.
 * ============================================================================
 */

#ifndef SERVER_NETWORK_MANAGER_H
#define SERVER_NETWORK_MANAGER_H

#include "AuthManager.h"
#include "GameSession.h"
#include "WebSocketPlayer.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <vector>

// Forward declaration of Crow app to keep header dependencies lightweight.
namespace crow {
    class SimpleApp;
    namespace websocket {
        struct connection;
    }
}

/**
 * @class ServerNetworkManager
 * @brief Manages Crow HTTP routes, WebSocket connections, and room matchmaking.
 */
class ServerNetworkManager {
private:
    int port;
    std::string usersFilePath;

    // Core business logic managers
    AuthManager authManager;

    // Active multiplayer game sessions indexed by sessionId
    std::unordered_map<std::string, std::shared_ptr<GameSession>> activeSessions;

    // Connection tracking: connection pointer -> authenticated username
    std::unordered_map<void*, std::string> connectionToUser;

    // User tracking: authenticated username -> connection pointer
    std::unordered_map<std::string, void*> userToConnection;

    // User tracking: username -> active sessionId
    std::unordered_map<std::string, std::string> userToSession;

    // Session tokens: token -> username
    std::unordered_map<std::string, std::string> sessionTokens;

    // Rematch tracking: sessionId -> set of consenting usernames
    std::unordered_map<std::string, std::vector<std::string>> rematchRequests;

    mutable std::mutex networkMutex;

    // Pointer to the Crow application instance (if compiled with Crow)
    std::unique_ptr<crow::SimpleApp> app;

    // Native POSIX Server state for zero-dependency Linux deployment
    std::atomic<bool> isRunning{false};
    int serverFd{-1};

    /**
     * @brief Generates and stores a cryptographically secure random session token.
     */
    std::string issueSessionToken(const std::string& username);

    /**
     * @brief Validates a session token, returning the username or empty string.
     */
    std::string validateSessionToken(const std::string& token);

    /**
     * @brief Native POSIX worker for HTTP/WebSocket clients.
     */
    void handleClientConnection(int clientFd);

    /**
     * @brief Registers REST API endpoints for user registration and login.
     */
    void setupHttpRoutes();

    /**
     * @brief Registers WebSocket endpoint (/ws) and binds event callbacks.
     */
    void setupWebSocketRoutes();

    /**
     * @brief Broadcasts a raw text message to a specific connection.
     * @param connPtr Raw connection pointer (or file descriptor cast).
     * @param message Text payload (JSON).
     */
    void sendToConnection(void* connPtr, const std::string& message);

public:
    /**
     * @brief Constructs the ServerNetworkManager.
     * @param listenPort Port to bind the server (e.g. 8080).
     * @param userDbFile Path to users.txt file.
     */
    explicit ServerNetworkManager(int listenPort = 8080, const std::string& userDbFile = "users.txt");

    /**
     * @brief Destructor. Closes active sockets and shuts down server gracefully.
     */
    ~ServerNetworkManager();

    // Prevent copying
    ServerNetworkManager(const ServerNetworkManager&) = delete;
    ServerNetworkManager& operator=(const ServerNetworkManager&) = delete;

    /**
     * @brief Initializes network routes and begins listening for connections.
     * Starts the multithreaded Asio event loop.
     */
    void run();

    /**
     * @brief Signals the server to stop accepting traffic and terminate cleanly.
     */
    void stop();

    // Internal WebSocket event handlers dispatched by Crow callbacks:
    void handleWsOpen(crow::websocket::connection& conn);
    void handleWsClose(crow::websocket::connection& conn, const std::string& reason);
    void handleWsMessage(crow::websocket::connection& conn, const std::string& data, bool is_binary);

    // Business action dispatchers:
    void handleCreateRoom(void* connPtr, const std::string& username);
    void handleCreateBotRoom(void* connPtr, const std::string& username);
    void handleJoinRoom(void* connPtr, const std::string& username, const std::string& roomId);
    void handleMove(void* connPtr, const std::string& username, int row, int col);
    void handleRematch(void* connPtr, const std::string& username);
    void broadcastSessionUpdate(const std::string& sessionId);

    /**
     * @brief Accessor to the internal AuthManager (for verification tests).
     * @return Reference to AuthManager.
     */
    AuthManager& getAuthManager();
};

#endif // SERVER_NETWORK_MANAGER_H
