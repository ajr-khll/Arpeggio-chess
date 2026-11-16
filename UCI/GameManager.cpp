#include "GameManager.h"
#include "../timer.cpp"
#include <chrono>
#include <string>

void GameManager::startSearch() {
  stopSearch();
  stop_search = false;
  bool white = state.sideToMove == 0;
  int myTime = white ? time.wtime : time.btime;
  int myInc = white ? time.winc : time.binc;

  int budget;
  if (time.movetime > 0) {
    budget = time.movetime;
  } else if (myTime > 0) {
    budget = myTime / 20 + myInc / 2;
  } else {
    budget = 1000;
  }

  deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(budget);
  searchThread = std::thread([this, s = state] {
    evaluatedMove m = newMinimax(s);
    sender.sendMove(m);
  });
}

void GameManager::stopSearch() {
  stop_search = true;
  if (searchThread.joinable())
    searchThread.join();
}

void GameManager::uciGameLoop() {
  std::string input;
  while (std::getline(std::cin, input)) {
    std::vector<std::string> parsedInput = parser.split(input);
    if (parsedInput.empty())
      continue;
    if (parsedInput[0] == "uci") {
      sender.uciHandshake();
    } else if (parsedInput[0] == "isready") {
      sender.uciReady();
    } else if (parsedInput[0] == "position") {
      parsedInput.erase(
          parsedInput.begin()); // remove "position", keep the actual data.
      state = parser.positionParse(parsedInput);
    } else if (parsedInput[0] == "go") {
      time = parser.goParser(parsedInput);
      startSearch();
    } else if (parsedInput[0] == "stop") {
      stopSearch();
    }
  }
}
