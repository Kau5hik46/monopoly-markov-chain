#pragma once
#include <string>

namespace monopoly::engine {

enum class CommandKind {
  None, Init, Roll, Buy, Sell, Rent, Build, Mortgage, Unmortgage, Tax, Jail, Mug,
  Airport, Claim, Cash, Query, Rules, Undo, Help, Quit, Invalid
};

enum class QueryKind { Risk, Options, Dist, Stationary, Value, State };

// Parsed command: a tagged value (kind + operands). The executor evaluates it.
struct Command {
  CommandKind kind = CommandKind::None;
  std::string error;       // set when kind == Invalid

  int player = -1;         // primary player index (P1 -> 0)
  int player2 = -1;        // secondary player (transfers, mugging)
  int posA = -1;           // square position
  int posB = -1;           // second square (airport travel)
  long amount = 0;
  bool hasAmount = false;
  int die1 = 0, die2 = 0;  // roll dice, or mugger contest dice
  int die3 = 0, die4 = 0;  // muggee contest dice (mug)
  int count = 0;           // build delta / dist horizon / init N
  bool sign = true;        // +/- for build, jail, cash
  QueryKind query = QueryKind::State;
  std::string name;        // rules name, or tax type ("INCOME"/"SUPER")
  bool flag = false;       // rules on/off
};

}  // namespace monopoly::engine
