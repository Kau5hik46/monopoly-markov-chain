# Product Steering — Monopoly Markov Advisor

## What this is
A C++17 decision-support tool for a **live, in-progress** game of (London-edition)
Monopoly. The operator types real game events as they happen via a small command
language; after each event the tool re-renders the full game state and prints
decision support: the probability and risk profile for the player about to roll,
and a fair insurance/option price for the next roll.

It is **not** an autonomous game player. It is a co-pilot: it tracks ground truth
from operator input and answers "given where things actually are, what is the risk
and the fair price of insuring the next roll?"

## Who uses it
A person playing or watching a real Monopoly game who wants quantitative edge:
landing probabilities, expected rent liability, ruin risk, and insurance pricing.

## Core capabilities
1. **Live state tracking** — a formal command DSL captures every event (rolls,
   buys, trades, rent, builds, mortgages, taxes, cards, jail, mugging, airport
   travel, free-parking claims). Each command reports its full causal side-effect
   chain and the tool always re-renders the complete game state afterwards.
2. **Probability engine (position-only Markov)** — exact stationary `π` and
   transient `pₙ = e₀·Pⁿ` landing distributions over the 40 squares, with full
   Chance/Community-Chest movement-card modeling and the 3-doubles→jail rule.
3. **Risk & valuation layer** — overlays live ownership/cash/houses onto the
   position probabilities to produce indices: expected rent liability, property
   valuation, ruin probability.
4. **Option / insurance pricing** — v1 prices single-roll rent liability for the
   next roller (fair premium = expected rent + signed mugging EV). Designed to
   extend to N-roll, strike-based option chains, and ruin insurance.
5. **Configurable house rules** — a first-class rules subsystem. Mugging, airport
   travel, and the Free-Parking house pot are all toggleable and parameterized.

## Edition & house rules (locked)
- **Board**: London-themed variant (Portobello Road Market → The City; airports as
  stations; Telecoms & The Sun as utilities). See `data/board.london.json`.
- **Mugging**: landing on an opponent-occupied square (excluding Jail and Free
  Parking) triggers a separate 2d6 contest. Mugger wins on strictly greater roll:
  £500K transfers from muggee and muggee goes to Free Parking (hospital). Muggee
  wins on ≥ (ties to muggee): mugger goes to Jail.
- **Airport travel**: landing on an airport you own lets you optionally travel to
  another airport you also own, at the cost of skipping your next turn.
- **Free-Parking house pot**: Income Tax paid to the bank adds 2 houses to the pot,
  Super Tax adds 1; houses are drawn from the bank's limited 32-house/12-hotel
  supply. A player landing on Free Parking claims the whole pot and places houses on
  properties they hold a monopoly on (per normal build rules); houses that cannot be
  placed convert to cash at `count × build cost`.

## Success criteria
- Single-roll risk and option premium for the next roller are computed exactly from
  known live positions, including mugging EV, even with coupling rules on.
- Analytic stationary distribution matches the Monte-Carlo simulator (rules off)
  within tolerance.
- Every command's side effects and the full game state are always visible.

## Out of scope (v1)
- Autonomous strategy/agent play, GUI, networking, persistence beyond session
  save/load, and AI opponents. The strike-based option chain (requirement C),
  N-roll cumulative and bankruptcy insurance are designed-for but not built in v1.
