#pragma once
#include <string>
#include "engine/command.h"
#include "engine/name_table.h"

namespace monopoly::engine {

// Parses one DSL line into a Command. On any error, returns a Command with
// kind == Invalid and a populated `error`. Square refs resolve via `names`.
Command parseLine(const std::string& line, const NameTable& names);

}  // namespace monopoly::engine
