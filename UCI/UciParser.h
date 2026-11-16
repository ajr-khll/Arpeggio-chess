#pragma once

#include "../Moves.h"
#include "TimeState.h"
#include <string>
#include <vector>

class UciParser {
private:
  std::vector<std::string> commandParse(std::string line);

public:
  void poll();
  GameState positionParse(std::vector<std::string> fenInput);
  std::vector<std::string> split(std::string line);
  TimeState goParser(std::vector<std::string> line);
};
