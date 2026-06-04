#pragma once
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "domain/board.h"

namespace monopoly::engine {

// Resolves square references in the DSL: either "#<pos>" or a normalized name
// (uppercase, runs of non-alphanumerics -> single underscore), e.g. TRAFALGAR_SQUARE.
class NameTable {
 public:
  explicit NameTable(const domain::Board& board);

  // Accepts "#24", "24", or "TRAFALGAR_SQUARE". Returns nullopt if unknown.
  std::optional<int> resolve(const std::string& ref) const;

  // Normalized square names starting with `prefix` (sorted), for @-autocomplete.
  std::vector<std::string> completions(const std::string& prefix) const;

  static std::string normalize(const std::string& name);

 private:
  std::unordered_map<std::string, int> byName_;
};

}  // namespace monopoly::engine
