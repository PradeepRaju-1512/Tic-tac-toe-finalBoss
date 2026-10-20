/**
 * ============================================================================
 * Course: CS204 - Object-Oriented Systems & Network Programming (B.Tech CSE)
 * File: src/AIPlayer.cpp
 * 
 * [OOP PILLARS DEMONSTRATED: INHERITANCE & POLYMORPHISM]
 * 1. INHERITANCE:
 *    - Extends abstract class `Player`, chaining constructor initialization.
 * 
 * 2. RUN-TIME POLYMORPHISM:
 *    - Fulfills `virtual std::pair<int, int> getMove() override;`.
 *    - Called uniformly by `GameSession` through a base `Player*` pointer.
 * 
 * [DSA / DAA RIGOR: MINIMAX ALGORITHM RECURSIVE ANALYSIS]
 * 1. DEFINITION:
 *    - Minimax is a backtracking decision rule used in decision theory, game 
 *      theory, and artificial intelligence for two-player zero-sum games.
 * 
 * 2. RECURSIVE BASE CASES:
 *    - AI Win: Returns (+10 - depth). The subtraction of `depth` incentivizes 
 *      the AI to prioritize faster wins (e.g., winning in 1 move yields +9, 
 *      winning in 3 moves yields +7).
 *    - Opponent Win: Returns (depth - 10). The addition of `depth` incentivizes 
 *      the AI to delay inevitable losses as long as possible.
 *    - Draw: Returns 0. No utility gain for either player.
 * 
 * 3. BACKTRACKING TREE STATE RESTORATION:
 *    - Uses `board.makeMove()` to simulate branch descent, followed by 
 *      `board.undoMove()` upon return to restore node state (Depth-First Search).
 * ============================================================================
 */

#include "AIPlayer.h"
#include <algorithm>
#include <limits>
#include <iostream>

/**
 * Constructor: Initializes base Player attributes and identifies opponent's symbol.
 */
AIPlayer::AIPlayer(const std::string& playerName, char playerSymbol, Board* board)
    : Player(playerName, playerSymbol), 
      opponentSymbol(playerSymbol == 'X' ? 'O' : 'X'), 
      currentBoardRef(board) {
}

/**
 * Associates the active game board reference.
 */
void AIPlayer::setBoard(Board* board) {
    currentBoardRef = board;
}

/**
 * [DAA ALGORITHM CORE]: Recursive Minimax Evaluation Function
 * 
 * Explores all legal branches of the game tree.
 * 
 * @param board The simulated board.
 * @param depth Current recursion depth.
 * @param isMaximizing True if evaluating AI moves (maximize utility); 
 *                     false if evaluating opponent responses (minimize utility).
 * @return int Utility score for the current node.
 */
int AIPlayer::minimax(Board& board, int depth, bool isMaximizing) {
    GameStatus status = board.getStatus();

    // -------------------------------------------------------------
    // BASE CASE 1: Terminal Win Node
    // -------------------------------------------------------------
    if (status == GameStatus::X_WON) {
        // Did AI win or did opponent win?
        return (symbol == 'X') ? (10 - depth) : (depth - 10);
    }
    if (status == GameStatus::O_WON) {
        return (symbol == 'O') ? (10 - depth) : (depth - 10);
    }

    // -------------------------------------------------------------
    // BASE CASE 2: Terminal Draw Node (All 9 cells filled)
    // -------------------------------------------------------------
    if (status == GameStatus::DRAW) {
        return 0; // Neutral utility
    }

    // -------------------------------------------------------------
    // RECURSIVE STEP: Internal Game Tree Node
    // -------------------------------------------------------------
    if (isMaximizing) {
        // AI's turn: We want the HIGHEST possible score
        int maxEval = std::numeric_limits<int>::min();

        // Enumerate all legal candidate moves
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                if (board.isCellAvailable(r, c)) {
                    // 1. Simulate move (Tree Edge Down)
                    board.makeMove(r, c, symbol);

                    // 2. Recurse down tree (Opponent's turn to minimize)
                    int evaluation = minimax(board, depth + 1, false);

                    // 3. Backtrack: restore board state (Tree Edge Up)
                    board.undoMove(r, c);

                    // 4. Maximize utility
                    maxEval = std::max(maxEval, evaluation);
                }
            }
        }
        return maxEval;

    } else {
        // Opponent's turn: Assume opponent plays OPTIMALLY to MINIMIZE AI's score
        int minEval = std::numeric_limits<int>::max();

        // Enumerate all legal candidate moves for the opponent
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                if (board.isCellAvailable(r, c)) {
                    // 1. Simulate opponent move (Tree Edge Down)
                    board.makeMove(r, c, opponentSymbol);

                    // 2. Recurse down tree (AI's turn to maximize)
                    int evaluation = minimax(board, depth + 1, true);

                    // 3. Backtrack: restore board state (Tree Edge Up)
                    board.undoMove(r, c);

                    // 4. Minimize opponent utility
                    minEval = std::min(minEval, evaluation);
                }
            }
        }
        return minEval;
    }
}

/**
 * [OPTIMAL MOVE SELECTOR]: Evaluates all top-level moves and picks the optimal one.
 */
std::pair<int, int> AIPlayer::getBestMove(Board& board) {
    int bestScore = std::numeric_limits<int>::min();
    std::pair<int, int> bestMove = {-1, -1};

    // Fast-path heuristic: If board is completely empty, taking center (1, 1) is optimal
    if (board.isCellAvailable(1, 1) && board.getStatus() == GameStatus::IN_PROGRESS) {
        // Check if all cells empty
        bool allEmpty = true;
        for (int r = 0; r < 3 && allEmpty; ++r) {
            for (int c = 0; c < 3 && allEmpty; ++c) {
                if (!board.isCellAvailable(r, c)) allEmpty = false;
            }
        }
        if (allEmpty) {
            return {1, 1};
        }
    }

    // Traverse all 9 cells
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            if (board.isCellAvailable(r, c)) {
                // Try move
                board.makeMove(r, c, symbol);

                // Compute minimax utility of this branch
                // Next turn is opponent's, so isMaximizing = false
                int moveScore = minimax(board, 0, false);

                // Undo move (backtrack)
                board.undoMove(r, c);

                // Update best move if superior score discovered
                if (moveScore > bestScore) {
                    bestScore = moveScore;
                    bestMove = {r, c};
                }
            }
        }
    }

    std::cout << "[AIPlayer Minimax] Evaluated optimal move: (" 
              << bestMove.first << ", " << bestMove.second 
              << ") with utility score: " << bestScore << std::endl;

    return bestMove;
}

/**
 * [POLYMORPHIC OVERRIDE]: getMove()
 */
std::pair<int, int> AIPlayer::getMove() {
    if (currentBoardRef != nullptr) {
        return getBestMove(*currentBoardRef);
    }
    // Fallback if board not set
    return {-1, -1};
}
