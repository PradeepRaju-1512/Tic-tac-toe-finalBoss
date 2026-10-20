/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: include/AIPlayer.h
 * 
 * [OOP PILLAR APPLIED: INHERITANCE & POLYMORPHISM]
 * AIPlayer derives from the abstract Player base class.
 * It fulfills the pure virtual contract:
 *   `virtual std::pair<int, int> getMove() override;`
 * 
 * The GameSession treats AIPlayer identically to LocalPlayer or WebSocketPlayer.
 * This demonstrates runtime polymorphism without modifying a single line of
 * board win-checking or turn-alternation code.
 * 
 * [DSA / DAA CONCEPTS DEMONSTRATED: MINIMAX ALGORITHM & ZERO-SUM GAME THEORY]
 * 1. GAME TREE RECURSION:
 *    - Tic-Tac-Toe is a finite, deterministic, two-player, zero-sum game with
 *      perfect information.
 *    - The Minimax algorithm evaluates all possible future board states up to
 *      terminal nodes (Win, Loss, Draw) via Depth-First Search (DFS).
 * 
 * 2. HEURISTIC EVALUATION & DEPTH PENALTY:
 *    - AI Win  = +10 - depth  (Favors faster wins)
 *    - AI Loss = depth - 10   (Favors delayed losses)
 *    - Draw    = 0
 * 
 * 3. TIME COMPLEXITY (DAA ANALYSIS):
 *    - Branching factor b <= 9, maximum depth d = 9.
 *    - Worst-case game tree leaves: 9! = 362,880 states.
 *    - Time Complexity: O(b^d), easily computed in < 15ms without alpha-beta.
 *    - Space Complexity: O(d) call stack frames (maximum recursion depth 9).
 * ============================================================================
 */

#ifndef AI_PLAYER_H
#define AI_PLAYER_H

#include "Player.h"
#include "Board.h"
#include <utility>

/**
 * @class AIPlayer
 * @brief Autonomous AI participant using the recursive Minimax decision rule.
 */
class AIPlayer : public Player {
private:
    char opponentSymbol; // 'X' or 'O'
    Board* currentBoardRef; // Reference to the active board state

    /**
     * @brief Recursive Minimax algorithm for decision-tree traversal.
     * 
     * Base Cases:
     *   1. AI has won: return (+10 - depth)
     *   2. Opponent has won: return (depth - 10)
     *   3. Draw / Board full: return 0
     * 
     * Recursive Step:
     *   - If isMaximizing == true:
     *       maxScore = -infinity
     *       For each empty cell (r, c):
     *           board.makeMove(r, c, aiSymbol)
     *           score = minimax(board, depth + 1, false)
     *           board.undoMove(r, c)
     *           maxScore = max(maxScore, score)
     *       return maxScore
     *   - If isMaximizing == false (Opponent minimizing):
     *       minScore = +infinity
     *       For each empty cell (r, c):
     *           board.makeMove(r, c, opponentSymbol)
     *           score = minimax(board, depth + 1, true)
     *           board.undoMove(r, c)
     *           minScore = min(minScore, score)
     *       return minScore
     * 
     * @param board The active game board being evaluated.
     * @param depth Current recursion depth (used to penalize longer paths to victory).
     * @param isMaximizing True if it is the AI's turn to maximize score; false if opponent minimizes.
     * @return int Computed heuristic utility score of this game tree node.
     */
    int minimax(Board& board, int depth, bool isMaximizing);

public:
    /**
     * @brief Constructs an AIPlayer with symbol and assigned board reference.
     * @param playerName Name of the bot (e.g., "Minimax Bot").
     * @param playerSymbol AI marker ('X' or 'O').
     * @param board Optional pointer to active board.
     */
    AIPlayer(const std::string& playerName = "Minimax Bot (AI)", 
             char playerSymbol = 'O', 
             Board* board = nullptr);

    /**
     * @brief Virtual destructor for clean polymorphic disposal.
     */
    ~AIPlayer() override = default;

    /**
     * @brief Associates the active board pointer with the AI player.
     * @param board Pointer to the active Board instance.
     */
    void setBoard(Board* board);

    /**
     * @brief Computes the mathematically optimal move using Minimax.
     * 
     * Loops through all legal moves, simulates each move, executes minimax(),
     * and selects the move with the highest utility score.
     * 
     * @param board The board to evaluate.
     * @return std::pair<int, int> Best (row, col) coordinates.
     */
    std::pair<int, int> getBestMove(Board& board);

    /**
     * @brief Polymorphic getMove() override satisfying the Player interface.
     * Uses internal board reference to calculate the optimal move.
     * 
     * @return std::pair<int, int> The optimal coordinates.
     */
    std::pair<int, int> getMove() override;
};

#endif // AI_PLAYER_H
