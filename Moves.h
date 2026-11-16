#pragma once
#include <SFML/System/Vector2.hpp>
#include <optional>
#include <vector>

struct Move {
  sf::Vector2i from;
  sf::Vector2i to;
  int promotion = 0; // piece to promote to (e.g. W_QUEEN), 0 if none
};

// castling rights, stored as bit flags in GameState::castling
enum CastlingRights {
  W_KINGSIDE = 1,
  W_QUEENSIDE = 2,
  B_KINGSIDE = 4,
  B_QUEENSIDE = 8
};

struct Undo {
  Move move;
  int movedPiece;
  int capturedPiece;
  sf::Vector2i kingPosBefore[2];
};

struct GameState {
  int board[8][8];
  int sideToMove;
  sf::Vector2i kingPos[2];
  int castling = 0;
};

void startPos(GameState &state);

std::vector<Move> calculatePossibleMoves(int piece, sf::Vector2i position,
                                         GameState game);

std::vector<Move> gatherMoves(GameState game, int side,
                              Move priority = {{-1, -1}, {-1, -1}});

bool isInCheck(GameState current, int color);

bool isCheckmated(GameState state, int side);

int colorOf(int piece);

GameState
makeMove(const GameState &current, const Move &move,
         const std::optional<std::vector<Move>> &moves = std::nullopt);
