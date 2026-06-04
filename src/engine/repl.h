#pragma once
#include <istream>
#include <ostream>
#include "domain/board.h"
#include "domain/deck_factory.h"
#include "domain/game_state.h"
#include "engine/executor.h"
#include "engine/name_table.h"
#include "engine/style.h"
#include "rules/rule_config.h"

namespace monopoly::engine {

// Interactive setup asked at game start: prompts yes/no for each house rule and
// returns the chosen RuleConfig. On EOF/empty answers, keeps defaults.
rules::RuleConfig runRulesWizard(std::istream& in, std::ostream& out);

// The read-eval-print loop. After every command it prints the echoed command, the
// causal effect log, the full state panel, and the next roller's advisory.
class Repl {
 public:
  Repl(const domain::Board& board, const domain::Decks& decks,
       const rules::RuleConfig& rules, std::ostream& out,
       const Palette& pal = Palette{});

  void run(std::istream& in);

 private:
  void render(const std::string& line, const CommandResult& result,
              CommandKind kind);

  const domain::Board& board_;
  rules::RuleConfig rules_;
  Palette pal_;
  domain::GameState gs_;
  NameTable names_;
  Executor exec_;
  std::ostream& out_;
};

}  // namespace monopoly::engine
