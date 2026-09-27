# Tempo

A chess engine written from scratch in C++20. It plays at about **2760 on Stockfish 19's
strength-limited scale**. Every change since the first version was kept or thrown out
based on measured games, not intuition.

![Tempo's score against Stockfish 19 at six strength settings, v0.1 and v0.2](docs/strength.svg)

### Highlights

- **Rated 2758 (95% CI 2718–2797)** against Stockfish 19's `UCI_Elo` levels. The rating is a
  maximum-likelihood fit over 600 games at 10s+0.1s, up from 2688 for v0.1.
- **+94 ± 28 Elo** from a hand-built evaluation (pawn structure, mobility, king safety),
  measured over 400 games against the previous version.
- **Move generation matches every one of 32 published perft counts**, 610M nodes at
  144M nodes/s with bulk leaf counting. 200 random positions also match python-chess
  move for move.
- **Ready for Lichess through the official `lichess-bot` bridge.** An offline end-to-end test
  plays full games through lichess-bot's mocked server. It is not live yet: a BOT account
  has to be created first.

**C++20 · CMake · Python · fastchess · Stockfish (as a sparring partner) · lichess-bot**

## Overview

Tempo is a classical alpha-beta engine that speaks UCI, so it runs in any chess GUI, in
match runners like fastchess and cutechess, and on Lichess as a BOT account. The
interesting part is the method rather than the playing strength. Correctness is pinned by
perft and an external oracle, strength is measured against a fixed reference opponent, and
after the first version each change had to win games before it was kept. One change I expected to help, a
machine-tuned evaluation, lost games and was rejected (see below).

## Engineering Highlights

- **Built a bitboard move generator with magic-bitboard sliders**, found at startup by a
  seeded search, and a pin-aware legality check that avoids make/unmake for most moves.
  It passes every published perft count up to depth 6 (610M nodes) and a move-by-move
  diff against python-chess on 200 random positions.
- **Implemented a principal-variation search** with a transposition table, iterative
  deepening, aspiration windows, null-move pruning, reverse futility, late-move
  reductions and pruning, SEE-based capture pruning, killer moves, butterfly history
  and 1- and 2-ply continuation history, plus quiescence search.
- **Measured strength against a fixed reference instead of self-play alone.** Each release
  plays 100 games against Stockfish 19 at six `UCI_Elo` limits (1800 to 3190), with both
  colours of every opening from an 8-move book. A single rating is fitted by maximum
  likelihood with a 95% confidence interval.
- **Gated changes with head-to-head matches and SPRT** through fastchess.
  - New evaluation terms: **+94 ± 28 Elo** (400 games).
  - Continuation history: **+7.9 ± 6.6 Elo** over 4,000 games. The SPRT's
    log-likelihood ratio reached 2.61 of the 2.94 needed to formally accept, so this is
    a small gain whose confidence interval sits just above zero.
- **Wrote a multithreaded Texel tuner** (`tools/tune.cpp`). It fits all 475 evaluation
  parameter pairs to game outcomes with Adam and reports loss on 10% held-out positions.
  The engine's evaluator emits a feature trace, so the tuner optimises exactly the code
  the engine runs.
- **Rejected the tuned weights because they lost games.** Tuning on 750k quiet positions
  from 8,000 self-play games cut held-out MSE from 0.0783 to 0.0743. Every tuned build
  still played worse than the hand-set values:

  | tuned variant | opponent | result (400 games) |
  |---|---|---|
  | all parameters free | v0.1 | −41 ± 30 Elo |
  | rare features frozen | hand-set eval | −105 ± 30 Elo |
  | rare features and material frozen | hand-set eval | −139 ± 30 Elo |

  The same hand-set eval scored +94 ± 28 against v0.1.

  Lower prediction error on self-play results did not buy playing strength here, so the
  hand-set values ship.
- **Packaged for Lichess with the official `lichess-bot` bridge.** The token comes only from
  the `LICHESS_BOT_TOKEN` environment variable. An offline test swaps lichess-bot's scripted
  mock opponent for one that plays full random games, and on the final build Tempo wins by
  checkmate with every move legal and clock time to spare.

<details>
<summary>Sample game: Tempo mates Stockfish 19 (UCI_Elo 2800) with two rooks</summary>

![Final position, Tempo mates with Rh8](docs/mate_vs_sf2800.svg)

Full game: [`docs/mate_vs_sf2800.pgn`](docs/mate_vs_sf2800.pgn)
</details>

## Results

Score against Stockfish 19, 100 games per level, 10s + 0.1s:

| Stockfish `UCI_Elo` | v0.1 (PeSTO eval) | v0.2 |
|---|---|---|
| 1800 | 93.0% | 97.0% |
| 2200 | 90.0% | 92.0% |
| 2600 | 67.5% | 82.5% |
| 2800 | 36.0% | 41.5% |
| 3000 | 17.0% | 19.0% |
| 3190 (max) | 6.5% | 6.5% |
| **fitted rating** | **2688** (2647–2728) | **2758** (2718–2797) |

All 1,200 games ended normally: no illegal moves, crashes or time forfeits. These ratings
sit on Stockfish's own `UCI_Elo` scale at a fast time control. They are not a CCRL or
Lichess rating.

## Architecture

```
UCI loop (main.cpp) ──► Search (search.cpp)  ◄── TranspositionTable (tt.h)
                          │  iterative deepening → PVS → quiescence
                          ▼
                       Position (position.cpp)  ◄── Bitboards / magic sliders (bitboard.cpp)
                          │  make/unmake, legality, SEE, Zobrist, repetition
                          ▼
                       Evaluation (eval.cpp) ── one code path for search and tuner trace
```

Tooling around the engine:

- `tools/match.py` runs fastchess gauntlets and SPRTs.
- `tools/pgn_to_positions.py` and `tools/tune.cpp` turn games into tuning data and tune
  the evaluation.
- `tools/plot_gauntlet.py` draws the chart above from the match logs.

## Getting Started

Needs CMake 3.20+ and a C++20 compiler. Built and tested with GCC 16.2 (mingw-w64, Windows)
and GCC 13.3 (Ubuntu under WSL); both builds give the same `bench` node count.

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/tempo            # UCI engine: plug into any chess GUI
./build/tempo bench      # fixed-depth search signature + nodes/s
```

To play on Lichess, create a fresh account that has never played a game, make a token with
the `bot:play` scope, then:

```powershell
$env:LICHESS_BOT_TOKEN = "lip_..."
.\deploy\run.ps1 -Upgrade   # once: converts the account to a BOT (irreversible)
.\deploy\run.ps1
```

On Linux or a VPS, `deploy/run.sh -u` and then `deploy/run.sh` do the same. Both scripts
were run up to Lichess's token check. A real account has not been connected yet.

## Testing

```sh
python tests/perft.py --deep     # 32 published perft counts + 200-position python-chess diff
python tests/test_engine.py      # Zobrist, eval symmetry, 13 forced mates, UCI stop, time use
./deploy/test_lichess.ps1        # full game through lichess-bot's mocked lichess.org
```

`test_engine.py` covers four areas:

- **Zobrist:** the incremental hash equals a from-scratch hash after 300 random move
  sequences.
- **Eval symmetry:** each of 300 random positions scores the same as its colour-flipped
  mirror.
- **Forced mates:** 13 positions labelled by Stockfish at depth 30. Mates up to mate in 4
  must be found at the exact length.
- **UCI protocol:** `stop` returns a move within 200 ms, and a 1-second clock is not
  overspent.

Reproducing the strength numbers needs `fastchess`, a Stockfish binary and the
`8moves_v3.pgn` book in `tools/`:

```sh
python tools/match.py gauntlet --levels 1800 2200 2600 2800 3000 3190 --games 100
python tools/match.py sprt --new build/tempo --old path/to/baseline
```

## What I Learned

- **A better loss score is not a better engine.** The tuner lowered held-out error on game
  outcomes, but lost 40 to 140 Elo in play. Only games can judge an evaluation.
- **Test data needs its own checks.** Twice my hand-written positions were wrong: one
  bench position was illegal (the side not to move was in check), and several "mate in
  N" puzzles were really mate in 1. An external oracle (Stockfish, python-chess) caught
  both. Checks I only reasoned about did not.
- **Small gains need thousands of games.** Continuation history's roughly +8 Elo was
  indistinguishable from noise at 400 games and only separated from zero near 4,000.

## Credits

Starting piece-square tables and material values are PeSTO's (Ronald Friederich, via
the Chess Programming Wiki). `lichess-bot`, fastchess, Stockfish and the 8-move opening
book are external projects downloaded at setup, not vendored.
