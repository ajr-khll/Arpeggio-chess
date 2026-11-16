#include "UciOut.h"
#include "../TextureManager.h"
#include <iostream>

void UciOut::sendMove(Move bestMove) {
  // convert move into correct notation
  std::string from = "";
  from += (char)(bestMove.from.x + 'a');
  from += (char)('8' - bestMove.from.y);

  std::string to = "";
  to += (char)(bestMove.to.x + 'a');
  to += (char)('8' - bestMove.to.y);

  std::string promotion = "";
  switch (bestMove.promotion) {
  case W_QUEEN:
  case B_QUEEN:
    promotion = "q";
    break;
  case W_ROOK:
  case B_ROOK:
    promotion = "r";
    break;
  case W_BISHOP:
  case B_BISHOP:
    promotion = "b";
    break;
  case W_KNIGHT:
  case B_KNIGHT:
    promotion = "n";
    break;
  }

  std::cout << "bestmove " << from << to << promotion << std::endl;
};

void UciOut::uciHandshake() {
  // respond with engine id, then uciok
  std::cout << "id name arpeggio" << std::endl;
  std::cout << "id author AJ Khullar" << std::endl;
  std::cout << "uciok" << std::endl;
}

void UciOut::uciReady() {
  // respond with readyok
  std::cout << "readyok" << std::endl;
}
