#include "engine/repl.h"

#include <string>
#include "engine/formatter.h"
#include "engine/line_reader.h"
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
  c.landOnGoDoubles = askYesNo(in, out, "Double salary for landing exactly on GO?", true);
  out << "Rules: mugging=" << (c.muggingEnabled ? "on" : "off")
      << " airport=" << (c.airportTravelEnabled ? "on" : "off")
      << " free-parking-pot=" << (c.freeParkingPotEnabled ? "on" : "off")
      << " 2x-on-GO=" << (c.landOnGoDoubles ? "on" : "off")
      << "  (pass-GO salary " << c.passGoBonus << ")\n\n";
  return c;
}

Repl::Repl(const domain::Board& board, const domain::Decks& decks,
           const rules::RuleConfig& rules, std::ostream& out, const Palette& pal)
    : board_(board), rules_(rules), pal_(pal), gs_(board_), names_(board_),
      exec_(gs_, decks, rules_, pal), out_(out) {}

void Repl::render(const std::string& line, const CommandResult& result,
                  CommandKind kind) {
  // Interactively the terminal already shows the typed line; only echo when piped.
  if (!pal_.on) out_ << formatEcho(line, result.ok, pal_);
  out_ << formatEffects(result, pal_);
  out_ << formatPrompts(result, pal_);
  const bool showPanel = result.ok && gs_.numPlayers() > 0 &&
                         kind != CommandKind::Query && kind != CommandKind::Help;
  if (showPanel) {
    out_ << formatStatePanel(gs_, exec_.lastMover(), pal_);
    out_ << "\n" << exec_.queries().advisory(exec_.nextRoller());
  }
  out_ << "\n";
  out_.flush();
}

void Repl::run(std::istream& in) {
  out_ << "Monopoly Markov Advisor — London edition.  Type 'help' or 'quit'.\n\n";
  std::string line;
  while (true) {
    bool got;
    if (pal_.on) {  // interactive TTY: raw-mode reader with @-autocomplete
      got = readInteractiveLine(names_, "monopoly> ", line, out_);
    } else {
      out_ << "monopoly> ";
      out_.flush();
      got = static_cast<bool>(std::getline(in, line));
    }
    if (!got) break;
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
