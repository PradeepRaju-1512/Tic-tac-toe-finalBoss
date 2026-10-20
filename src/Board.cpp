/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: src/Board.cpp
 * 
 * [OOP PILLAR DEMONSTRATED: ENCAPSULATION]
 * 1. ENCAPSULATION OF STATE:
 *    - The 3x3 array `char grid[3][3]` and the state variable `int moveCount` 
 *      are private members.
 *    - No outside entity can overwrite cells directly or set arbitrary values.
 * 
 * 2. ENFORCEMENT OF INVARIANTS:
 *    - `makeMove(int row, int col, char symbol)` validates:
 *        a) Coordinate validity: row, col in [0..2]
 *        b) Cell availability: grid[row][col] == ' '
 *        c) Symbol validity: symbol in {'X', 'O'}
 *    - Because all mutations pass through this method, invalid states 
 *      (e.g., negative coordinates, overwriting an existing mark, or inserting 
 *      unauthorized characters like 'Z') are mathematically impossible.
 * 
 * 3. EXHAUSTIVE WIN / DRAW EVALUATION:
 *    - Evaluates all 8 winning lines (3 horizontal rows, 3 vertical columns, 
 *      1 primary diagonal, 1 anti-diagonal) in O(1) time.
 * ============================================================================
 */

#include "Board.h"
#include <iomanip>

/**
 * Constructor: Initializes an empty 3x3 board.
 * Calls reset() to avoid code duplication (DRY principle).
 */
Board::Board() {
    reset();
}

/**
 * Resets all 9 cells to empty space ' ' and resets move counter to zero.
 */
void Board::reset() {
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            grid[r][c] = ' ';
        }
    }
    moveCount = 0;
}

/**
 * [ENCAPSULATION IN ACTION]: Validates move legality before applying mutation.
 * 
 * @param row Zero-indexed row (0, 1, or 2).
 * @param col Zero-indexed column (0, 1, or 2).
 * @param symbol The player's marker ('X' or 'O').
 * @return true if valid and applied, false if out of bounds, cell occupied, or invalid symbol.
 */
bool Board::makeMove(int row, int col, char symbol) {
    // 1. Boundary checking
    if (row < 0 || row > 2 || col < 0 || col > 2) {
        return false;
    }

    // 2. Cell occupancy check
    if (grid[row][col] != ' ') {
        return false;
    }

    // 3. Symbol validity check
    if (symbol != 'X' && symbol != 'O') {
        return false;
    }

    // Apply validated mutation
    grid[row][col] = symbol;
    ++moveCount;
    return true;
}

/**
 * [DSA BACKTRACKING]: undoMove()
 * Resets cell to empty space ' ' and decrements moveCount.
 * Critical for Minimax game-tree depth-first search (DFS) state restoration.
 */
void Board::undoMove(int row, int col) {
    if (row >= 0 && row < 3 && col >= 0 && col < 3) {
        if (grid[row][col] != ' ') {
            grid[row][col] = ' ';
            --moveCount;
        }
    }
}

/**
 * Non-mutating inspector to test if a cell is legal to play.
 */
bool Board::isCellAvailable(int row, int col) const {
    if (row < 0 || row > 2 || col < 0 || col > 2) {
        return false;
    }
    return grid[row][col] == ' ';
}

/**
 * Helper: Checks all 8 winning lines for a specific symbol.
 * 
 * Line 0-2: Rows (grid[r][0] == sym && grid[r][1] == sym && grid[r][2] == sym)
 * Line 3-5: Columns (grid[0][c] == sym && grid[1][c] == sym && grid[2][c] == sym)
 * Line 6:   Primary Diagonal (grid[0][0] == sym && grid[1][1] == sym && grid[2][2] == sym)
 * Line 7:   Anti-Diagonal (grid[0][2] == sym && grid[1][1] == sym && grid[2][0] == sym)
 */
bool Board::checkWinCondition(char sym) const {
    // Check Rows
    for (int r = 0; r < 3; ++r) {
        if (grid[r][0] == sym && grid[r][1] == sym && grid[r][2] == sym) {
            return true;
        }
    }

    // Check Columns
    for (int c = 0; c < 3; ++c) {
        if (grid[0][c] == sym && grid[1][c] == sym && grid[2][c] == sym) {
            return true;
        }
    }

    // Check Primary Diagonal
    if (grid[0][0] == sym && grid[1][1] == sym && grid[2][2] == sym) {
        return true;
    }

    // Check Anti-Diagonal
    if (grid[0][2] == sym && grid[1][1] == sym && grid[2][0] == sym) {
        return true;
    }

    return false;
}

/**
 * Evaluates the terminal state of the board.
 * Returns GameStatus::X_WON, O_WON, DRAW, or IN_PROGRESS.
 */
GameStatus Board::getStatus() const {
    if (checkWinCondition('X')) {
        return GameStatus::X_WON;
    }
    if (checkWinCondition('O')) {
        return GameStatus::O_WON;
    }
    // If 9 moves have elapsed and no winner, it is a drawn game (stalemate).
    if (moveCount >= 9) {
        return GameStatus::DRAW;
    }
    return GameStatus::IN_PROGRESS;
}

/**
 * Read-only accessor for a specific cell marker.
 */
char Board::getCell(int row, int col) const {
    if (row < 0 || row > 2 || col < 0 || col > 2) {
        return '\0';
    }
    return grid[row][col];
}

/**
 * Serializes the 3x3 grid into a 9-character flat string in row-major order.
 * E.g., "X  O X   "
 * Used for lightweight WebSocket transport.
 */
std::string Board::serialize() const {
    std::string s;
    s.reserve(9);
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            s += grid[r][c];
        }
    }
    return s;
}

/**
 * Renders an educational ASCII representation of the board.
 * Includes column and row indices for CLI users and terminal logging.
 */
void Board::printBoard(std::ostream& os) const {
    os << "\n     0   1   2  \n";
    os << "   +---+---+---+\n";
    for (int r = 0; r < 3; ++r) {
        os << " " << r << " | ";
        for (int c = 0; c < 3; ++c) {
            os << grid[r][c] << " | ";
        }
        os << "\n   +---+---+---+\n";
    }
    os << std::endl;
}
