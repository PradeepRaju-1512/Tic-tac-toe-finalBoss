/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: include/LocalPlayer.h
 * 
 * [OOP PILLAR APPLIED: INHERITANCE]
 * LocalPlayer inherits directly from the abstract base class Player.
 * It provides the concrete implementation of getMove() by reading coordinates
 * from the local Linux console stream (std::cin).
 * 
 * Demonstrates:
 *  - Concrete subclassing
 *  - Overriding pure virtual functions (override specifier)
 *  - Robust standard input validation and clearing of error state bits
 * ============================================================================
 */

#ifndef LOCAL_PLAYER_H
#define LOCAL_PLAYER_H

#include "Player.h"
#include <iostream>

/**
 * @class LocalPlayer
 * @brief Concrete Player implementation that reads coordinates from local terminal stdin.
 */
class LocalPlayer : public Player {
private:
    std::istream& inStream;
    std::ostream& outStream;

public:
    /**
     * @brief Constructor for LocalPlayer.
     * @param playerName Authenticated username.
     * @param playerSymbol Assigned marker ('X' or 'O').
     * @param in Input stream (defaults to std::cin for CLI testing).
     * @param out Output stream (defaults to std::cout for prompts).
     */
    LocalPlayer(const std::string& playerName,
                char playerSymbol,
                std::istream& in = std::cin,
                std::ostream& out = std::cout);

    /**
     * @brief Virtual destructor.
     */
    ~LocalPlayer() override = default;

    /**
     * @brief Obtains move coordinates by prompting the local operator via stdin.
     * 
     * Handles non-integer input gracefully by clearing std::cin failbit and
     * discarding invalid characters from the stream buffer.
     * 
     * @return std::pair<int, int> Validated zero-indexed (row, column).
     */
    std::pair<int, int> getMove() override;
};

#endif // LOCAL_PLAYER_H
