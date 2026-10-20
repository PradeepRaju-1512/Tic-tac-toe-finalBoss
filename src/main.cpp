/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: src/main.cpp
 * 
 * [PROJECT ARCHITECTURE SUMMARY]:
 * 1. ENCAPSULATION:
 *    - Board encapsulates the 3x3 grid array and validates all moves.
 *    - AuthManager encapsulates users.txt file I/O and deterministic DJB2 hashing.
 * 
 * 2. ABSTRACTION:
 *    - main() contains ZERO socket code, ZERO HTTP headers, and ZERO grid logic.
 *    - It relies purely on the high-level abstraction `ServerNetworkManager`.
 * 
 * 3. INHERITANCE & POLYMORPHISM:
 *    - Abstract class `Player` defines `virtual std::pair<int, int> getMove() = 0;`.
 *    - `WebSocketPlayer` and `LocalPlayer` polymorphically override move retrieval.
 *    - `GameSession` orchestrates matches holding `std::shared_ptr<Player>` pointers.
 * 
 * [SYSTEMS RESILIENCE & SIGNAL HANDLING]:
 *    - Catches SIGINT (Ctrl+C) and SIGTERM using Linux POSIX signal handlers to 
 *      allow graceful termination and clean socket closure.
 * ============================================================================
 */

#include "ServerNetworkManager.h"
#include <iostream>
#include <csignal>
#include <cstdlib>

// Global pointer to ServerNetworkManager for POSIX signal handler invocation
static ServerNetworkManager* g_serverInstance = nullptr;

/**
 * POSIX Signal Handler for graceful termination (SIGINT / SIGTERM).
 */
void handleSignal(int signalNumber) {
    std::cout << "\n[SIGNAL INTERRUPT] Caught signal " << signalNumber 
              << ". Initiating graceful server shutdown..." << std::endl;
    if (g_serverInstance != nullptr) {
        g_serverInstance->stop();
    }
    std::cout << "[SHUTDOWN COMPLETE] Socket ports released. Exiting cleanly.\n";
    std::exit(0);
}

int main(int argc, char* argv[]) {
    std::cout << "========================================================\n"
              << "   CS204 B.Tech CSE: Online Multiplayer Tic-Tac-Toe     \n"
              << "   Backend Engine: C++17 Crow Web & WebSocket Server    \n"
              << "========================================================\n"
              << std::endl;

    // 1. Parse port from CLI arguments or use default 8080
    int port = 8080;
    if (argc >= 2) {
        try {
            port = std::stoi(argv[1]);
            if (port < 1024 || port > 65535) {
                std::cerr << "[WARNING] Port " << port 
                          << " is outside typical unprivileged user space (1024-65535). Using default 8080.\n";
                port = 8080;
            }
        } catch (const std::exception& e) {
            std::cerr << "[ERROR] Invalid port argument provided: " << argv[1] 
                      << ". Defaulting to 8080.\n";
            port = 8080;
        }
    }

    // 2. Register POSIX signal handlers
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);

    std::cout << "[SYSTEM BOOT] Server binding to 0.0.0.0:" << port << std::endl;
    std::cout << "[PERSISTENCE] Authentication database initialized at 'users.txt' (Salted DJB2)" << std::endl;

    try {
        // 3. [ABSTRACTION]: Instantiate the network manager
        ServerNetworkManager server(port, "users.txt");
        g_serverInstance = &server;

        // 4. Run the server event loop
        server.run();

    } catch (const std::exception& e) {
        std::cerr << "[FATAL EXCEPTION] Server encountered an unrecoverable error: " 
                  << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "[FATAL ERROR] An unknown exception occurred during runtime." << std::endl;
        return 2;
    }

    return 0;
}
