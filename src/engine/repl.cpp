#include "engine/repl.h"

#include <string>
#include "engine/formatter.h"
#include "engine/parser.h"

namespace monopoly::engine {

namespace {
bool askYesNo(std::istream& in, std::ostream& out, const std::string& prompt,
              bool dflt) {
  out << prompt << (dflt ? " [Y/n] " : " [y/N] ");
  out.flush();
  std::string line;
  if (!std::getline(in, line)) return dflt;
  for (char c : line) {
    if (c == 'y' || c == 'Y') return true;
    if (c == 'n' || c == 'N') return false;
  }
  return dflt;
}
}  // namespace

rules::RuleConfig runRulesWizard(std::istream& in, std::ostream& out) {
  out << "=== House rules setup ===\n";
  rules::RuleConfig c;
  c.muggingEnabled = askYesNo(in, out, "Enable mugging?", true);
  c.airportTravelEnabled = askYesNo(in, out, "Enable airport travel?", true);
  c.freeParkingPotEnabled = askYesNo(in, out, "Enable free-parking house pot?", true);
  out << "Rules: mugging=" << (c.muggingEnabled ? "on" : "off")
      << " airport=" << (c.airportTravelEnabled ? "on" : "off")
      << " free-parking-pot=" << (c.freeParkingPotEnabled ? "on" : "off") << "\n\n";
  return c;
}

Repl::Repl(const domain::Board& board, const domain::Decks& decks,
           const rules::RuleConfig& rules, std::ostream& out)
    : board_(board), rules_(rules), gs_(board_), names_(board_),
      exec_(gs_, decks, rules_), out_(out) {}

void Repl::render(const std::string& line, const CommandResult& result,
                  CommandKind kind) {
  out_ << "> " << line << "\n";
  out_ << formatEffects(result);
  const bool showPanel = result.ok && gs_.numPlayers() > 0 &&
                         kind != CommandKind::Query && kind != CommandKind::Help;
  if (showPanel) {
    out_ << formatStatePanel(gs_);
    const int n = gs_.numPlayers();
    const int next = (exec_.lastMover() >= 0) ? (exec_.lastMover() + 1) % n : 0;
    out_ << exec_.queries().advisory(next);
  }
  out_ << "\n";
  out_.flush();
}

void Repl::run(std::istream& in) {
  out_ << "Monopoly Markov Advisor — London edition.  Type 'help' or 'quit'.\n\n";
  std::string line;
  while (true) {
    out_ << "monopoly> ";
    out_.flush();
    if (!std::getline(in, line)) break;
    Command cmd = parseLine(line, names_);
    if (cmd.kind == CommandKind::None) continue;
    if (cmd.kind == CommandKind::Quit) {
      out_ << "bye\n";
      break;
    }
    CommandResult result = exec_.execute(cmd);
    render(line, result, cmd.kind);
  }
}

}  // namespace monopoly::engine
