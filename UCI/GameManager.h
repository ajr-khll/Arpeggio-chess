#pragma once
#include "../minimax.h"
#include "UciOut.h"
#include "UciParser.h"
#include <thread>

class GameManager {
public:
  void uciGameLoop();

private:
  UciParser parser;
  UciOut sender;
  GameState state;
  void startSearch();
  void stopSearch();
  std::thread searchThread;
};

// GameManager has a UciParser, has a UciOut, and handles actual stop/start of
// move calculation.
