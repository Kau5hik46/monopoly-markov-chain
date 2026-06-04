#include "engine/parser.h"

#include <cctype>
#include <optional>
#include <vector>
#include "engine/amount.h"
#include "engine/lexer.h"

namespace monopoly::engine {

namespace {

Command invalid(const std::string& msg) {
  Command c;
  c.kind = CommandKind::Invalid;
  c.error = msg;
  return c;
}

// "P1" -> 0, "p3" -> 2. nullopt otherwise.
std::optional<int> parsePlayer(const std::string& t) {
  if (t.size() < 2 || (t[0] != 'P' && t[0] != 'p')) return std::nullopt;
  for (std::size_t i = 1; i < t.size(); ++i)
    if (!std::isdigit(static_cast<unsigned char>(t[i]))) return std::nullopt;
  int n = std::stoi(t.substr(1));
  if (n < 1) return std::nullopt;
  return n - 1;
}

std::optional<int> parseInt(const std::string& t) {
  if (t.empty()) return std::nullopt;
  for (char c : t)
    if (!std::isdigit(static_cast<unsigned char>(c))) return std::nullopt;
  return std::stoi(t);
}

// Cursor over tokens with small helpers.
struct Cur {
  const std::vector<std::string>& t;
  std::size_t i = 0;
  bool done() const { return i >= t.size(); }
  const std::string& peek() const { return t[i]; }
  std::string take() { return t[i++]; }
  bool eat(const std::string& s) {
    if (!done() && t[i] == s) { ++i; return true; }
    return false;
  }
};

}  // namespace

Command parseLine(const std::string& line, const NameTable& names) {
  auto toks = lex(line);
  if (toks.empty()) { Command c; c.kind = CommandKind::None; return c; }
  Cur cur{toks};
  const std::string verb = cur.take();

  auto needPlayer = [&](int& out) -> bool {
    if (cur.done()) return false;
    auto p = parsePlayer(cur.take());
    if (!p) return false;
    out = *p;
    return true;
  };
  auto needSquare = [&](int& out) -> bool {
    if (!cur.eat("@")) return false;
    if (cur.done()) return false;
    auto pos = names.resolve(cur.take());
    if (!pos) return false;
    out = *pos;
    return true;
  };

  Command c;

  if (verb == "init") {
    auto n = cur.done() ? std::nullopt : parseInt(cur.take());
    if (!n || *n < 1) return invalid("init expects a player count >= 1");
    c.kind = CommandKind::Init; c.count = *n; return c;
  }
  if (verb == "roll") {
    if (!needPlayer(c.player)) return invalid("roll expects a player");
    if (!cur.eat("=")) return invalid("roll expects '='");
    auto d1 = cur.done() ? std::nullopt : parseInt(cur.take());
    if (!cur.eat(",")) return invalid("roll expects 'd1,d2'");
    auto d2 = cur.done() ? std::nullopt : parseInt(cur.take());
    if (!d1 || !d2 || *d1 < 1 || *d1 > 6 || *d2 < 1 || *d2 > 6)
      return invalid("roll dice must be 1..6");
    c.kind = CommandKind::Roll; c.die1 = *d1; c.die2 = *d2; return c;
  }
  if (verb == "buy") {
    if (!needPlayer(c.player)) return invalid("buy expects a player");
    if (!needSquare(c.posA)) return invalid("buy expects @square");
    if (cur.eat("=")) {
      auto a = cur.done() ? std::nullopt : parseAmount(cur.take());
      if (!a) return invalid("buy: bad amount"); c.amount = *a; c.hasAmount = true;
    }
    c.kind = CommandKind::Buy; return c;
  }
  if (verb == "sell") {
    if (!needPlayer(c.player)) return invalid("sell expects a seller");
    if (!cur.eat("->")) return invalid("sell expects '->'");
    if (!needPlayer(c.player2)) return invalid("sell expects a buyer");
    if (!needSquare(c.posA)) return invalid("sell expects @square");
    if (cur.eat("=")) {
      auto a = cur.done() ? std::nullopt : parseAmount(cur.take());
      if (!a) return invalid("sell: bad amount"); c.amount = *a; c.hasAmount = true;
    }
    c.kind = CommandKind::Sell; return c;
  }
  if (verb == "rent") {
    if (!needPlayer(c.player)) return invalid("rent expects a payer");
    if (!cur.eat("->")) return invalid("rent expects '->'");
    if (!needPlayer(c.player2)) return invalid("rent expects a payee");
    if (!needSquare(c.posA)) return invalid("rent expects @square");
    if (cur.eat("=")) {
      auto a = cur.done() ? std::nullopt : parseAmount(cur.take());
      if (!a) return invalid("rent: bad amount"); c.amount = *a; c.hasAmount = true;
    }
    c.kind = CommandKind::Rent; return c;
  }
  if (verb == "build") {
    if (!needPlayer(c.player)) return invalid("build expects a player");
    if (!needSquare(c.posA)) return invalid("build expects @square");
    if (cur.eat("+")) c.sign = true;
    else if (cur.eat("-")) c.sign = false;
    else return invalid("build expects '+' or '-'");
    auto n = cur.done() ? std::nullopt : parseInt(cur.take());
    if (!n || *n < 1) return invalid("build expects a count");
    c.kind = CommandKind::Build; c.count = *n; return c;
  }
  if (verb == "mortgage" || verb == "unmortgage") {
    if (!needPlayer(c.player)) return invalid("mortgage expects a player");
    if (!needSquare(c.posA)) return invalid("mortgage expects @square");
    c.kind = (verb == "mortgage") ? CommandKind::Mortgage : CommandKind::Unmortgage;
    return c;
  }
  if (verb == "tax") {
    if (!needPlayer(c.player)) return invalid("tax expects a player");
    if (!cur.eat("=")) return invalid("tax expects '='");
    auto a = cur.done() ? std::nullopt : parseAmount(cur.take());
    if (!a) return invalid("tax: bad amount");
    if (!cur.eat("@")) return invalid("tax expects @INCOME or @SUPER");
    std::string kind = cur.done() ? "" : cur.take();
    if (kind != "INCOME" && kind != "SUPER") return invalid("tax kind must be INCOME/SUPER");
    c.kind = CommandKind::Tax; c.amount = *a; c.hasAmount = true; c.name = kind; return c;
  }
  if (verb == "jail") {
    if (!needPlayer(c.player)) return invalid("jail expects a player");
    if (cur.eat("+")) c.sign = true;
    else if (cur.eat("-")) c.sign = false;
    else return invalid("jail expects '+' or '-'");
    c.kind = CommandKind::Jail; return c;
  }
  if (verb == "mug") {
    if (!needPlayer(c.player)) return invalid("mug expects a mugger");
    if (!cur.eat("vs")) return invalid("mug expects 'vs'");
    if (!needPlayer(c.player2)) return invalid("mug expects a muggee");
    if (!cur.eat("=")) return invalid("mug expects '='");
    auto a = cur.done() ? std::nullopt : parseInt(cur.take());
    if (!cur.eat(":")) return invalid("mug expects 'muggerSum:muggeeSum'");
    auto b = cur.done() ? std::nullopt : parseInt(cur.take());
    if (!a || !b) return invalid("mug expects integer contest totals");
    c.kind = CommandKind::Mug; c.die1 = *a; c.die3 = *b; return c;
  }
  if (verb == "airport") {
    if (!needPlayer(c.player)) return invalid("airport expects a player");
    if (!needSquare(c.posA)) return invalid("airport expects @from");
    if (!cur.eat("->")) return invalid("airport expects '->'");
    if (!needSquare(c.posB)) return invalid("airport expects @to");
    c.kind = CommandKind::Airport; return c;
  }
  if (verb == "claim") {
    if (!needPlayer(c.player)) return invalid("claim expects a player");
    c.kind = CommandKind::Claim; return c;
  }
  if (verb == "cash") {
    if (!needPlayer(c.player)) return invalid("cash expects a player");
    if (cur.eat("+=")) c.sign = true;
    else if (cur.eat("-=")) c.sign = false;
    else return invalid("cash expects '+=' or '-='");
    auto a = cur.done() ? std::nullopt : parseAmount(cur.take());
    if (!a) return invalid("cash: bad amount");
    c.kind = CommandKind::Cash; c.amount = *a; c.hasAmount = true; return c;
  }
  if (verb == "query") {
    if (cur.done()) return invalid("query expects a subcommand");
    std::string sub = cur.take();
    c.kind = CommandKind::Query;
    if (sub == "risk") { c.query = QueryKind::Risk; needPlayer(c.player); return c; }
    if (sub == "options") { c.query = QueryKind::Options; needPlayer(c.player); return c; }
    if (sub == "dist") {
      c.query = QueryKind::Dist; if (!needPlayer(c.player)) return invalid("query dist expects a player");
      if (cur.eat("^")) { auto n = cur.done() ? std::nullopt : parseInt(cur.take()); c.count = n ? *n : 1; }
      else c.count = 1;
      return c;
    }
    if (sub == "stationary") { c.query = QueryKind::Stationary; return c; }
    if (sub == "board") { c.query = QueryKind::Board; return c; }
    if (sub == "value") { c.query = QueryKind::Value; if (!needSquare(c.posA)) return invalid("query value expects @square"); return c; }
    if (sub == "state") { c.query = QueryKind::State; return c; }
    return invalid("unknown query: " + sub);
  }
  if (verb == "rules") {
    if (cur.done()) return invalid("rules expects a name");
    c.name = cur.take();
    if (cur.eat("on")) c.flag = true;
    else if (cur.eat("off")) c.flag = false;
    else return invalid("rules expects 'on' or 'off'");
    c.kind = CommandKind::Rules; return c;
  }
  if (verb == "undo") { c.kind = CommandKind::Undo; return c; }
  if (verb == "help") { c.kind = CommandKind::Help; return c; }
  if (verb == "quit" || verb == "exit") { c.kind = CommandKind::Quit; return c; }

  return invalid("unknown command: " + verb);
}

}  // namespace monopoly::engine
