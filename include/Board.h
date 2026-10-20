/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: include/Board.h
 * 
 * [OOP PILLAR APPLIED: ENCAPSULATION]
 * The internal representation of the 3x3 game grid is strictly private.
 * External consumers (players, network managers, game loops) are barred from
 * directly reading or modifying the memory backing the grid array.
 * 
 * Every modification must pass through makeMove(), which validates:
 *  1. Boundary constraints (0 <= row <= 2, 0 <= col <= 2)
 *  2. Cell vacancy (grid[row][col] == ' ')
 *  3. Symbol validity (symbol == 'X' || symbol == 'O')
 * 
 * This guarantees class invariants are maintained and board state cannot
 * be corrupted by malicious or invalid network packets.
 * ============================================================================
 */

#ifndef BOARD_H
#define BOARD_H

#include <vector>
#include <string>
#include <iostream>

/**
 * @enum GameStatus
 * @brief Represents the terminal or ongoing state of the Tic-Tac-Toe board.
 */
enum class GameStatus {
    IN_PROGRESS, ///< Moves are still available and no win condition has been met.
    X_WON,       ///< Player X has aligned three marks horizontally, vertically, or diagonally.
    O_WON,       ///< Player O has aligned three marks horizontally, vertically, or diagonally.
    DRAW         ///< All 9 cells are filled with no winning alignment.
};

/**
 * @class Board
 * @brief Encapsulates a 3x3 Tic-Tac-Toe game grid and enforces move legality.
 */
class Board {
private:
    // [ENCAPSULATION]: Fixed 3x3 array strictly hidden from outside access.
    // Initialized to ' ' (empty space).
    char grid[3][3];

    // [ENCAPSULATION]: Internal move counter to determine draw states efficiently.
    int moveCount;

    /**
     * @brief Evaluates whether a specific symbol has 3 consecutive marks.
     * 
     * Checks all 3 rows, 3 columns, the main diagonal, and anti-diagonal.
     * 
     * @param sym Target marker ('X' or 'O').
     * @return true if sym satisfies a winning condition, false otherwise.
     */
    bool checkWinCondition(char sym) const;

public:
    /**
     * @brief Constructs an empty 3x3 board.
     * Initializes all cells to space ' ' and moveCount to 0.
     */
    Board();

    /**
     * @brief Virtual destructor for clean memory deallocation.
     */
    virtual ~Board() = default;

    /**
     * @brief Resets the board back to empty state.
     */
    void reset();

    /**
     * @brief Validates and executes a move on the board.
     * 
     * [ENCAPSULATION IN ACTION]: Validates coordinates and cell availability
     * before modifying internal state. Returns false if illegal.
     * 
     * @param row Row index (0 to 2).
     * @param col Column index (0 to 2).
     * @param symbol Mark to place ('X' or 'O').
     * @return true if move was legal and applied, false if coordinates invalid or cell occupied.
     */
    bool makeMove(int row, int col, char symbol);

    /**
     * @brief Undoes a move on the board (used by Minimax recursive backtracking).
     * 
     * [DSA BACKTRACKING]: Resets the cell to empty ' ' and decrements moveCount,
     * restoring previous game tree node state during recursive exploration.
     * 
     * @param row Row index (0 to 2).
     * @param col Column index (0 to 2).
     */
    void undoMove(int row, int col);

    /**
     * @brief Non-mutating inspector to check if a specific cell can be played.
     * @param row Row index (0 to 2).
     * @param col Column index (0 to 2).
     * @return true if cell is within bounds and currently empty (' ').
     */
    bool isCellAvailable(int row, int col) const;

    /**
     * @brief Non-mutating accessor for current game status.
     * Evaluates X_WON, O_WON, DRAW, or IN_PROGRESS.
     * @return Current GameStatus enum.
     */
    GameStatus getStatus() const;

    /**
     * @brief Read-only accessor for a specific cell marker.
     * @param row Row index.
     * @param col Column index.
     * @return Character at cell (' ', 'X', or 'O'), or '\0' if out of bounds.
     */
    char getCell(int row, int col) const;

    /**
     * @brief Serializes the current board state into a flat 9-character string.
     * Useful for JSON payloads over WebSockets (e.g. "X OX O   ").
     * @return std::string Exactly 9 characters representing the row-major grid.
     */
    std::string serialize() const;

    /**
     * @brief Renders the 3x3 grid to a standard output stream for terminal debugging.
     * @param os Output stream (defaults to std::cout).
     */
    void printBoard(std::ostream& os = std::cout) const;
};

#endif // BOARD_H
