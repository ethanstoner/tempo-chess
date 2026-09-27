"""Reader for the 32-byte records written by `tempo datagen` (src/datagen.h)."""

import numpy as np

RECORD = np.dtype([
    ("occupancy", "<u8"),
    ("pieces", "u1", 16),
    ("score", "<i2"),
    ("result", "u1"),
    ("stm", "u1"),
    ("pad", "u1", 4),
])
assert RECORD.itemsize == 32


def load(path):
    return np.memmap(path, dtype=RECORD, mode="r")


def decode(records):
    """Vectorised decode of a batch of records.

    Returns (piece, square, count) arrays shaped [N, 32] with piece codes 0-11
    (white P N B R Q K, then black) and -1 padding, plus per-record piece
    counts. Squares use a1 = 0.
    """
    n = len(records)
    occ = records["occupancy"].astype(np.uint64)
    bits = ((occ[:, None] >> np.arange(64, dtype=np.uint64)) & np.uint64(1)).astype(bool)  # [N, 64]
    nibbles = np.empty((n, 32), dtype=np.int16)
    nibbles[:, 0::2] = records["pieces"] & 0x0F
    nibbles[:, 1::2] = records["pieces"] >> 4

    count = bits.sum(axis=1)
    order = np.argsort(~bits, axis=1, kind="stable")[:, :32]  # occupied squares, ascending
    slot = np.arange(32)[None, :]
    valid = slot < count[:, None]
    square = np.where(valid, order, -1).astype(np.int16)
    piece = np.where(valid, nibbles, -1).astype(np.int16)
    return piece, square, count
