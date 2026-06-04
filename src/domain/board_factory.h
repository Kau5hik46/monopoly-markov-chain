#pragma once
#include <string>
#include "domain/board.h"

namespace monopoly::domain {

// Loads a Board from a JSON file shaped like data/board.london.json.
// Throws std::runtime_error on malformed input or wrong square count.
Board loadBoardFromFile(const std::string& path);

}  // namespace monopoly::domain
