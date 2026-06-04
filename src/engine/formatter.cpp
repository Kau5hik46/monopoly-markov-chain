#include "engine/formatter.h"

#include <sstream>
#include <string>
#include "domain/square.h"
#include "engine/amount.h"

namespace monopoly::engine {

namespace {

struct Glyph { const char* g; const char* label; };

Glyph glyphFor(EffectKind k) {
  switch (k) {
    case EffectKind::Move: return {">", "Move"};
    case EffectKind::PassGo: return {"*", "PassGo"};
    case EffectKind::Buy: return {"$", "Buy"};
    case EffectKind::Sell: return {"~", "Sell"};
    case EffectKind::RentPaid: return {"L", "RentPaid"};
    case EffectKind::CashTransfer: return {"L", "Cash"};
    case EffectKind::Mugging: return {"!", "Mugging"};
    case EffectKind::SentToJail: return {"x", "ToJail"};
    case EffectKind::SentToHospital: return {"x", "Hospital"};
    case EffectKind::TaxToPot: return {"%", "TaxToPot"};
    case EffectKind::PotClaimed: return {"%", "Claim"};
    case EffectKind::HouseBuilt: return {"^", "Build"};
    case EffectKind::Mortgage: return {"~", "Mortgage"};
    case EffectKind::AirportTravel: return {"@", "Airport"};
    case EffectKind::Bankrupt: return {"x", "Bankrupt"};
    case EffectKind::JailEnter: return {"J", "Jail"};
    case EffectKind::JailLeave: return {"J", "Jail"};
    case EffectKind::Double: return {"+", "Double"};
    case EffectKind::Query: return {" ", ""};
    case EffectKind::Error: return {"!", "Error"};
    case EffectKind::Info: default: return {"+", "Info"};
  }
}

std::string colorize(EffectKind k, const std::string& s, const Palette& p) {
  switch (k) {
    case EffectKind::Error: return p.red(s);
    case EffectKind::SentToJail:
    case EffectKind::SentToHospital: return p.red(s);
    case EffectKind::PassGo:
    case EffectKind::Buy: return p.green(s);
    case EffectKind::RentPaid:
    case EffectKind::TaxToPot: return p.yellow(s);
    default: return s;
  }
}

std::string groupShort(domain::ColorGroup g) {
  using G = domain::ColorGroup;
  switch (g) {
    case G::Brown: return "BROWN";
    case G::LightBlue: return "L.BLUE";
    case G::Pink: return "PINK";
    case G::Orange: return "ORANGE";
    case G::Red: return "RED";
    case G::Yellow: return "YELLOW";
    case G::Green: return "GREEN";
    case G::DarkBlue: return "DK.BLUE";
    case G::Station: return "STATION";
    case G::Utility: return "UTILITY";
    default: return "--";
  }
}

std::string devOf(const domain::GameState& gs, int pos) {
  if (!domain::isPurchasable(gs.board().at(pos).type)) return "--";
  if (gs.ownerOf(pos) == domain::kUnowned) return "--";
  if (gs.isMortgaged(pos)) return "MORTG";
  int h = gs.housesOn(pos);
  if (h == domain::kHotel) return "HOTEL";
  if (h > 0) return std::to_string(h) + "h";
  return "---";
}

std::string devTag(const domain::GameState& gs, int pos) {
  if (gs.isMortgaged(pos)) return "[m]";
  int h = gs.housesOn(pos);
  if (h == domain::kHotel) return "[H]";
  if (h > 0) return "[" + std::to_string(h) + "]";
  return "";
}

}  // namespace

std::string formatEcho(const std::string& line, bool ok, const Palette& pal) {
  // No leading "> " — the REPL prompt already supplies it. Used for piped logs.
  return (ok ? pal.bold(line) : pal.red("! " + line)) + "\n";
}

std::string formatEffects(const CommandResult& result, const Palette& pal) {
  std::ostringstream os;
  // Pure query/help output (only pre-rendered blocks) prints without an EVENTS header.
  bool onlyBlocks = !result.effects.empty();
  for (const auto& e : result.effects)
    if (e.kind != EffectKind::Query) onlyBlocks = false;
  if (onlyBlocks) {
    for (const auto& e : result.effects) os << e.text << "\n";
    return os.str();
  }
  os << sectionHeader("EVENTS", "", pal) << "\n";
  if (result.effects.empty()) {
    os << "  " << pal.dim("(no side effects)") << "\n";
    return os.str();
  }
  for (const auto& e : result.effects) {
    if (e.kind == EffectKind::Query) {  // pre-rendered multi-line block
      os << e.text << "\n";
      continue;
    }
    Glyph gl = glyphFor(e.kind);
    std::string head = std::string("[") + gl.g + "] " + padRight(gl.label, 9);
    os << "  " << colorize(e.kind, head, pal) << " " << e.text << "\n";
  }
  return os.str();
}

std::string formatStatePanel(const domain::GameState& gs, int actingPlayer,
                             const Palette& pal) {
  std::ostringstream os;
  os << sectionHeader("STATE", "", pal) << "\n";
  os << " " << padRight("PLAYER", 7) << padRight("POSITION", 26)
     << padRight("CASH", 12) << "JL  PROPERTIES\n";
  for (int id = 0; id < gs.numPlayers(); ++id) {
    const auto& p = gs.player(id);
    std::string tag = "P" + std::to_string(id + 1) + (id == actingPlayer ? " *" : "");
    std::string pos = truncate(gs.board().at(p.position).name + " (" +
                                   std::to_string(p.position) + ")", 25);
    std::string props;
    for (int sq = 0; sq < domain::kBoardSize; ++sq) {
      if (gs.ownerOf(sq) != id) continue;
      if (!props.empty()) props += " ";
      props += gs.board().at(sq).name.substr(0, 10) + devTag(gs, sq);
    }
    if (props.empty()) props = pal.dim("(none)");
    else props = truncate(props, 34);
    std::string jl = p.inJail ? pal.red("J ") : pal.dim("- ");
    os << " " << padRight(tag, 7) << padRight(pos, 26)
       << padRight(formatMoney(static_cast<double>(p.cash)), 12) << jl << " "
       << props << "\n";
  }
  os << " " << pal.dim("Free-Parking pot: ") << gs.freeParkingPot()
     << " houses     " << pal.dim("Bank: ") << gs.bank().housesAvailable << "h / "
     << gs.bank().hotelsAvailable << "H\n";
  return os.str();
}

std::string formatBoard(const domain::GameState& gs, const Palette& pal) {
  std::ostringstream os;
  os << sectionHeader("BOARD", "40 squares", pal) << "\n";
  os << " " << padLeft("IDX", 3) << "  " << padRight("NAME", 24)
     << padRight("GROUP", 9) << padRight("OWNER", 7) << padRight("DEV", 7)
     << "HERE\n";
  for (int pos = 0; pos < domain::kBoardSize; ++pos) {
    const auto& sq = gs.board().at(pos);
    std::string owner = "--";
    if (gs.ownerOf(pos) != domain::kUnowned)
      owner = "P" + std::to_string(gs.ownerOf(pos) + 1);
    std::string here;
    for (int id = 0; id < gs.numPlayers(); ++id) {
      if (gs.player(id).position != pos) continue;
      if (!here.empty()) here += " ";
      here += "P" + std::to_string(id + 1) + (gs.player(id).inJail ? "*" : "");
    }
    if (here.empty()) here = pal.dim("--");
    std::string dev = padRight(devOf(gs, pos), 7);  // pad first, color after
    if (devOf(gs, pos) == "HOTEL") dev = pal.cyan(dev);
    else if (devOf(gs, pos) == "MORTG") dev = pal.yellow(dev);
    os << " " << padLeft(std::to_string(pos), 3) << "  "
       << padRight(truncate(sq.name, 24), 24) << padRight(groupShort(sq.group), 9)
       << padRight(owner, 7) << dev << here << "\n";
  }
  return os.str();
}

}  // namespace monopoly::engine
