"""Check the engine's integer NNUE against the PyTorch float network.

Evaluates random positions three ways: the float model, a Python replica of
the engine's integer arithmetic, and the engine itself. The engine must equal
the integer replica exactly; the float gap is the cost of quantization.

Each position is also reached by replaying its moves, which exercises the
incrementally updated accumulator; that must agree with a fresh build too.

  python tools/nnue/verify.py src/nnue_weights.h.pt build/tempo.exe --hidden 256
"""

import argparse
import random
import subprocess

import chess
import numpy as np
import torch

from data import RECORD
from train import QA, QB, SCALE, Net, features

PIECE_CODE = {(c, t): (0 if c else 6) + (t - 1) for c in (True, False) for t in range(1, 7)}


def record(board):
    r = np.zeros(1, dtype=RECORD)
    occ, nib = 0, []
    for sq in range(64):
        p = board.piece_at(sq)
        if p:
            occ |= 1 << sq
            nib.append(PIECE_CODE[(p.color, p.piece_type)])
    r["occupancy"] = occ
    for i, v in enumerate(nib):
        r["pieces"][0, i // 2] |= v << (4 * (i % 2))
    r["stm"] = 0 if board.turn else 1
    return r


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("weights_pt")
    ap.add_argument("engine")
    ap.add_argument("--hidden", type=int, default=256)
    ap.add_argument("--positions", type=int, default=300)
    args = ap.parse_args()

    net = Net(args.hidden)
    net.load_state_dict(torch.load(args.weights_pt, map_location="cpu"))
    net.eval()
    ft_w = torch.round(net.ft.weight.detach() * QA).long()
    ft_b = torch.round(net.ft_bias.detach() * QA).long()
    out_w = torch.round(net.out.weight.detach()[0] * QB).long()
    out_b = int(round(net.out.bias.item() * QA * QB))
    h = args.hidden

    def quantized(us, them, w):
        mask = w[0].bool()
        a = ft_w[us[0][mask]].sum(0) + ft_b
        b = ft_w[them[0][mask]].sum(0) + ft_b
        s = int((a.clamp(0, QA) ** 2 * out_w[:h]).sum() + (b.clamp(0, QA) ** 2 * out_w[h:]).sum())
        v = int(s / QA) + out_b  # C++ integer division truncates toward zero
        num = v * SCALE
        return abs(num) // (QA * QB) * (1 if num >= 0 else -1)

    eng = subprocess.Popen([args.engine], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    eng.stdin.write("setoption name UseNNUE value true\n")

    def engine_eval(position):
        eng.stdin.write(f"position {position}\nd\n")
        eng.stdin.flush()
        while not (line := eng.stdout.readline()).startswith("key"):
            pass
        return int(line.split()[-1])

    rng = random.Random(3)
    diffs, mismatches, incremental_bad = [], 0, 0
    for _ in range(args.positions):
        board = chess.Board()
        for _ in range(rng.randint(0, 80)):
            moves = list(board.legal_moves)
            if not moves:
                break
            board.push(rng.choice(moves))
        with torch.no_grad():
            f = features(record(board), "cpu")
            ref = net(*f).item() * SCALE
            exact = quantized(*f)
        got = engine_eval(f"fen {board.fen()}")
        replayed = engine_eval("startpos moves " + " ".join(m.uci() for m in board.move_stack))
        diffs.append(abs(got - ref))
        mismatches += got != exact
        incremental_bad += replayed != got
    eng.stdin.write("quit\n")
    eng.stdin.flush()
    d = np.array(diffs)
    print(f"{len(d)} positions: engine == integer replica in {len(d) - mismatches}; "
          f"incremental == rebuilt in {len(d) - incremental_bad}; "
          f"quantization gap vs float: mean {d.mean():.2f} cp, max {d.max():.2f} cp")
    raise SystemExit(0 if mismatches == 0 and incremental_bad == 0 else 1)


if __name__ == "__main__":
    main()
