/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: include/WebSocketPlayer.h
 * 
 * [OOP PILLAR APPLIED: INHERITANCE & POLYMORPHISM]
 * WebSocketPlayer inherits directly from Player. Instead of reading from a local
 * keyboard stream, it reads coordinates transmitted across the network over
 * an active TCP WebSocket channel.
 * 
 * [SYSTEMS CONCURRENCY & SYNCHRONIZATION]:
 * Because WebSockets deliver messages asynchronously in worker threads,
 * WebSocketPlayer uses a thread-safe synchronized move queue protected by
 * std::mutex and std::condition_variable. When getMove() is called by the
 * game loop, it safely blocks until the network dispatcher enqueues a move,
 * or times out cleanly if the remote client disconnects.
 * ============================================================================
 */

#ifndef WEBSOCKET_PLAYER_H
#define WEBSOCKET_PLAYER_H

#include "Player.h"
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

/**
 * @class WebSocketPlayer
 * @brief Concrete Player implementation that receives moves over a network WebSocket.
 */
class WebSocketPlayer : public Player {
private:
    // Unique identifier for the client's network connection or socket session.
    std::string connectionId;

    // Synchronized queue to hold incoming move coordinates sent from the client.
    std::queue<std::pair<int, int>> pendingMoves;
    mutable std::mutex queueMutex;
    std::condition_variable cvMoveAvailable;

    // Atomic flag signaling whether the network peer is currently connected.
    std::atomic<bool> connected;

public:
    /**
     * @brief Constructs a WebSocketPlayer.
     * @param playerName Authenticated username of the connected player.
     * @param playerSymbol Assigned marker ('X' or 'O').
     * @param connId Connection handle / socket identifier.
     */
    WebSocketPlayer(const std::string& playerName, char playerSymbol, const std::string& connId);

    /**
     * @brief Virtual destructor. Cleans up pending synchronization barriers.
     */
    ~WebSocketPlayer() override;

    /**
     * @brief Polymorphic implementation of getMove().
     * 
     * Blocks on the condition variable until pushIncomingMove() is called by
     * the network dispatcher, or returns (-1, -1) if the socket drops.
     * 
     * @return std::pair<int, int> Received (row, column) coordinates.
     */
    std::pair<int, int> getMove() override;

    /**
     * @brief Called by the network dispatcher when a move payload is received.
     * Enqueues the coordinates and notifies waiting threads.
     * 
     * @param row Target row (0..2).
     * @param col Target column (0..2).
     */
    void pushIncomingMove(int row, int col);

    /**
     * @brief Accessor for the socket / connection ID.
     * @return Const reference to connection ID.
     */
    const std::string& getConnectionId() const;

    /**
     * @brief Sets or updates the connection ID (e.g. upon client reconnect).
     * @param connId New connection ID.
     */
    void setConnectionId(const std::string& connId);

    /**
     * @brief Notifies the player that the remote network socket has closed.
     * Unblocks any waiting getMove() call to prevent deadlocks.
     */
    void notifyDisconnection();

    /**
     * @brief Checks if the player is currently connected.
     * @return true if connected, false otherwise.
     */
    bool isConnected() const;
};

#endif // WEBSOCKET_PLAYER_H
