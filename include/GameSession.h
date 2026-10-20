/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: include/GameSession.h
 * 
 * [OOP PILLARS APPLIED: ABSTRACTION & POLYMORPHISM]
 * 1. Abstraction: GameSession encapsulates the entire state machine of an active
 *    multiplayer match (room creation, player pairing, turn alternation, move
 *    legality verification, and victory detection). Consumers do not need to
 *    manually check rows, columns, or synchronization flags.
 * 
 * 2. Polymorphism: GameSession holds references to the abstract Player base class
 *    (std::shared_ptr<Player>). It has zero knowledge of whether Player 1 or
 *    Player 2 is a LocalPlayer, WebSocketPlayer, or AIPlayer—it uniformly
 *    interacts with them through the Player interface.
 * ============================================================================
 */

#ifndef GAME_SESSION_H
#define GAME_SESSION_H

#include "Board.h"
#include "Player.h"
#include <memory>
#include <string>
#include <mutex>

/**
 * @enum SessionState
 * @brief Represents the lifecycle phase of a multiplayer match room.
 */
enum class SessionState {
    WAITING_FOR_PLAYER_2, ///< Room created by host; awaiting opponent join.
    ACTIVE,               ///< Both players present; match in progress.
    FINISHED,             ///< Game has concluded in win or draw.
    ABANDONED             ///< A player disconnected prematurely.
};

/**
 * @class GameSession
 * @brief Thread-safe orchestrator managing game flow between two Player instances.
 */
class GameSession {
private:
    std::string sessionId;
    Board board;

    // [POLYMORPHISM]: Pointers to abstract base class Player.
    std::shared_ptr<Player> playerX;
    std::shared_ptr<Player> playerO;

    char currentTurn; // 'X' or 'O'
    SessionState state;
    std::string winnerName;

    mutable std::mutex sessionMutex;

public:
    /**
     * @brief Constructs a new game session with a unique room identifier.
     * @param id Unique room / session code.
     * @param hostPlayer Shared pointer to the host (Player X).
     */
    GameSession(const std::string& id, std::shared_ptr<Player> hostPlayer);

    /**
     * @brief Destructor.
     */
    virtual ~GameSession() = default;

    /**
     * @brief Adds the second player (Player O) to transition session to ACTIVE.
     * @param guestPlayer Shared pointer to joining opponent.
     * @return true if joined successfully, false if session already full.
     */
    bool joinPlayer2(std::shared_ptr<Player> guestPlayer);

    /**
     * @brief Configures the session for Single-Player Bot Mode.
     * Immediately transitions state to ACTIVE with Player O as an AIPlayer.
     * @param bot Shared pointer to the polymorphic AIPlayer instance.
     */
    void enableBotPlayer(std::shared_ptr<Player> bot);

    /**
     * @brief Checks if Player O is an automated AI instance.
     * @return true if playing against bot, false otherwise.
     */
    bool isBotGame() const;

    /**
     * @brief Executes an AI bot turn automatically via polymorphism.
     * Calls playerO->getMove() polymorphically, which computes optimal minimax.
     * @return true if bot successfully executed a move, false otherwise.
     */
    bool triggerBotMove();

    /**
     * @brief Processes a move submitted by an authenticated player.
     * 
     * Verifies:
     *   1. Session is currently ACTIVE.
     *   2. It is strictly the requesting player's turn.
     *   3. Coordinates are within [0..2] and cell is unoccupied.
     * 
     * If valid, mutates the Board, checks for win/draw, and switches turn.
     * 
     * @param playerName Name of the player attempting the move.
     * @param row Target grid row.
     * @param col Target grid column.
     * @param outErrorMessage Description of violation if move fails.
     * @return true if move was applied, false otherwise.
     */
    bool submitMove(const std::string& playerName, int row, int col, std::string& outErrorMessage);

    /**
     * @brief Resets the board for a rematch between the same two players.
     */
    void restartGame();

    /**
     * @brief Marks the session abandoned when a participant disconnects.
     * @param disconnectedPlayer Name of player who left.
     */
    void handlePlayerDisconnect(const std::string& disconnectedPlayer);

    // Read-only state accessors
    const std::string& getSessionId() const;
    SessionState getSessionState() const;
    char getCurrentTurn() const;
    std::string getWinnerName() const;
    const Board& getBoard() const;

    std::shared_ptr<Player> getPlayerX() const;
    std::shared_ptr<Player> getPlayerO() const;

    /**
     * @brief Serializes the session into a structured JSON string.
     * Suitable for broadcasting over WebSockets to both connected clients.
     * @return JSON string representing complete match state.
     */
    std::string toJson() const;
};

#endif // GAME_SESSION_H
