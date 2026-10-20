/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: src/WebSocketPlayer.cpp
 * 
 * [OOP PILLARS DEMONSTRATED: INHERITANCE & POLYMORPHISM]
 * 1. INHERITANCE IN ACTION:
 *    - WebSocketPlayer is a concrete specialization of the Player base class.
 *    - Reuses the base attributes `name` and `symbol` initialized via the base
 *      constructor delegation: `Player(playerName, playerSymbol)`.
 * 
 * 2. RUN-TIME POLYMORPHISM (OVERRIDING PURE VIRTUAL METHOD):
 *    - Overrides `virtual std::pair<int, int> getMove() override;`.
 *    - When the orchestrating GameSession calls `player->getMove()`, dynamic
 *      dispatch routes execution here. Rather than reading from stdin, this
 *      method blocks safely on an internal synchronized queue until incoming
 *      network frames arrive from the client.
 * 
 * [SYSTEMS CONCURRENCY & IPC]:
 *    - Thread-safe Producer-Consumer pattern using `std::mutex` and 
 *      `std::condition_variable`.
 *    - Crow's asynchronous network thread acts as the Producer (pushIncomingMove),
 *      while the GameSession loop acts as the Consumer (getMove).
 *    - Disconnection handling ensures condition variables are notified so threads
 *      never deadlock if a remote client abruptly closes the socket.
 * ============================================================================
 */

#include "WebSocketPlayer.h"
#include <chrono>
#include <iostream>

/**
 * Constructor: Initializes base Player and sets up socket session identifier.
 */
WebSocketPlayer::WebSocketPlayer(const std::string& playerName, 
                                 char playerSymbol, 
                                 const std::string& connId)
    : Player(playerName, playerSymbol), 
      connectionId(connId), 
      connected(true) {
}

/**
 * Destructor: Signals disconnections to unblock any waiting consumer threads.
 */
WebSocketPlayer::~WebSocketPlayer() {
    notifyDisconnection();
}

/**
 * [POLYMORPHIC CORE]: getMove()
 * 
 * Blocks until a move is placed into the queue by pushIncomingMove(), or returns
 * sentinel coordinates (-1, -1) if the client disconnects or times out.
 * 
 * @return std::pair<int, int> The zero-indexed (row, col) coordinates.
 */
std::pair<int, int> WebSocketPlayer::getMove() {
    std::unique_lock<std::mutex> lock(queueMutex);

    // Wait until a move arrives or the socket drops (with 60-second turn timeout)
    bool moveReceived = cvMoveAvailable.wait_for(
        lock, 
        std::chrono::seconds(60), 
        [this]() {
            return !pendingMoves.empty() || !connected.load();
        }
    );

    // Check for disconnection or timeout
    if (!moveReceived || !connected.load()) {
        std::cerr << "[WebSocketPlayer TIMEOUT/DISCONNECT] Player " 
                  << name << " failed to deliver a move in time." << std::endl;
        return {-1, -1};
    }

    // Retrieve the front move from the synchronized queue
    if (!pendingMoves.empty()) {
        std::pair<int, int> move = pendingMoves.front();
        pendingMoves.pop();
        return move;
    }

    return {-1, -1};
}

/**
 * Producer function: Called by the Network Manager thread when an incoming
 * JSON payload {"type": "move", "row": r, "col": c} is decoded.
 */
void WebSocketPlayer::pushIncomingMove(int row, int col) {
    {
        std::lock_guard<std::mutex> lock(queueMutex);
        pendingMoves.push({row, col});
    }
    // Notify the consumer thread waiting in getMove()
    cvMoveAvailable.notify_one();
}

/**
 * Accessor for the WebSocket connection handle.
 */
const std::string& WebSocketPlayer::getConnectionId() const {
    return connectionId;
}

/**
 * Updates the connection ID upon socket reconnects.
 */
void WebSocketPlayer::setConnectionId(const std::string& connId) {
    std::lock_guard<std::mutex> lock(queueMutex);
    connectionId = connId;
    connected = true;
}

/**
 * Unblocks any waiting getMove() call when the network socket drops.
 */
void WebSocketPlayer::notifyDisconnection() {
    connected.store(false);
    cvMoveAvailable.notify_all();
}

/**
 * Non-mutating status check.
 */
bool WebSocketPlayer::isConnected() const {
    return connected.load();
}
