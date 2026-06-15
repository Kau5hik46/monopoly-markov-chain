#include "engine/repl.h"

#include <fstream>
#include <string>
#include "engine/completion.h"
#include "engine/fastpath.h"
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

rules::RuleConfig runRulesWizard(std::istream& in, std::ostream& out,
                                 rules::RuleConfig base) {
  out << "=== House rules setup ===\n";
  rules::RuleConfig c = base;  // keep board-dependent economy; toggle only the booleans
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
  std::vector<std::string> lastActions;  // cleaned recommended actions for TAB completion
  std::vector<std::string> lastPrompts;  // raw prompts, for the fast-accept menu
  std::string pendingFill;               // pre-loaded buffer (accept / error recovery)
  while (true) {
    auto menu = buildMenu(lastPrompts);
    bool got;
    if (pal_.on) {  // interactive TTY: raw-mode reader with command-aware autocomplete
      CompletionModel model =
          makeCompletionModel(names_, gs_.numPlayers(), lastActions);
      got = readInteractiveLine(model, "monopoly> ", line, out_, pendingFill);
      pendingFill.clear();
    } else {
      out_ << "monopoly> ";
      out_.flush();
      got = static_cast<bool>(std::getline(in, line));
    }
    if (!got) break;

    // Fast-input: dice shorthand, Enter-default, and accept-by-number. Prefill loads the
    // buffer for a confirming Enter (interactive only); Run/None feed the parser directly.
    const std::string original = line;
    FastInput fi = interpret(line, menu, exec_.nextRoller());
    if (fi.kind == FastInput::Kind::Prefill && pal_.on) {
      pendingFill = fi.text;
      continue;
    }
    if (fi.kind != FastInput::Kind::None) line = fi.text;

    Command cmd = parseLine(line, names_);
    if (cmd.kind == CommandKind::None) continue;
    if (cmd.kind == CommandKind::Quit) {
      out_ << "bye\n";
      break;
    }
    if (cmd.kind == CommandKind::Log) {  // session journal — not a game command
      const int n = cmd.count;
      const int total = static_cast<int>(journal_.size());
      const int start = (n > 0 && n < total) ? total - n : 0;
      for (int i = start; i < total; ++i) {
        const auto& e = journal_[static_cast<std::size_t>(i)];
        out_ << "[" << e.seq << "] " << e.command << "\n";
        for (const auto& t : e.effects) out_ << "    " << t << "\n";
      }
      out_.flush();
      continue;
    }
    CommandResult result = exec_.execute(cmd);
    render(line, result, cmd.kind);

    // Journal the executed command + its effect texts; append to the session log file.
    JournalEntry je;
    je.seq = ++seq_;
    je.command = line;
    for (const auto& e : result.effects) je.effects.push_back(e.text);
    journal_.push_back(je);
    {
      std::ofstream f(logPath_, std::ios::app);
      if (f) {
        f << "[" << je.seq << "] " << je.command << "\n";
        for (const auto& t : je.effects) f << "    " << t << "\n";
      }
    }

    // Error-line preservation (G1): reload a rejected line so the operator edits it.
    if (pal_.on && !result.ok) pendingFill = original;

    lastPrompts = result.prompts;
    lastActions.clear();
    for (const auto& p : result.prompts) lastActions.push_back(cleanPrompt(p));

    // Fast-accept hint: numbered menu with the Enter default marked.
    if (pal_.on && !result.prompts.empty()) {
      auto m = buildMenu(result.prompts);
      if (!m.empty()) {
        const int def = defaultActionIndex(m);
        out_ << pal_.dim("fast:") << " "
             << (def < 0 ? pal_.dim("[\xE2\x86\xB5 skip]") : "");
        for (std::size_t i = 0; i < m.size(); ++i) {
          const bool isDef = static_cast<int>(i) == def;
          const std::string tag =
              isDef ? "[\xE2\x86\xB5] " : "[" + std::to_string(i + 1) + "] ";
          out_ << " " << (isDef ? pal_.bold(tag) : pal_.dim(tag)) << m[i].command;
        }
        out_ << "\n";
        out_.flush();
      }
    }
  }
}

}  // namespace monopoly::engine
