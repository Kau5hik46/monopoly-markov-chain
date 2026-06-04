#pragma once
#include <string>

namespace monopoly::domain {

struct PlayerState {
  std::string name;
  int position = 0;     // current board square (0..39)
  long cash = 0;
  bool inJail = false;
  int jailAttempts = 0;
};

}  // namespace monopoly::domain
