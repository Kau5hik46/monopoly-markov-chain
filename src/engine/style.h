#pragma once
#include <string>

namespace monopoly::engine {

// ANSI styling, gated by `on` (auto-disabled when output is not a TTY). Color is
// strictly additive — every distinction is also carried by text/glyphs/labels.
struct Palette {
  bool on = false;

  std::string wrap(const char* code, const std::string& s) const {
    if (!on) return s;
    return std::string("\x1b[") + code + "m" + s + "\x1b[0m";
  }
  std::string bold(const std::string& s) const { return wrap("1", s); }
  std::string dim(const std::string& s) const { return wrap("2", s); }
  std::string red(const std::string& s) const { return wrap("31", s); }
  std::string green(const std::string& s) const { return wrap("32", s); }
  std::string yellow(const std::string& s) const { return wrap("33", s); }
  std::string cyan(const std::string& s) const { return wrap("36", s); }
};

// Visible (un-styled) length helpers and padding to fixed columns.
inline std::string padRight(const std::string& s, std::size_t n) {
  if (s.size() >= n) return s;
  return s + std::string(n - s.size(), ' ');
}
inline std::string padLeft(const std::string& s, std::size_t n) {
  if (s.size() >= n) return s;
  return std::string(n - s.size(), ' ') + s;
}
inline std::string truncate(const std::string& s, std::size_t n) {
  if (s.size() <= n) return s;
  if (n == 0) return "";
  return s.substr(0, n - 1) + "*";
}

// A section header rule: "TITLE ───────────────── right" padded to `width`.
inline std::string sectionHeader(const std::string& title, const std::string& right,
                                 const Palette& pal, std::size_t width = 78) {
  std::string left = title + " ";
  std::string r = right.empty() ? "" : (" " + right);
  std::size_t used = left.size() + r.size();
  std::size_t fill = (used < width) ? width - used : 1;
  std::string line = left + std::string(fill, '-') + r;
  // Bold the title segment and dim the rule fill if styling is on.
  if (!pal.on) return line;
  return pal.bold(title) + " " + pal.dim(std::string(fill, '-')) +
         (right.empty() ? "" : (" " + pal.bold(right)));
}

}  // namespace monopoly::engine
