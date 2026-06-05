#pragma once
#include <ostream>
#include <string>
#include "engine/completion.h"

namespace monopoly::engine {

// Reads one line from the terminal (fd 0) in raw mode, with TAB completion driven by
// `model`: command verbs, context operands, @square names, and empty-line cycling of the
// previous command's recommended actions (Shift-TAB cycles back, Esc clears). Writes the
// prompt and echoes to `term`. Returns false on EOF (Ctrl-D on an empty line). Only call
// when stdin is an interactive TTY.
bool readInteractiveLine(const CompletionModel& model, const std::string& prompt,
                         std::string& out, std::ostream& term);

}  // namespace monopoly::engine
