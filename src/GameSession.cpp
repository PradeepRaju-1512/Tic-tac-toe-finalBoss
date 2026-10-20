/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: src/GameSession.cpp
 * 
 * [OOP PILLARS DEMONSTRATED: ABSTRACTION & POLYMORPHISM]
 * 1. ABSTRACTION OF MATCH LIFECYCLE:
 *    - GameSession encapsulates the full state machine of a multiplayer room:
 *      (WAITING_FOR_PLAYER_2 -> ACTIVE -> FINISHED / ABANDONED).
 *    - External network handlers do not need to know the rules of Tic-Tac-Toe;
 *      they simply invoke `submitMove(playerName, row, col, outError)`.
 * 
 * 2. RUN-TIME POLYMORPHISM VIA BASE CLASS POINTERS:
 *    - GameSession manages `playerX` and `playerO` as `std::shared_ptr<Player>`.
 *    - The session logic does not know or care if the players are LocalPlayer
 *      or WebSocketPlayer instances. It treats them uniformly via the Player
 *      interface.
 * 
 * 3. STRICT TURN INTEGRITY & STATE PROTECTION:
 *    - Prevents out-of-turn play, unauthorized spectator actions, and moves
 *      on concluded boards.
 *    - Generates standardized JSON payloads for bi-directional WebSocket frames.
 * ============================================================================
 */

#include "GameSession.h"
#include <sstream>
#include <iostream>

/**
 * Constructor: Initializes room with host player assigned as Player X.
 */
GameSession::GameSession(const std::string& id, std::shared_ptr<Player> hostPlayer)
    : sessionId(id), 
      playerX(hostPlayer), 
      playerO(nullptr), 
      currentTurn('X'), 
      state(SessionState::WAITING_FOR_PLAYER_2), 
      winnerName("") {
    if (playerX) {
        playerX->setSymbol('X');
    }
}

/**
 * Pairs the second player (Player O) and transitions room to ACTIVE.
 */
bool GameSession::joinPlayer2(std::shared_ptr<Player> guestPlayer) {
    std::lock_guard<std::mutex> lock(sessionMutex);

    if (state != SessionState::WAITING_FOR_PLAYER_2 || playerO != nullptr) {
        return false;
    }

    playerO = guestPlayer;
    playerO->setSymbol('O');
    state = SessionState::ACTIVE;
    currentTurn = 'X'; // Player X always leads in standard Tic-Tac-Toe rules
    winnerName.clear();

    std::cout << "[GameSession " << sessionId << "] Match started: " 
              << playerX->getName() << " (X) vs " << playerO->getName() << " (O)" 
              << std::endl;
    return true;
}

/**
 * [SINGLE PLAYER BOT MODE]: Configures AIPlayer as Player O.
 */
void GameSession::enableBotPlayer(std::shared_ptr<Player> bot) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    playerO = bot;
    playerO->setSymbol('O');
    state = SessionState::ACTIVE;
    currentTurn = 'X';
    winnerName.clear();
    std::cout << "[GameSession " << sessionId << "] Single Player Mode initialized vs " 
              << playerO->getName() << " (Minimax AI)" << std::endl;
}

/**
 * Checks whether this session includes an AI bot opponent.
 */
bool GameSession::isBotGame() const {
    std::lock_guard<std::mutex> lock(sessionMutex);
    return playerO != nullptr && playerO->getName().find("AI") != std::string::npos;
}

/**
 * [POLYMORPHISM IN ACTION]: Executes AI bot move.
 * Calls playerO->getMove() dynamically dispatched to AIPlayer::getMove().
 */
bool GameSession::triggerBotMove() {
    std::pair<int, int> move;
    std::string botName;
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        if (state != SessionState::ACTIVE || currentTurn != 'O' || !playerO) {
            return false;
        }
        botName = playerO->getName();
        // Dynamic dispatch: calls AIPlayer::getMove()
        move = playerO->getMove();
    }

    std::string error;
    return submitMove(botName, move.first, move.second, error);
}

/**
 * [CORE GAME LOGIC & INVARIANT ENFORCEMENT]: submitMove()
 * 
 * Validates player identity, turn order, and delegates board mutation.
 */
bool GameSession::submitMove(const std::string& playerName, 
                             int row, 
                             int col, 
                             std::string& outErrorMessage) {
    std::lock_guard<std::mutex> lock(sessionMutex);

    // 1. Verify session is currently in active play
    if (state != SessionState::ACTIVE) {
        if (state == SessionState::WAITING_FOR_PLAYER_2) {
            outErrorMessage = "Waiting for an opponent to join the room.";
        } else if (state == SessionState::FINISHED) {
            outErrorMessage = "The match has already finished. Request a rematch.";
        } else {
            outErrorMessage = "The match has been abandoned.";
        }
        return false;
    }

    // 2. Determine requesting player and verify turn permission
    char playerSymbol = '\0';
    if (playerX && playerX->getName() == playerName) {
        playerSymbol = 'X';
    } else if (playerO && playerO->getName() == playerName) {
        playerSymbol = 'O';
    } else {
        outErrorMessage = "You are not an active player in this room.";
        return false;
    }

    if (playerSymbol != currentTurn) {
        outErrorMessage = "It is not your turn. Please wait for your opponent.";
        return false;
    }

    // 3. Delegate move execution to Board (ENCAPSULATION)
    if (!board.makeMove(row, col, playerSymbol)) {
        outErrorMessage = "Illegal move: Cell is either occupied or out of bounds.";
        return false;
    }

    // 4. Evaluate win / draw state
    GameStatus status = board.getStatus();
    if (status == GameStatus::X_WON) {
        state = SessionState::FINISHED;
        winnerName = playerX->getName();
        std::cout << "[GameSession " << sessionId << "] Player X (" 
                  << winnerName << ") WON!" << std::endl;
    } else if (status == GameStatus::O_WON) {
        state = SessionState::FINISHED;
        winnerName = playerO->getName();
        std::cout << "[GameSession " << sessionId << "] Player O (" 
                  << winnerName << ") WON!" << std::endl;
    } else if (status == GameStatus::DRAW) {
        state = SessionState::FINISHED;
        winnerName = "DRAW";
        std::cout << "[GameSession " << sessionId << "] Match DRAW!" << std::endl;
    } else {
        // Switch turn between 'X' and 'O'
        currentTurn = (currentTurn == 'X') ? 'O' : 'X';
    }

    outErrorMessage.clear();
    return true;
}

/**
 * Resets board for rematch while preserving player pairing.
 */
void GameSession::restartGame() {
    std::lock_guard<std::mutex> lock(sessionMutex);
    board.reset();
    currentTurn = 'X';
    state = SessionState::ACTIVE;
    winnerName.clear();
    std::cout << "[GameSession " << sessionId << "] Rematch initiated." << std::endl;
}

/**
 * Handles early participant departures.
 */
void GameSession::handlePlayerDisconnect(const std::string& disconnectedPlayer) {
    std::lock_guard<std::mutex> lock(sessionMutex);
    if (state != SessionState::FINISHED) {
        state = SessionState::ABANDONED;
        if (playerX && playerX->getName() == disconnectedPlayer && playerO) {
            winnerName = playerO->getName() + " (by forfeit)";
        } else if (playerO && playerO->getName() == disconnectedPlayer && playerX) {
            winnerName = playerX->getName() + " (by forfeit)";
        } else {
            winnerName = "ABANDONED";
        }
        std::cout << "[GameSession " << sessionId << "] Player " 
                  << disconnectedPlayer << " disconnected. State set to ABANDONED." 
                  << std::endl;
    }
}

// Accessors
const std::string& GameSession::getSessionId() const {
    return sessionId;
}

SessionState GameSession::getSessionState() const {
    std::lock_guard<std::mutex> lock(sessionMutex);
    return state;
}

char GameSession::getCurrentTurn() const {
    std::lock_guard<std::mutex> lock(sessionMutex);
    return currentTurn;
}

std::string GameSession::getWinnerName() const {
    std::lock_guard<std::mutex> lock(sessionMutex);
    return winnerName;
}

const Board& GameSession::getBoard() const {
    return board;
}

std::shared_ptr<Player> GameSession::getPlayerX() const {
    std::lock_guard<std::mutex> lock(sessionMutex);
    return playerX;
}

std::shared_ptr<Player> GameSession::getPlayerO() const {
    std::lock_guard<std::mutex> lock(sessionMutex);
    return playerO;
}

/**
 * [NETWORK ABSTRACTION]: Serializes session state into standardized JSON.
 */
std::string GameSession::toJson() const {
    std::lock_guard<std::mutex> lock(sessionMutex);

    std::string stateStr;
    switch (state) {
        case SessionState::WAITING_FOR_PLAYER_2: stateStr = "WAITING"; break;
        case SessionState::ACTIVE:               stateStr = "ACTIVE"; break;
        case SessionState::FINISHED:             stateStr = "FINISHED"; break;
        case SessionState::ABANDONED:            stateStr = "ABANDONED"; break;
    }

    std::stringstream ss;
    ss << "{"
       << "\"sessionId\":\"" << sessionId << "\","
       << "\"state\":\"" << stateStr << "\","
       << "\"playerX\":\"" << (playerX ? playerX->getName() : "") << "\","
       << "\"playerO\":\"" << (playerO ? playerO->getName() : "") << "\","
       << "\"currentTurn\":\"" << currentTurn << "\","
       << "\"winner\":\"" << winnerName << "\","
       << "\"board\":\"" << board.serialize() << "\""
       << "}";
    return ss.str();
}
