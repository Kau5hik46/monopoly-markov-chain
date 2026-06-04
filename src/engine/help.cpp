#include "engine/help.h"

#include <sstream>

namespace monopoly::engine {

namespace {
std::string sec(const std::string& t, const Palette& p) { return p.bold(t); }
}  // namespace

std::string manPage(const Palette& pal) {
  std::ostringstream o;
  o << sec("NAME", pal) << "\n"
    << "    monopoly — Markov decision advisor for a live game of Monopoly\n\n";

  o << sec("SYNOPSIS", pal) << "\n"
    << "    <verb> <operands>\n"
    << "    players : P1, P2, ...            squares : @#<pos> or @<NAME_WITH_UNDERSCORES>\n"
    << "    amounts : int with K/M suffix    e.g. 500K, 2.4M, 15M\n\n";

  o << sec("DESCRIPTION", pal) << "\n"
    << "    Type real game events as they happen. After each command the advisor applies\n"
    << "    it, prints the resulting events and the full game state, and shows the next\n"
    << "    player's roll risk plus a fair insurance price for the upcoming roll.\n\n";

  o << sec("SETUP", pal) << "\n"
    << "    init N                      start a game with N players (P1..PN)\n\n";

  o << sec("MOVEMENT", pal) << "\n"
    << "    roll Pi = d1,d2             apply a dice roll (GO bonus, jail, doubles handled)\n"
    << "    jail Pi +|-                 send to / release from jail\n"
    << "    airport Pi @FROM -> @TO     travel between two airports you own (skips a turn)\n"
    << "    mug Pi vs Pj = a:b          resolve a mugging contest (Pi's total : Pj's total)\n"
    << "    card Pi : GO|JAIL|BACK3|STATION|UTILITY|@SQ   apply a drawn card's movement\n\n";

  o << sec("PROPERTY", pal) << "\n"
    << "    buy Pi @SQ [= amt]          buy a property (price defaults to face value)\n"
    << "    sell Pi -> Pj @SQ [= amt]   transfer a property for cash\n"
    << "    rent Pi -> Pj @SQ [= amt]   pay rent (auto-computed if amount omitted)\n"
    << "    build Pi @SQ +|- n          add / remove n houses (5 = hotel)\n"
    << "    mortgage|unmortgage Pi @SQ  toggle a mortgage\n"
    << "    trade Pi <-> Pj : @A 1M <-> @B   swap property+cash bundles between players\n\n";

  o << sec("MONEY", pal) << "\n"
    << "    tax Pi = amt @INCOME|SUPER  pay tax (feeds the free-parking house pot)\n"
    << "    cash Pi +=|-= amt           adjust a player's cash\n"
    << "    claim Pi                    claim the free-parking house pot\n\n";

  o << sec("ANALYSIS", pal) << "\n"
    << "    query state                 player panel: positions, cash, holdings\n"
    << "    query board                 full 40-square board: owners, development, tokens\n"
    << "    query risk|options Pi       risk profile + fair insurance premium for Pi\n"
    << "    query chain Pi              option chain: fair premium across deductibles\n"
    << "    query dist Pi ^n            landing distribution n rolls ahead\n"
    << "    query stationary            long-run landing probabilities\n"
    << "    query value @SQ             long-run landing probability for one square\n\n";

  o << sec("RULES", pal) << "\n"
    << "    rules <mugging|airport|pot> on|off    toggle a house rule mid-game\n\n";

  o << sec("CONTROL", pal) << "\n"
    << "    undo                        revert the last command\n"
    << "    help                        show this page\n"
    << "    quit                        exit\n\n";

  o << sec("EXAMPLES", pal) << "\n"
    << "    init 2\n"
    << "    buy P2 @TRAFALGAR_SQUARE\n"
    << "    build P2 @#24 + 5\n"
    << "    roll P1 = 3,4\n"
    << "    query options P1\n\n";

  o << sec("NOTES", pal) << "\n"
    << "    The advisory is always shown for the NEXT player to roll. 'Max single-roll\n"
    << "    risk' is the worst case if the dice (or a Chance card) route you onto the most\n"
    << "    expensive reachable property. Probabilities come from a 123-state Markov chain.\n";
  return o.str();
}

}  // namespace monopoly::engine
