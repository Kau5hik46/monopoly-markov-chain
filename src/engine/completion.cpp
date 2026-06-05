#include "engine/completion.h"

#include <algorithm>

namespace monopoly::engine {

namespace {

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

std::vector<std::string> filterByPrefix(const std::vector<std::string>& pool,
                                        const std::string& pfx) {
  std::vector<std::string> out;
  for (const auto& s : pool)
    if (s.size() >= pfx.size() && s.compare(0, pfx.size(), pfx) == 0) out.push_back(s);
  return out;
}

// Fill `r` from `pool` against the current token; `head` is everything before the token.
CompletionResult fillFrom(const std::vector<std::string>& pool, const std::string& token,
                          const std::string& head) {
  CompletionResult r;
  auto matches = filterByPrefix(pool, token);
  if (matches.size() == 1) { r.replaced = true; r.replacement = head + matches[0]; return r; }
  if (!matches.empty()) {
    const std::string lcp = commonPrefix(matches);
    if (lcp.size() > token.size()) { r.replaced = true; r.replacement = head + lcp; }
    r.candidates = matches;
  }
  return r;
}

std::string rstrip(std::string s) {
  while (!s.empty() && s.back() == ' ') s.pop_back();
  return s;
}

}  // namespace

std::string cleanPrompt(const std::string& prompt) {
  std::size_t cut = std::string::npos;
  for (const char* marker : {"  (", " \xE2\x80\x94", "  \xE2\x80\x94", " --"}) {
    auto idx = prompt.find(marker);
    if (idx != std::string::npos) cut = std::min(cut, idx);
  }
  return rstrip(cut == std::string::npos ? prompt : prompt.substr(0, cut));
}

CompletionModel makeCompletionModel(const NameTable& names, int numPlayers,
                                    std::vector<std::string> recommendedActions) {
  CompletionModel m;
  m.names = &names;
  m.numPlayers = numPlayers;
  m.recommendedActions = std::move(recommendedActions);
  m.verbs = {"init", "roll", "buy", "sell", "rent", "build", "mortgage", "unmortgage",
             "tax", "jail", "mug", "airport", "claim", "card", "trade", "cash", "insure",
             "write", "settle", "log", "query", "rules", "save", "load", "undo", "help",
             "quit"};
  m.querySubs = {"risk", "options", "dist", "stationary", "value", "state", "board",
                 "chain", "forecast", "simulate", "ledger"};
  m.keywords = {"strike", "premium", "call", "put", "vs", "on", "off", "all", "income",
                "landers", "INCOME", "SUPER", "GO", "JAIL", "BACK3", "STATION", "UTILITY"};
  return m;
}

CompletionResult complete(const std::string& buf, const CompletionModel& model,
                          int cycleIndex) {
  CompletionResult r;

  // 1. Active @square token.
  const auto at = buf.rfind('@');
  const auto sp = buf.rfind(' ');
  const bool atActive = at != std::string::npos && (sp == std::string::npos || at > sp);
  if (atActive && model.names) {
    const std::string prefix = buf.substr(at + 1);
    auto matches = model.names->completions(prefix);
    if (matches.size() == 1) {
      r.replaced = true; r.replacement = buf.substr(0, at + 1) + matches[0]; return r;
    }
    if (!matches.empty()) {
      const std::string lcp = commonPrefix(matches);
      if (lcp.size() > prefix.size()) {
        r.replaced = true; r.replacement = buf.substr(0, at + 1) + lcp;
      }
      r.candidates = matches;
    }
    return r;
  }

  // 2. Empty buffer: cycle recommended actions, else list verbs.
  if (buf.empty()) {
    const int n = static_cast<int>(model.recommendedActions.size());
    if (n > 0) {
      const int i = ((cycleIndex % n) + n) % n;
      r.replaced = true;
      r.replacement = model.recommendedActions[static_cast<std::size_t>(i)];
      return r;
    }
    r.candidates = model.verbs;
    return r;
  }

  const std::string token = (sp == std::string::npos) ? buf : buf.substr(sp + 1);
  const std::string head = (sp == std::string::npos) ? "" : buf.substr(0, sp + 1);

  // 3. First token -> verbs.
  if (sp == std::string::npos) return fillFrom(model.verbs, token, head);

  // 4. After "query" first token -> query subcommands.
  const std::string first = buf.substr(0, buf.find(' '));
  if (first == "query") return fillFrom(model.querySubs, token, head);

  // 5. Otherwise players ∪ keywords.
  std::vector<std::string> pool;
  for (int i = 1; i <= model.numPlayers; ++i) pool.push_back("P" + std::to_string(i));
  pool.insert(pool.end(), model.keywords.begin(), model.keywords.end());
  return fillFrom(pool, token, head);
}

}  // namespace monopoly::engine
