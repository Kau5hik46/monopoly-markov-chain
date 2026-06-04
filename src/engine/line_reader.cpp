#include "engine/line_reader.h"

#include <termios.h>
#include <unistd.h>

#include <string>
#include <vector>

namespace monopoly::engine {

namespace {

void redraw(std::ostream& term, const std::string& prompt, const std::string& buf) {
  term << "\r\x1b[K" << prompt << buf;  // CR + clear-to-EOL + prompt + buffer
  term.flush();
}

// Longest common prefix of a non-empty list.
std::string commonPrefix(const std::vector<std::string>& v) {
  if (v.empty()) return "";
  std::string p = v.front();
  for (const auto& s : v) {
    std::size_t k = 0;
    while (k < p.size() && k < s.size() && p[k] == s[k]) ++k;
    p.resize(k);
  }
  return p;
}

// Completes the @-token at the end of `buf`. Returns true if the buffer changed.
void complete(const NameTable& names, std::string& buf, std::ostream& term,
              const std::string& prompt) {
  const auto at = buf.rfind('@');
  if (at == std::string::npos) return;
  const std::string prefix = buf.substr(at + 1);
  auto matches = names.completions(prefix);
  if (matches.empty()) { term << "\a"; term.flush(); return; }
  if (matches.size() == 1) {
    buf = buf.substr(0, at + 1) + matches[0];
    redraw(term, prompt, buf);
    return;
  }
  const std::string lcp = commonPrefix(matches);
  if (lcp.size() > prefix.size()) buf = buf.substr(0, at + 1) + lcp;
  // List the candidates above a fresh prompt line.
  term << "\r\n";
  std::size_t shown = 0;
  for (const auto& m : matches) {
    if (shown++ >= 24) { term << "  ... (" << (matches.size() - 24) << " more)"; break; }
    term << "  " << m;
  }
  term << "\r\n";
  redraw(term, prompt, buf);
}

}  // namespace

bool readInteractiveLine(const NameTable& names, const std::string& prompt,
                         std::string& out, std::ostream& term) {
  termios oldt;
  if (tcgetattr(STDIN_FILENO, &oldt) != 0) return false;  // not a tty
  termios raw = oldt;
  raw.c_lflag &= ~(static_cast<tcflag_t>(ICANON | ECHO | ISIG));
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);

  std::string buf;
  redraw(term, prompt, buf);
  bool eof = false;
  for (;;) {
    char ch;
    const ssize_t n = ::read(STDIN_FILENO, &ch, 1);
    if (n <= 0) { eof = buf.empty(); break; }
    if (ch == '\n' || ch == '\r') { term << "\r\n"; term.flush(); break; }
    if (ch == 4) { eof = buf.empty(); if (eof) break; continue; }  // Ctrl-D
    if (ch == 3) { buf.clear(); term << "^C\r\n"; redraw(term, prompt, buf); continue; }
    if (ch == 127 || ch == 8) {  // backspace
      if (!buf.empty()) { buf.pop_back(); redraw(term, prompt, buf); }
      continue;
    }
    if (ch == '\t') { complete(names, buf, term, prompt); continue; }
    if (ch == 27) {  // swallow escape sequences (arrow keys etc.)
      char seq[2];
      ::read(STDIN_FILENO, &seq[0], 1);
      ::read(STDIN_FILENO, &seq[1], 1);
      continue;
    }
    if (static_cast<unsigned char>(ch) >= 32) {
      buf.push_back(ch);
      term << ch;
      term.flush();
    }
  }

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  if (eof) return false;
  out = buf;
  return true;
}

}  // namespace monopoly::engine
