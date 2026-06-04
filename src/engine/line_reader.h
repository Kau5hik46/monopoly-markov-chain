#pragma once
#include <ostream>
#include <string>
#include "engine/name_table.h"

namespace monopoly::engine {

// Reads one line from the terminal (fd 0) in raw mode, with TAB completion of square
// names typed after '@'. Writes the prompt and echoes to `term`. Returns false on EOF
// (Ctrl-D on an empty line). Only call when stdin is an interactive TTY.
bool readInteractiveLine(const NameTable& names, const std::string& prompt,
                         std::string& out, std::ostream& term);

}  // namespace monopoly::engine
