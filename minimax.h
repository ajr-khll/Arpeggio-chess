#pragma once
#include "Moves.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <vector>

extern std::atomic_bool stop_search;
extern std::chrono::steady_clock::time_point deadline;
extern std::vector<uint64_t> positionHistory;

struct evaluatedMove : Move {
  int score = 0;
};

evaluatedMove Minimax(GameState state, int side, int alpha = -1000000,
                      int beta = 10000000, Move lastMove = {{-1, -1}, {-1, -1}},
                      int depth = 0);

evaluatedMove newMinimax(GameState state);
