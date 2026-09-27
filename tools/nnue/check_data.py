"""Sanity-check a datagen file: every record must decode to a legal board.

  python tools/nnue/check_data.py data/train.bin
"""

import sys

import chess
import numpy as np

from data import decode, load

PIECES = "PNBRQKpnbrqk"


def main():
    recs = load(sys.argv[1])
    n = len(recs)
    sample = recs[np.random.default_rng(0).choice(n, size=min(n, 20000), replace=False)]
    piece, square, _ = decode(sample)
    bad = 0
    for i in range(len(sample)):
        board = chess.Board(None)
        for p, s in zip(piece[i], square[i]):
            if p >= 0:
                board.set_piece_at(int(s), chess.Piece.from_symbol(PIECES[p]))
        board.turn = chess.WHITE if sample["stm"][i] == 0 else chess.BLACK
        status = board.status() & ~(chess.STATUS_BAD_CASTLING_RIGHTS)
        if status != chess.STATUS_VALID or board.is_check():
            bad += 1
            if bad <= 3:
                print("bad record:", board.fen(), status)
    res = np.bincount(recs["result"], minlength=3) / n
    print(f"{n} records; {len(sample)} decoded, {bad} invalid")
    print(f"results: black {res[0]:.1%}  draw {res[1]:.1%}  white {res[2]:.1%}")
    s = recs["score"].astype(np.int32)
    print(f"score (white POV): mean {s.mean():.1f}, |score|<100: {np.mean(np.abs(s) < 100):.1%}, "
          f"sign agrees with decisive result: "
          f"{np.mean(np.sign(s[recs['result'] != 1]) == np.where(recs['result'][recs['result'] != 1] == 2, 1, -1)):.1%}")
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
