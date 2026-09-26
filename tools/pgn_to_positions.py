"""Convert PGN games into "<fen> | <result>" lines for tools/tune.cpp.

Skips time-forfeit games, the opening-book plies (they carry no information about the engine's
own play), positions after the game was already decided by adjudication, and
duplicate positions. Result is from white's point of view.

  python tools/pgn_to_positions.py matches/selfplay.pgn temp/positions.txt --skip 16
"""

import argparse
import random

import chess.pgn

RESULTS = {"1-0": "1.0", "0-1": "0.0", "1/2-1/2": "0.5"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pgn", nargs="+")
    ap.add_argument("out")
    ap.add_argument("--skip", type=int, default=16, help="plies to skip at the start of each game")
    ap.add_argument("--per-game", type=int, default=0, help="sample at most this many positions per game (0 = all)")
    args = ap.parse_args()

    rng = random.Random(1)
    seen = set()
    games = kept = 0
    with open(args.out, "w") as out:
        for path in args.pgn:
            with open(path) as f:
                while (game := chess.pgn.read_game(f)) is not None:
                    result = RESULTS.get(game.headers.get("Result"))
                    # Time forfeits say nothing about the position on the board.
                    if result is None or game.headers.get("Termination", "normal") != "normal":
                        continue
                    games += 1
                    board = game.board()
                    fens = []
                    for ply, move in enumerate(game.mainline_moves()):
                        board.push(move)
                        if ply + 1 < args.skip:
                            continue
                        key = board.epd()
                        if key in seen:
                            continue
                        seen.add(key)
                        fens.append(board.fen())
                    if args.per_game and len(fens) > args.per_game:
                        fens = rng.sample(fens, args.per_game)
                    for fen in fens:
                        out.write(f"{fen} | {result}\n")
                    kept += len(fens)
                    if games % 2000 == 0:
                        print(f"{games} games, {kept} positions", flush=True)
    print(f"done: {games} games, {kept} positions -> {args.out}")


if __name__ == "__main__":
    main()
