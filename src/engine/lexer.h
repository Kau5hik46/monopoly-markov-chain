#pragma once
#include <string>
#include <vector>

namespace monopoly::engine {

// Splits a DSL line into tokens. Multi-char operators (-> <-> += -=) are recognized
// before single-char operators (= , @ : + - ^). Words are [A-Za-z0-9_.#] runs.
std::vector<std::string> lex(const std::string& line);

}  // namespace monopoly::engine
