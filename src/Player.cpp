/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: src/Player.cpp
 * 
 * [OOP PILLARS DEMONSTRATED: INHERITANCE & POLYMORPHISM]
 * 1. ABSTRACT BASE CLASS ROLE:
 *    - Player cannot be directly instantiated because it declares the pure 
 *      virtual function `virtual std::pair<int, int> getMove() = 0;`.
 *    - It encapsulates common state shared by all participants: their 
 *      authenticated username and their assigned game symbol ('X' or 'O').
 * 
 * 2. VIRTUAL DESTRUCTOR GUARANTEE:
 *    - Even though `~Player() = default;` is declared in the header, having 
 *      it marked virtual ensures that the C++ dynamic dispatch table (vtable) 
 *      properly resolves the derived class destructor when deleting objects 
 *      via a `Player*` pointer.
 *    - Example: If a `Player* p = new WebSocketPlayer(...)` is deleted, the 
 *      vtable ensures `~WebSocketPlayer()` runs first, freeing network mutexes 
 *      and socket queues, followed by `~Player()`. Without `virtual`, only 
 *      `~Player()` would be invoked, causing resource and memory leaks.
 * ============================================================================
 */

#include "Player.h"

/**
 * Base Constructor: Initializes common member variables.
 * 
 * @param playerName Authenticated username.
 * @param playerSymbol Marker character ('X' or 'O').
 */
Player::Player(const std::string& playerName, char playerSymbol)
    : name(playerName), symbol(playerSymbol) {
}

/**
 * Accessor: Returns const reference to player's name (no unnecessary string copies).
 */
const std::string& Player::getName() const {
    return name;
}

/**
 * Accessor: Returns the marker symbol ('X' or 'O').
 */
char Player::getSymbol() const {
    return symbol;
}

/**
 * Mutator: Updates or swaps the player's marker symbol.
 */
void Player::setSymbol(char sym) {
    symbol = sym;
}
