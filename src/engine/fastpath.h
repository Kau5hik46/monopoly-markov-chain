#pragma once
#include <string>
#include <vector>

namespace monopoly::engine {

// One selectable follow-up action, derived from a CommandResult prompt.
struct MenuAction {
  std::string command;        // clean, executable command (may carry a <placeholder>)
  bool skippable = false;     // optional prompt ("… or skip" / "optional")
  bool moneyMoving = false;   // buy/sell/trade/tax/write/insure -> confirm, don't auto-run
  bool hasPlaceholder = false;  // contains '<…>' -> must be filled before running
};

// Build the accept menu from a command's raw prompts. Roll prompts are excluded (the
// dice shorthand is their fast path); each remaining prompt is cleaned (cleanPrompt) and
// classified.
std::vector<MenuAction> buildMenu(const std::vector<std::string>& prompts);

// Index of the Enter-default action: the first non-skippable (mandatory) action, or -1
// when every action is optional (Enter then skips / does nothing).
int defaultActionIndex(const std::vector<MenuAction>& menu);

// Outcome of interpreting one raw input line against the menu + current roller.
struct FastInput {
  enum class Kind { None, Run, Prefill } kind = Kind::None;
  std::string text;  // Run: command to execute; Prefill: buffer to load for editing
};

// Interpret a raw line:
//   - "d,d" / "d d" (1..6)         -> Run "roll P{currentRoller+1} = d,d"
//   - "" (blank)                   -> default action (Run/Prefill), or None if skip
//   - lone digit N in range        -> action N (Run if safe, Prefill if money/placeholder)
//   - anything else                -> None (caller parses the line normally)
// `currentRoller` is 0-based (nextRoller); pass -1 if unknown (disables dice shorthand).
FastInput interpret(const std::string& line, const std::vector<MenuAction>& menu,
                    int currentRoller);

}  // namespace monopoly::engine
