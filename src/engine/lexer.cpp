#include "engine/lexer.h"

#include <cctype>

namespace monopoly::engine {

namespace {
bool isWordChar(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '.' ||
         c == '#';
}
bool starts(const std::string& s, std::size_t i, const char* op) {
  for (std::size_t k = 0; op[k]; ++k)
    if (i + k >= s.size() || s[i + k] != op[k]) return false;
  return true;
}
}  // namespace

std::vector<std::string> lex(const std::string& line) {
  std::vector<std::string> out;
  std::size_t i = 0;
  const std::size_t n = line.size();
  while (i < n) {
    if (std::isspace(static_cast<unsigned char>(line[i]))) { ++i; continue; }
    if (starts(line, i, "<->")) { out.emplace_back("<->"); i += 3; continue; }
    if (starts(line, i, "->")) { out.emplace_back("->"); i += 2; continue; }
    if (starts(line, i, "+=")) { out.emplace_back("+="); i += 2; continue; }
    if (starts(line, i, "-=")) { out.emplace_back("-="); i += 2; continue; }
    const char c = line[i];
    if (c == '=' || c == ',' || c == '@' || c == ':' || c == '+' || c == '-' ||
        c == '^') {
      out.emplace_back(1, c);
      ++i;
      continue;
    }
    if (isWordChar(c)) {
      std::size_t j = i;
      while (j < n && isWordChar(line[j])) ++j;
      out.push_back(line.substr(i, j - i));
      i = j;
      continue;
    }
    ++i;  // skip unknown char
  }
  return out;
}

}  // namespace monopoly::engine
