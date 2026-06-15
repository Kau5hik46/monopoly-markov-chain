#include "engine/fastpath.h"

#include <cctype>
#include "engine/completion.h"  // cleanPrompt

namespace monopoly::engine {

namespace {

std::string trim(const std::string& s) {
  std::size_t a = 0, b = s.size();
  while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
  while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
  return s.substr(a, b - a);
}

std::string firstToken(const std::string& s) {
  const std::size_t sp = s.find(' ');
  return sp == std::string::npos ? s : s.substr(0, sp);
}

bool isMoneyVerb(const std::string& verb) {
  return verb == "buy" || verb == "sell" || verb == "trade" || verb == "tax" ||
         verb == "write" || verb == "insure";
}

bool allDigits(const std::string& s) {
  if (s.empty()) return false;
  for (char c : s)
    if (!std::isdigit(static_cast<unsigned char>(c))) return false;
  return true;
}

// Parse "d,d" or "d d" with each die 1..6. Returns false if not a dice shorthand.
bool parseDice(const std::string& line, int& d1, int& d2) {
  std::string a, b;
  bool sawSep = false;
  for (char c : line) {
    if (c == ',' || c == ' ') { sawSep = true; continue; }
    (sawSep ? b : a).push_back(c);
  }
  if (!sawSep || !allDigits(a) || !allDigits(b) || a.size() != 1 || b.size() != 1)
    return false;
  d1 = a[0] - '0';
  d2 = b[0] - '0';
  return d1 >= 1 && d1 <= 6 && d2 >= 1 && d2 <= 6;
}

// Classify a chosen action into Run (execute now) or Prefill (load for confirmation).
FastInput toFastInput(const MenuAction& a) {
  FastInput fi;
  if (a.hasPlaceholder) {
    fi.kind = FastInput::Kind::Prefill;
    const std::size_t lt = a.command.find('<');
    fi.text = trim(a.command.substr(0, lt));  // up to the first placeholder
    fi.text += ' ';                            // leave a trailing space to type after
    return fi;
  }
  if (a.moneyMoving) { fi.kind = FastInput::Kind::Prefill; fi.text = a.command; return fi; }
  fi.kind = FastInput::Kind::Run;
  fi.text = a.command;
  return fi;
}

}  // namespace

std::vector<MenuAction> buildMenu(const std::vector<std::string>& prompts) {
  std::vector<MenuAction> menu;
  for (const auto& p : prompts) {
    const std::string cmd = cleanPrompt(p);
    if (cmd.empty() || firstToken(cmd) == "roll") continue;  // rolls use dice shorthand
    MenuAction a;
    a.command = cmd;
    a.skippable = p.find("skip") != std::string::npos ||
                  p.find("optional") != std::string::npos;
    a.moneyMoving = isMoneyVerb(firstToken(cmd));
    a.hasPlaceholder = cmd.find('<') != std::string::npos;
    menu.push_back(std::move(a));
  }
  return menu;
}

int defaultActionIndex(const std::vector<MenuAction>& menu) {
  for (std::size_t i = 0; i < menu.size(); ++i)
    if (!menu[i].skippable) return static_cast<int>(i);
  return -1;  // all optional -> Enter skips
}

FastInput interpret(const std::string& rawLine, const std::vector<MenuAction>& menu,
                    int currentRoller) {
  const std::string line = trim(rawLine);

  // Dice shorthand: roll for the current player.
  int d1 = 0, d2 = 0;
  if (currentRoller >= 0 && parseDice(line, d1, d2)) {
    FastInput fi;
    fi.kind = FastInput::Kind::Run;
    fi.text = "roll P" + std::to_string(currentRoller + 1) + " = " +
              std::to_string(d1) + "," + std::to_string(d2);
    return fi;
  }

  // Blank -> default action (or skip).
  if (line.empty()) {
    const int di = defaultActionIndex(menu);
    if (di < 0) return {};  // nothing mandatory -> skip
    return toFastInput(menu[static_cast<std::size_t>(di)]);
  }

  // Lone digit -> accept action N (1-based).
  if (allDigits(line) && line.size() == 1) {
    const int n = line[0] - '0';
    if (n >= 1 && n <= static_cast<int>(menu.size()))
      return toFastInput(menu[static_cast<std::size_t>(n - 1)]);
    return {};  // out of range -> let the parser handle/err
  }

  return {};  // normal command line
}

}  // namespace monopoly::engine
