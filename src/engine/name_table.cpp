#include "engine/name_table.h"

#include <algorithm>
#include <cctype>

namespace monopoly::engine {

std::string NameTable::normalize(const std::string& name) {
  std::string out;
  bool lastUnderscore = false;
  for (char c : name) {
    if (std::isalnum(static_cast<unsigned char>(c))) {
      out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
      lastUnderscore = false;
    } else if (!lastUnderscore && !out.empty()) {
      out.push_back('_');
      lastUnderscore = true;
    }
  }
  while (!out.empty() && out.back() == '_') out.pop_back();
  return out;
}

NameTable::NameTable(const domain::Board& board) {
  for (const auto& sq : board.squares())
    byName_[normalize(sq.name)] = sq.position;
}

std::optional<int> NameTable::resolve(const std::string& ref) const {
  if (ref.empty()) return std::nullopt;
  std::string r = ref;
  if (r.front() == '#') r = r.substr(1);
  // Numeric position?
  bool numeric = !r.empty();
  for (char c : r) if (!std::isdigit(static_cast<unsigned char>(c))) numeric = false;
  if (numeric) {
    int pos = std::stoi(r);
    if (pos >= 0 && pos < domain::kBoardSize) return pos;
    return std::nullopt;
  }
  auto it = byName_.find(normalize(ref));
  if (it != byName_.end()) return it->second;
  return std::nullopt;
}

std::vector<std::string> NameTable::completions(const std::string& prefix) const {
  const std::string p = normalize(prefix);
  std::vector<std::string> out;
  for (const auto& kv : byName_)
    if (kv.first.rfind(p, 0) == 0) out.push_back(kv.first);
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

}  // namespace monopoly::engine
