/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: include/Player.h
 * 
 * [OOP PILLAR APPLIED: INHERITANCE & POLYMORPHISM]
 * Player is an Abstract Base Class (ABC). It defines the universal contract
 * for any participant in a Tic-Tac-Toe session, regardless of whether that
 * player is physically seated at the host console, connected remotely via
 * a Linux POSIX TCP socket, or streaming moves through a WebSocket connection.
 * 
 * Key Elements:
 *  1. Pure Virtual Function:
 *     virtual std::pair<int, int> getMove() = 0;
 *     Forces all derived classes to implement their own move acquisition
 *     strategy, establishing run-time polymorphism.
 * 
 *  2. Virtual Destructor:
 *     virtual ~Player() = default;
 *     Mandatory in C++ when deleting derived objects through a base class
 *     pointer (Player*). Omitting this causes undefined behavior and memory leaks.
 * ============================================================================
 */

#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include <utility>

/**
 * @class Player
 * @brief Abstract base class defining interface and state for all player types.
 */
class Player {
protected:
    // [ENCAPSULATION]: Protected member attributes accessible by derived classes.
    std::string name;
    char symbol; // 'X' or 'O'

public:
    /**
     * @brief Constructs a Player with an authenticated username and assigned symbol.
     * @param playerName The authenticated name of the player.
     * @param playerSymbol The assigned marker ('X' or 'O').
     */
    Player(const std::string& playerName, char playerSymbol);

    /**
     * @brief Virtual destructor ensuring clean polymorphic destruction.
     * Crucial when a GameSession deletes a Player* pointer.
     */
    virtual ~Player() = default;

    /**
     * @brief Pure virtual function to obtain the player's chosen move coordinates.
     * 
     * [POLYMORPHISM]: The GameSession orchestrator calls getMove() through a
     * Player* pointer. At runtime, the C++ vtable dispatches the call to:
     *   - LocalPlayer::getMove()     -> Reads from terminal stdin
     *   - WebSocketPlayer::getMove() -> Reads from incoming network buffer
     * 
     * @return std::pair<int, int> A pair representing (row, column) in [0..2].
     */
    virtual std::pair<int, int> getMove() = 0;

    /**
     * @brief Accessor for the player's username.
     * @return Const reference to username string.
     */
    const std::string& getName() const;

    /**
     * @brief Accessor for the player's marker symbol.
     * @return Symbol character ('X' or 'O').
     */
    char getSymbol() const;

    /**
     * @brief Mutator to assign or swap player symbol.
     * @param sym New symbol character.
     */
    void setSymbol(char sym);
};

#endif // PLAYER_H
