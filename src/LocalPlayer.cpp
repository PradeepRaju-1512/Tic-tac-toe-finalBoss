/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: src/LocalPlayer.cpp
 * 
 * [OOP PILLARS DEMONSTRATED: INHERITANCE & POLYMORPHISM]
 * 1. INHERITANCE:
 *    - Concrete subclass of `Player`.
 *    - Demonstrates constructor chaining (`Player(playerName, playerSymbol)`).
 * 
 * 2. RUN-TIME POLYMORPHISM:
 *    - Overrides `virtual std::pair<int, int> getMove() override;`.
 *    - When the orchestrator holds a `Player*` pointer and invokes `getMove()`,
 *      the vtable dispatches to this console prompt logic.
 * 
 * [ROBUST TERMINAL I/O & RESILIENCE]:
 *    - In systems programming, std::cin stream corruption from invalid user
 *      input (e.g. typing characters when numbers are expected) causes the
 *      failbit to be set, leading to infinite input loops if unhandled.
 *    - This implementation validates stream integrity, clears `std::cin.clear()`,
 *      and purges the input buffer with `std::cin.ignore()`.
 * ============================================================================
 */

#include "LocalPlayer.h"
#include <limits>

/**
 * Constructor: Initializes base class and stream references.
 * Stream dependency injection allows testing via std::stringstream in unit tests.
 */
LocalPlayer::LocalPlayer(const std::string& playerName, 
                         char playerSymbol, 
                         std::istream& in, 
                         std::ostream& out)
    : Player(playerName, playerSymbol), inStream(in), outStream(out) {
}

/**
 * [POLYMORPHIC CORE]: getMove()
 * 
 * Prompts the local terminal operator to enter row and column coordinates.
 * Validates stream state and ensures inputs are integer coordinates in [0..2].
 * 
 * @return std::pair<int, int> Validated (row, col) coordinates.
 */
std::pair<int, int> LocalPlayer::getMove() {
    int row = -1;
    int col = -1;

    while (true) {
        outStream << "\n[" << name << " (" << symbol 
                  << ")] Enter your move (row [0-2] and column [0-2], separated by space): ";

        // Attempt to read two integers
        if (inStream >> row >> col) {
            // Check boundary bounds
            if (row >= 0 && row <= 2 && col >= 0 && col <= 2) {
                return {row, col};
            }
            outStream << "[ERROR] Coordinates out of bounds! Row and Column must be 0, 1, or 2.\n";
        } else {
            // Stream failure (e.g. non-numeric input typed)
            outStream << "[ERROR] Invalid input! Please enter integer values only.\n";
            inStream.clear(); // Clear failbit state
            inStream.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Discard bad line
        }
    }
}
