#pragma once
#include <cmath>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

namespace monopoly::engine {

// Parses an amount token like "500000", "500K", "2M", "1.5M" into pounds.
// Returns nullopt on malformed input.
inline std::optional<long> parseAmount(const std::string& tok) {
  if (tok.empty()) return std::nullopt;
  double mult = 1.0;
  std::string num = tok;
  const char last = tok.back();
  if (last == 'K' || last == 'k') { mult = 1e3; num = tok.substr(0, tok.size() - 1); }
  else if (last == 'M' || last == 'm') { mult = 1e6; num = tok.substr(0, tok.size() - 1); }
  if (num.empty()) return std::nullopt;
  try {
    std::size_t consumed = 0;
    double v = std::stod(num, &consumed);
    if (consumed != num.size()) return std::nullopt;
    return static_cast<long>(std::llround(v * mult));
  } catch (...) {
    return std::nullopt;
  }
}

// Process-wide display currency symbol (UTF-8). Set once at startup from the chosen
// board (e.g. "£" for London, "$" for the US board); defaults to £.
inline std::string& moneySymbol() {
  static std::string sym = "\xC2\xA3";
  return sym;
}

// Formats an amount in K/M units with the active currency symbol: £1.61M, $250.0K, $48.
inline std::string formatMoney(double v) {
  std::ostringstream os;
  const double a = std::fabs(v);
  os << moneySymbol();
  if (a >= 1e6) os << std::fixed << std::setprecision(2) << v / 1e6 << "M";
  else if (a >= 1e3) os << std::fixed << std::setprecision(1) << v / 1e3 << "K";
  else os << std::fixed << std::setprecision(0) << v;
  return os.str();
}

}  // namespace monopoly::engine
