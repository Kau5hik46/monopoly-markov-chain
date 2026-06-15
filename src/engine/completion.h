#pragma once
#include <string>
#include <vector>
#include "engine/name_table.h"

namespace monopoly::engine {

// Inputs the completion engine needs: square names, live player count, the previous
// command's recommended actions (clean command forms), and the static token tables.
struct CompletionModel {
  const NameTable* names = nullptr;
  int numPlayers = 0;
  std::vector<std::string> recommendedActions;  // clean command forms
  std::vector<std::string> verbs;
  std::vector<std::string> querySubs;
  std::vector<std::string> keywords;
};

// Result of one completion request: either replace the buffer, or list candidates.
struct CompletionResult {
  bool replaced = false;
  std::string replacement;
  std::vector<std::string> candidates;
};

// Strip a recommended-action prompt to its executable command form (cut at the first
// "  (" or em-dash / "--" tail, then trim trailing spaces).
std::string cleanPrompt(const std::string& prompt);

// Build a model with the static verb/querySub/keyword tables filled in.
// (Verb/keyword tables mirror engine/parser.cpp — keep both in sync.)
CompletionModel makeCompletionModel(const NameTable& names, int numPlayers,
                                    std::vector<std::string> recommendedActions);

// Pure completion decision. `cycleIndex` selects the recommended action on an empty line
// (callers pass the running TAB count; negative values wrap for Shift-TAB).
CompletionResult complete(const std::string& buf, const CompletionModel& model,
                          int cycleIndex);

}  // namespace monopoly::engine
