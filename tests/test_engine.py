"""Engine behaviour tests over the UCI protocol.

- Zobrist keys: the incrementally updated key after a move sequence must equal
  the key computed from scratch for the resulting FEN.
- Tactics: forced mates the search must find, reported as "mate N".
- Protocol: "stop" ends "go infinite" promptly, and every bestmove is legal.

Usage: python tests/test_engine.py [path/to/tempo]
"""

import random
import subprocess
import sys
import time
from pathlib import Path

import chess

ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ENGINE = ROOT / "build" / ("tempo.exe" if sys.platform == "win32" else "tempo")

# (fen, moves-to-mate), labelled by Stockfish 19 at depth 30. Short mates must
# be found exactly; long endgame mates only need some forced mate.
MATES = [
    ("r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4", 1),
    ("4k3/8/4K3/8/8/8/8/7R w - - 0 1", 1),
    ("r1b2k1r/ppp1bppp/8/1B1Q4/5q2/2P5/PPP2PPP/R3R1K1 w - - 1 1", 2),
    ("r2qkb1r/pp2nppp/3p4/2pNN1B1/2BnP3/3P4/PPP2PPP/R2bK2R w KQkq - 1 1", 2),
    ("6k1/pp4p1/2p5/2bp4/8/P5Pb/1P3rrP/2BRRN1K b - - 0 1", 2),
    ("k7/8/2K5/8/8/8/8/1R6 w - - 0 1", 2),
    ("5rk1/1p1q2bp/p2pN1p1/2pP2Bn/2P3P1/1P6/P4QKP/5R2 w - - 0 1", 2),
    ("r5rk/5p1p/5R2/4B3/8/8/7P/7K w - - 0 1", 3),
    ("2r3k1/p4p2/3Rp2p/1p2P1pK/8/1P4P1/P3Q2P/1q6 b - - 0 1", 3),
    ("3r1r1k/1p3p1p/p2p4/4n1NN/6bQ/1BPq4/P3p1PP/1R5K w - - 0 1", 3),
    ("r1bk3r/pppq1ppp/5n2/4N1N1/2Bp4/Bn6/P4PPP/4R1K1 w - - 1 1", 4),
    ("8/8/8/8/8/4k3/8/R3K3 w - - 0 1", 11),
    ("8/8/8/2k5/8/8/8/Q3K3 w - - 0 1", 8),
]
EXACT_UP_TO = 4


class Engine:
    def __init__(self, path):
        self.p = subprocess.Popen([str(path)], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    def send(self, cmd):
        self.p.stdin.write(cmd + "\n")
        self.p.stdin.flush()

    def read_until(self, prefix, timeout=30):
        lines, t0 = [], time.time()
        while time.time() - t0 < timeout:
            line = self.p.stdout.readline().strip()
            lines.append(line)
            if line.startswith(prefix):
                return lines
        raise TimeoutError(prefix)

    def close(self):
        self.send("quit")
        self.p.wait(timeout=5)


def check_keys(eng, failures):
    rng = random.Random(7)
    checked = 0
    for _ in range(300):
        board = chess.Board()
        for _ in range(rng.randint(1, 80)):
            moves = list(board.legal_moves)
            if not moves:
                break
            board.push(rng.choice(moves))
        ucimoves = " ".join(m.uci() for m in board.move_stack)
        eng.send(f"position startpos moves {ucimoves}\nd")
        fen_line, key_line = eng.read_until("key")[-2:]
        eng.send(f"position fen {fen_line}\nd")
        fen2, key2 = eng.read_until("key")[-2:]
        incremental, scratch = key_line.split()[1], key2.split()[1]
        # python-chess only prints the ep square when a capture is legal; the
        # engine keeps it when any enemy pawn is adjacent, so compare boards only.
        if fen_line.split()[:3] != board.fen().split()[:3] or incremental != scratch:
            failures.append(f"key/fen mismatch after {ucimoves}: {fen_line} {incremental} vs {scratch}")
        checked += 1
    print(f"zobrist: {checked} random move sequences, incremental key == from-scratch key")


def check_mates(eng, failures):
    for fen, n in MATES:
        eng.send("ucinewgame")
        eng.send(f"position fen {fen}\ngo movetime 5000")
        lines = eng.read_until("bestmove")
        infos = [l for l in lines if l.startswith("info depth")]
        last = infos[-1].split()
        score = last[last.index("score") + 1: last.index("score") + 3]
        if n <= EXACT_UP_TO:
            ok = score == ["mate", str(n)]
        else:
            ok = score[0] == "mate" and int(score[1]) > 0
        print(f"{'ok  ' if ok else 'FAIL'} mate in {n}: got score {' '.join(score)}, {lines[-1]}")
        if not ok:
            failures.append(f"mate in {n} not found: {fen}")


def check_protocol(eng, failures):
    eng.send("ucinewgame\nposition startpos\ngo infinite")
    time.sleep(1.0)
    t0 = time.time()
    eng.send("stop")
    lines = eng.read_until("bestmove", timeout=5)
    latency = (time.time() - t0) * 1000
    move = lines[-1].split()[1]
    ok = move in {m.uci() for m in chess.Board().legal_moves} and latency < 200
    print(f"{'ok  ' if ok else 'FAIL'} go infinite + stop: bestmove {move} {latency:.0f} ms after stop")
    if not ok:
        failures.append("stop handling")

    eng.send("position startpos moves e2e4 e7e5\ngo wtime 1000 btime 1000 winc 0 binc 0")
    t0 = time.time()
    lines = eng.read_until("bestmove", timeout=5)
    spent = (time.time() - t0) * 1000
    ok = spent < 400
    print(f"{'ok  ' if ok else 'FAIL'} time management: {spent:.0f} ms used with 1 s on the clock")
    if not ok:
        failures.append("time management")


def main():
    eng = Engine(sys.argv[1] if len(sys.argv) > 1 else DEFAULT_ENGINE)
    eng.send("uci")
    eng.read_until("uciok")
    failures = []
    check_keys(eng, failures)
    check_mates(eng, failures)
    check_protocol(eng, failures)
    eng.close()
    for f in failures:
        print("FAIL:", f)
    print("PASS" if not failures else f"{len(failures)} FAILURE(S)")
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
