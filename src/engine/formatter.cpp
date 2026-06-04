#include "engine/formatter.h"

#include <sstream>
#include "domain/square.h"
#include "engine/amount.h"

namespace monopoly::engine {

std::string formatEffects(const CommandResult& result) {
  std::ostringstream os;
  for (const auto& e : result.effects) {
    os << (e.kind == EffectKind::Error ? "  ! " : "  - ") << e.text << "\n";
  }
  return os.str();
}

std::string formatStatePanel(const domain::GameState& gs) {
  std::ostringstream os;
  os << "+- state " << std::string(50, '-') << "\n";
  for (int id = 0; id < gs.numPlayers(); ++id) {
    const auto& p = gs.player(id);
    os << "| P" << (id + 1) << " " << p.name << "  @"
       << gs.board().at(p.position).name << " (" << p.position << ")"
       << (p.inJail ? " [JAIL]" : "") << "  cash " << formatMoney(p.cash) << "\n";
    // owned properties
    std::string owned;
    for (int pos = 0; pos < domain::kBoardSize; ++pos) {
      if (gs.ownerOf(pos) != id) continue;
      owned += " " + gs.board().at(pos).name;
      int h = gs.housesOn(pos);
      if (h == domain::kHotel) owned += "(H)";
      else if (h > 0) owned += "(" + std::to_string(h) + ")";
      if (gs.isMortgaged(pos)) owned += "[M]";
    }
    if (!owned.empty()) os << "|     owns:" << owned << "\n";
  }
  os << "| Free-Parking pot: " << gs.freeParkingPot() << " houses   "
     << "bank: " << gs.bank().housesAvailable << "h/" << gs.bank().hotelsAvailable
     << "H\n";
  os << "+" << std::string(58, '-') << "\n";
  return os.str();
}

}  // namespace monopoly::engine
