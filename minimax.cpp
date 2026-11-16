#include "minimax.h"
#include "Moves.h"
#include "TextureManager.h"
#include "simulateMoves.h"
#include <SFML/Window/Keyboard.hpp>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <optional>

// Array for values of pieces
// 0: Empty
// 1-6:  W_PAWN, W_ROOK, W_KNIGHT, W_BISHOP, W_QUEEN, W_KING
// 7-12: B_PAWN, B_ROOK, B_KNIGHT, B_BISHOP, B_QUEEN, B_KING
// (must match the PieceIDs order in TextureManager.h)

enum colors { WHITE = 0, BLACK = 1 };

static const int PIECE_VALUES[13] = {
    0,     // EMPTY
    100,   // W_PAWN
    500,   // W_ROOK
    320,   // W_KNIGHT
    330,   // W_BISHOP
    900,   // W_QUEEN
    20000, // W_KING (Arbitrarily high so the engine never sacrifices it)
    100,   // B_PAWN
    500,   // B_ROOK
    320,   // B_KNIGHT
    330,   // B_BISHOP
    900,   // B_QUEEN
    20000  // B_KING
};

// --------------------------------------------------------------------------
// PIECE-SQUARE TABLES (White's Perspective)
// --------------------------------------------------------------------------
// These are flat arrays of 64 integers.
// Access them using: table[y * 8 + x]

// PAWNS: Encourage moving forward and controlling the center (d4/e4).
static const int mvv_luv[64] = {
    0,   0,   0,   0,
    0,   0,   0,   0, // Rank 8 (Promoted - usually irrelevant here)
    50,  50,  50,  50,
    50,  50,  50,  50, // Rank 7 (Almost promoted!)
    10,  10,  20,  30,
    30,  20,  10,  10, // Rank 6
    5,   5,   10,  25,
    25,  10,  5,   5, // Rank 5
    0,   0,   0,   20,
    20,  0,   0,   0, // Rank 4
    5,   -5,  -10, 0,
    0,   -10, -5,  5, // Rank 3
    5,   10,  10,  -20,
    -20, 10,  10,  5, // Rank 2 (Don't move f/g/h pawns too early)
    0,   0,   0,   0,
    0,   0,   0,   0 // Rank 1 (Base)
};

// KNIGHTS: Strong in the center, terrible at the edges/corners.
static const int knight_pst[64] = {
    -50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,   0,   0,
    0,   -20, -40, -30, 0,   10,  15,  15,  10,  0,   -30, -30, 5,
    15,  20,  20,  15,  5,   -30, -30, 0,   15,  20,  20,  15,  0,
    -30, -30, 5,   10,  15,  15,  10,  5,   -30, -40, -20, 0,   5,
    5,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50};

// BISHOPS: Good on long diagonals, better in center than corners.
static const int bishop_pst[64] = {
    -20, -10, -10, -10, -10, -10, -10, -20, -10, 0,   0,   0,   0,
    0,   0,   -10, -10, 0,   5,   10,  10,  5,   0,   -10, -10, 5,
    5,   10,  10,  5,   5,   -10, -10, 0,   10,  10,  10,  10,  0,
    -10, -10, 10,  10,  10,  10,  10,  10,  -10, -10, 5,   0,   0,
    0,   0,   5,   -10, -20, -10, -10, -10, -10, -10, -10, -20};

// ROOKS: Bonus for 7th rank (attacking enemy pawns) and centering.
static const int rook_pst[64] = {
    0, 0, 0, 0, 0, 0, 0, 0, 5, 10, 10, 10, 10, 10, 10, 5, // 7th Rank (Pig
                                                          // on the 7th)
    -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0,
    -5, -5, 0, 0, 0, 0, 0, 0, -5, -5, 0, 0, 0, 0, 0, 0, -5, 0, 0, 0, 5, 5, 0, 0,
    0 // Castling squares
      // slightly better
};

// QUEENS: Generally kept simple. Avoid corners, stay somewhat central.
static const int queen_pst[64] = {
    -20, -10, -10, -5, -5, -10, -10, -20, -10, 0,   0,   0,  0,  0,   0,   -10,
    -10, 0,   5,   5,  5,  5,   0,   -10, -5,  0,   5,   5,  5,  5,   0,   -5,
    0,   0,   5,   5,  5,  5,   0,   -5,  -10, 5,   5,   5,  5,  5,   0,   -10,
    -10, 0,   5,   0,  0,  0,   0,   -10, -20, -10, -10, -5, -5, -10, -10, -20};

// KINGS (MIDDLE GAME): Highly penalized for being in the center or open files.
// Encourages castling into the corners (g1/b1).
static const int king_pst[64] = {
    -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40,
    -30, -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40,
    -40, -30, -20, -30, -30, -40, -40, -30, -30, -20, -10, -20, -20, -20, -20,
    -20, -20, -10, 20,  20,  0,   0,   0,   0,   20,  20, // Pawn shield rank
    20,  30,  10,  0,   0,   10,  30,  20 // Back rank (Corners are safest)
};

int positionalScore(int piece, int x, int y) {
  if (piece == EMPTY)
    return 0;

  const int *table = nullptr;
  if (piece == W_PAWN || piece == B_PAWN)
    table = mvv_luv;
  else if (piece == W_KNIGHT || piece == B_KNIGHT)
    table = knight_pst;
  else if (piece == W_BISHOP || piece == B_BISHOP)
    table = bishop_pst;
  else if (piece == W_ROOK || piece == B_ROOK)
    table = rook_pst;
  else if (piece == W_QUEEN || piece == B_QUEEN)
    table = queen_pst;
  else if (piece == W_KING || piece == B_KING)
    table = king_pst;

  if (!table)
    return 0;

  if (colorOf(piece) == WHITE) {
    return table[y * 8 + x];
  } else {
    return table[(7 - y) * 8 + x];
  }
}

int evaluateScore(GameState state, int color) {
  // Given a game state, return an integer value for the score of the game
  int score = 0;
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      int piece = state.board[y][x];
      if (colorOf(piece) == color) {
        score += PIECE_VALUES[piece];
        score += positionalScore(piece, x, y);

      } else {
        score -= PIECE_VALUES[state.board[y][x]];
        score -= positionalScore(piece, x, y);
      }
    }
  }
  return score;
}

// Minimax algorithm, recursive
const int CAP = 5; // think no more than 5 moves ahead

evaluatedMove Minimax(GameState state, int side, int alpha, int beta,
                      Move lastMove, int depth) {
  // base case

  if (depth == CAP) {
    int score = evaluateScore(state, BLACK);
    if (score > alpha) {
      alpha = score;
    }
    return {lastMove, score};
  }
  Move nullMove = {{-1, -1}, {-1, -1}};
  evaluatedMove bestMove{nullMove};
  if (side == BLACK)
    bestMove.score = -999999;
  else
    bestMove.score = 999999;
  // recursive case. keep growing tree until depth == 4
  // first, get every possible move
  std::vector<Move> moves = gatherMoves(state, side);
  if (moves.empty()) {
    if (isInCheck(state, side)) {
      // If White is mated, return a very high number (but slightly lower if it
      // takes longer) If Black is mated, return a very low number (but
      // slightly higher if it takes longer)
      return {{{-1, -1}, {-1, -1}},
              (side == WHITE) ? 1000000 + depth : -1000000 - depth};
    } else {
      return {{{-1, -1}, {-1, -1}}, 0};
    }
  }
  // Now moves is a list of all possible moves we can make in our current
  // state.
  int enemy = !side;
  for (Move move : moves) {
    evaluatedMove result =
        Minimax(simulateMove(state, move), enemy, alpha, beta, move, depth + 1);
    if (side == BLACK) {
      if (result.score > alpha) {
        alpha = result.score;
      }
      if (result.score > bestMove.score) {
        bestMove = {move, result.score};
      }
      if (alpha >= beta) {
        break;
      }
    } else {
      if (result.score < beta) {
        beta = result.score;
      }
      if (result.score < bestMove.score) {
        bestMove = {move, result.score};
      }
      if (beta <= alpha) {
        break;
      }
    }
  }

  return bestMove;
}

std::atomic_bool stop_search{false};
std::chrono::steady_clock::time_point deadline;

// follow all capture-to-capture paths on the board to find any potential
// blunders.
int quiescenceSearch(GameState state, int alpha, int beta) {
  int standPat = evaluateScore(state, BLACK);
  bool black = state.sideToMove == BLACK;

  if (black) {
    if (standPat >= beta)
      return standPat;
    if (standPat > alpha)
      alpha = standPat;
  } else {
    if (standPat <= alpha)
      return standPat;
    if (standPat < beta)
      beta = standPat;
  }

  int best = standPat;
  for (Move move : gatherMoves(state, state.sideToMove, true)) {
    int score = quiescenceSearch(simulateMove(state, move), alpha, beta);
    if (black) {
      if (score > best)
        best = score;
      if (score > alpha)
        alpha = score;
    } else {
      if (score < best)
        best = score;
      if (score < beta)
        beta = score;
    }
    if (alpha >= beta)
      break;
  }
  return best;
}

int minimaxScore(GameState state, int maxDepth, int alpha = -1000000,
                 int beta = 1000000) {

  if (maxDepth == 0) {
    // base case
    int score = evaluateScore(state, BLACK);
    // consider if any standing moves are blunders
    score = quiescenceSearch(state, alpha, beta);
    return score;
  }

  std::vector<Move> moves = gatherMoves(state, state.sideToMove);
  if (moves.empty()) {
    if (isInCheck(state, state.sideToMove)) {
      // If White is mated, return a very high number (but slightly lower if
      // it takes longer) If Black is mated, return a very low number (but
      // slightly higher if it takes longer)
      return (state.sideToMove == WHITE) ? 1000000 + maxDepth
                                         : -1000000 - maxDepth;
    } else {
      return 0;
    }
  }
  int score = (state.sideToMove == BLACK) ? -10000000 : 10000000;

  for (Move move : moves) {
    if (std::chrono::steady_clock::now() > deadline) {
      stop_search = true;
    }
    if (stop_search) {
      return -100000;
    }
    int moveScore =
        minimaxScore(simulateMove(state, move), maxDepth - 1, alpha, beta);
    if (state.sideToMove == BLACK) {
      // Black maximizes

      if (moveScore > alpha) {
        alpha = moveScore;
      }
      if (moveScore > score) {
        score = moveScore;
      }
      if (alpha >= beta) {
        break;
      }
    } else if (state.sideToMove == WHITE) {
      // White minimizes
      if (moveScore < beta) {
        beta = moveScore;
      }
      if (moveScore < score) {
        score = moveScore;
      }
      if (beta <= alpha) {
        break;
      }
    }
  }

  return score; // score of this move
}

evaluatedMove rootSearch(GameState state, int maxDepth,
                         Move lastBest = {{-1, -1}, {-1, -1}}) {
  std::vector<Move> moves =
      gatherMoves(state, state.sideToMove, false, lastBest);
  evaluatedMove bestMove{{{-1, -1}, {-1, -1}},
                         (state.sideToMove == BLACK) ? -10000000 : 10000000};
  for (Move move : moves) {
    int score = minimaxScore(simulateMove(state, move), maxDepth - 1);
    if (std::chrono::steady_clock::now() > deadline) {
      stop_search = true;
    }
    if (stop_search) {
      return {{{-1, -1}, {-1, -1}},
              -1000}; // don't trust anything from this branch,
      // we haven't considered all enemy responses.
    }
    if (score > bestMove.score && state.sideToMove == BLACK) {
      bestMove = {move, score};
    } else if (score < bestMove.score && state.sideToMove == WHITE) {
      bestMove = {move, score};
    }
  }
  return bestMove;
};

// rewrite of Minimax algorithm with iterative deepening
evaluatedMove newMinimax(GameState state) {
  evaluatedMove bestMove{{{-1, -1}, {-1, -1}}};
  for (int d = 1; d < 64; d++) {
    if (stop_search) {
      return bestMove;
    }
    evaluatedMove algoMove = rootSearch(state, d, bestMove);
    if (!stop_search) {
      bestMove = algoMove;
    }
  }

  return bestMove;
}
