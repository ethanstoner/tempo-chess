#pragma once

#include "types.h"

constexpr Bitboard FILE_A = 0x0101010101010101ULL;
constexpr Bitboard FILE_H = FILE_A << 7;
constexpr Bitboard RANK_1 = 0xFFULL;
constexpr Bitboard RANK_2 = RANK_1 << 8;
constexpr Bitboard RANK_7 = RANK_1 << 48;
constexpr Bitboard RANK_8 = RANK_1 << 56;

constexpr Bitboard bb(int sq) { return 1ULL << sq; }
inline int lsb(Bitboard b) { return __builtin_ctzll(b); }
inline int popcount(Bitboard b) { return __builtin_popcountll(b); }
inline int pop_lsb(Bitboard& b) {
    int sq = lsb(b);
    b &= b - 1;
    return sq;
}
inline bool more_than_one(Bitboard b) { return b & (b - 1); }

struct Magic {
    Bitboard mask;
    Bitboard magic;
    Bitboard* attacks;
    int shift;
    unsigned index(Bitboard occ) const { return unsigned(((occ & mask) * magic) >> shift); }
};

extern Bitboard PawnAttacks[2][64];
extern Bitboard KnightAttacks[64];
extern Bitboard KingAttacks[64];
extern Bitboard Between[64][64]; // squares strictly between two aligned squares
extern Bitboard Line[64][64];    // full line through two aligned squares, 0 if not aligned
extern Magic BishopMagics[64];
extern Magic RookMagics[64];

inline Bitboard bishop_attacks(int sq, Bitboard occ) {
    const Magic& m = BishopMagics[sq];
    return m.attacks[m.index(occ)];
}
inline Bitboard rook_attacks(int sq, Bitboard occ) {
    const Magic& m = RookMagics[sq];
    return m.attacks[m.index(occ)];
}
inline Bitboard queen_attacks(int sq, Bitboard occ) { return bishop_attacks(sq, occ) | rook_attacks(sq, occ); }

inline Bitboard attacks_of(int pt, int sq, Bitboard occ) {
    switch (pt) {
    case KNIGHT: return KnightAttacks[sq];
    case BISHOP: return bishop_attacks(sq, occ);
    case ROOK: return rook_attacks(sq, occ);
    case QUEEN: return queen_attacks(sq, occ);
    case KING: return KingAttacks[sq];
    default: return 0;
    }
}

void init_bitboards();
