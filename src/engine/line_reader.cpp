#include "engine/line_reader.h"

#include <termios.h>
#include <unistd.h>

#include <string>
#include "engine/completion.h"

namespace monopoly::engine {

namespace {

void redraw(std::ostream& term, const std::string& prompt, const std::string& buf) {
  term << "\r\x1b[K" << prompt << buf;  // CR + clear-to-EOL + prompt + buffer
  term.flush();
}

// Apply a completion decision to the buffer + terminal.
void applyCompletion(const CompletionResult& cr, std::string& buf, std::ostream& term,
                     const std::string& prompt) {
  if (cr.replaced) { buf = cr.replacement; redraw(term, prompt, buf); return; }
  if (cr.candidates.empty()) { term << "\a"; term.flush(); return; }
  term << "\r\n";
  std::size_t shown = 0;
  for (const auto& m : cr.candidates) {
    if (shown++ >= 24) { term << "  ... (" << (cr.candidates.size() - 24) << " more)"; break; }
    term << "  " << m;
  }
  term << "\r\n";
  redraw(term, prompt, buf);
}

}  // namespace

bool readInteractiveLine(const CompletionModel& model, const std::string& prompt,
                         std::string& out, std::ostream& term,
                         const std::string& initial) {
  termios oldt;
  if (tcgetattr(STDIN_FILENO, &oldt) != 0) return false;  // not a tty
  termios raw = oldt;
  raw.c_lflag &= ~(static_cast<tcflag_t>(ICANON | ECHO | ISIG));
  raw.c_cc[VMIN] = 1;
  raw.c_cc[VTIME] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);

  std::string buf = initial;  // pre-filled command (accept-to-confirm / error recovery)
  int tabCycles = 0;  // running TAB count for empty-line recommended-action cycling
  redraw(term, prompt, buf);
  bool eof = false;
  for (;;) {
    char ch;
    const ssize_t n = ::read(STDIN_FILENO, &ch, 1);
    if (n <= 0) { eof = buf.empty(); break; }
    if (ch == '\n' || ch == '\r') { term << "\r\n"; term.flush(); break; }
    if (ch == 4) { eof = buf.empty(); if (eof) break; continue; }  // Ctrl-D
    if (ch == 3) {  // Ctrl-C
      buf.clear(); tabCycles = 0; term << "^C\r\n"; redraw(term, prompt, buf); continue;
    }
    if (ch == 127 || ch == 8) {  // backspace
      if (!buf.empty()) buf.pop_back();
      tabCycles = 0; redraw(term, prompt, buf); continue;
    }
    if (ch == '\t') {
      applyCompletion(complete(buf, model, tabCycles), buf, term, prompt);
      ++tabCycles;
      continue;
    }
    if (ch == 27) {  // ESC: lone Esc (clear), or CSI sequence (arrows / Shift-TAB).
      char seq0;
      const ssize_t m0 = ::read(STDIN_FILENO, &seq0, 1);
      if (m0 <= 0) {  // lone Esc: clear line, reset cycling
        buf.clear(); tabCycles = 0; redraw(term, prompt, buf); continue;
      }
      char seq1;
      ::read(STDIN_FILENO, &seq1, 1);
      if (seq0 == '[' && seq1 == 'Z') {  // Shift-TAB: cycle backward
        --tabCycles;
        applyCompletion(complete(buf, model, tabCycles), buf, term, prompt);
      }
      continue;  // swallow other escape sequences (arrow keys etc.)
    }
    if (static_cast<unsigned char>(ch) >= 32) {
      buf.push_back(ch);
      tabCycles = 0;
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
