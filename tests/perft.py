"""Move-generation correctness tests.

1. The six standard perft positions against their published node counts.
2. Random positions from random games, perft-divided and compared move by
   move against python-chess, so a mismatch names the exact faulty move.

Usage: python tests/perft.py [path/to/tempo] [--deep] [--random N]
"""

import argparse
import random
import subprocess
import sys
import time
from pathlib import Path

import chess

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ENGINE = ROOT / "build" / ("tempo.exe" if sys.platform == "win32" else "tempo")

# (fen, {depth: nodes}) from https://www.chessprogramming.org/Perft_Results
SUITE = [
    ("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
     {1: 20, 2: 400, 3: 8902, 4: 197281, 5: 4865609, 6: 119060324}),
    ("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
     {1: 48, 2: 2039, 3: 97862, 4: 4085603, 5: 193690690}),
    ("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
     {1: 14, 2: 191, 3: 2812, 4: 43238, 5: 674624, 6: 11030083}),
    ("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
     {1: 6, 2: 264, 3: 9467, 4: 422333, 5: 15833292}),
    ("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
     {1: 44, 2: 1486, 3: 62379, 4: 2103487, 5: 89941194}),
    ("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
     {1: 46, 2: 2079, 3: 89890, 4: 3894594, 5: 164075551}),
]
QUICK_MAX_NODES = 5_000_000


class Engine:
    def __init__(self, path):
        self.p = subprocess.Popen([str(path)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    def divide(self, fen, depth):
        self.p.stdin.write(f"position fen {fen}\ngo perft {depth}\n")
        self.p.stdin.flush()
        moves, total, ms = {}, None, 0
        while True:
            line = self.p.stdout.readline().strip()
            if line.startswith("Nodes:"):
                total = int(line.split()[1])
            elif line.startswith("Time:"):
                ms = int(line.split()[1])
            elif line.startswith("NPS:"):
                return moves, total, ms
            elif ": " in line:
                mv, n = line.split(": ")
                moves[mv] = int(n)

    def close(self):
        self.p.stdin.write("quit\n")
        self.p.stdin.flush()
        self.p.wait()


def reference_divide(fen, depth):
    board = chess.Board(fen)
    out = {}
    for mv in board.legal_moves:
        board.push(mv)
        out[mv.uci()] = 1 if depth == 1 else _perft(board, depth - 1)
        board.pop()
    return out


def _perft(board, depth):
    if depth == 1:
        return board.legal_moves.count()
    n = 0
    for mv in board.legal_moves:
        board.push(mv)
        n += _perft(board, depth - 1)
        board.pop()
    return n


def random_positions(count, seed):
    rng = random.Random(seed)
    fens = []
    while len(fens) < count:
        board = chess.Board()
        for _ in range(rng.randint(4, 120)):
            moves = list(board.legal_moves)
            if not moves:
                break
            board.push(rng.choice(moves))
        if not board.is_game_over():
            fens.append(board.fen())
    return fens


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("engine", nargs="?", default=DEFAULT_ENGINE)
    ap.add_argument("--deep", action="store_true", help="run every published depth (slow)")
    ap.add_argument("--random", type=int, default=200, help="random positions to diff against python-chess")
    args = ap.parse_args()

    eng = Engine(args.engine)
    failures = 0
    total_nodes = total_ms = 0

    for fen, expected in SUITE:
        for depth, want in expected.items():
            if not args.deep and want > QUICK_MAX_NODES:
                continue
            _, got, ms = eng.divide(fen, depth)
            total_nodes += got
            total_ms += ms
            ok = got == want
            failures += not ok
            print(f"{'ok  ' if ok else 'FAIL'} d{depth} {got:>11} (want {want:>11})  {fen}")

    t0 = time.time()
    for i, fen in enumerate(random_positions(args.random, seed=1)):
        depth = 3
        got, _, _ = eng.divide(fen, depth)
        want = reference_divide(fen, depth)
        if got != want:
            failures += 1
            print(f"FAIL random #{i}: {fen}")
            for mv in sorted(set(got) | set(want)):
                if got.get(mv) != want.get(mv):
                    print(f"     {mv}: engine={got.get(mv)} python-chess={want.get(mv)}")
    print(f"random: {args.random} positions diffed at depth 3 in {time.time() - t0:.1f}s")

    eng.close()
    nps = total_nodes * 1000 // max(total_ms, 1)
    print(f"\nsuite perft speed: {total_nodes} nodes, {nps / 1e6:.1f} M nodes/s")
    print("PASS" if failures == 0 else f"{failures} FAILURE(S)")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
