#pragma once

#include "../Moves.h"
#include <string>
#include <vector>

class UciParser {
private:
  std::vector<std::string> commandParse(std::string line);

public:
  void poll();
  GameState positionParse(std::vector<std::string> fenInput);
  std::vector<std::string> split(std::string line);
};
