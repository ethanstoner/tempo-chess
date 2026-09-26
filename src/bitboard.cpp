#include "bitboard.h"

Bitboard PawnAttacks[2][64];
Bitboard KnightAttacks[64];
Bitboard KingAttacks[64];
Bitboard Between[64][64];
Bitboard Line[64][64];
Magic BishopMagics[64];
Magic RookMagics[64];

namespace {

Bitboard BishopTable[0x1480];
Bitboard RookTable[0x19000];

constexpr int ROOK_DIRS[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
constexpr int BISHOP_DIRS[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};

Bitboard slide(const int (&dirs)[4][2], int sq, Bitboard occ) {
    Bitboard result = 0;
    for (const auto& d : dirs) {
        int f = file_of(sq) + d[0], r = rank_of(sq) + d[1];
        while (f >= 0 && f < 8 && r >= 0 && r < 8) {
            int s = make_sq(f, r);
            result |= bb(s);
            if (occ & bb(s)) break;
            f += d[0];
            r += d[1];
        }
    }
    return result;
}

Bitboard step(int sq, const int (*deltas)[2], int n) {
    Bitboard result = 0;
    for (int i = 0; i < n; i++) {
        int f = file_of(sq) + deltas[i][0], r = rank_of(sq) + deltas[i][1];
        if (f >= 0 && f < 8 && r >= 0 && r < 8) result |= bb(make_sq(f, r));
    }
    return result;
}

u64 rng_state = 0x9E3779B97F4A7C15ULL;
u64 rand64() {
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 2685821657736338717ULL;
}

// Finds magic multipliers at startup with a sparse random search. Seeded, so
// the tables are identical every run; takes a few milliseconds.
void init_magics(const int (&dirs)[4][2], Magic* magics, Bitboard* table) {
    Bitboard occupancy[4096], reference[4096];
    int epoch[4096] = {}, attempt = 0;
    Bitboard* next = table;

    for (int sq = 0; sq < 64; sq++) {
        Bitboard edges = ((RANK_1 | RANK_8) & ~(RANK_1 << (8 * rank_of(sq)))) |
                         ((FILE_A | FILE_H) & ~(FILE_A << file_of(sq)));
        Magic& m = magics[sq];
        m.mask = slide(dirs, sq, 0) & ~edges;
        m.shift = 64 - popcount(m.mask);
        m.attacks = next;

        int size = 0;
        Bitboard b = 0;
        do {
            occupancy[size] = b;
            reference[size] = slide(dirs, sq, b);
            size++;
            b = (b - m.mask) & m.mask;
        } while (b);

        for (int i = 0; i < size;) {
            do {
                m.magic = rand64() & rand64() & rand64();
            } while (popcount((m.magic * m.mask) >> 56) < 6);
            attempt++;
            for (i = 0; i < size; i++) {
                unsigned idx = m.index(occupancy[i]);
                if (epoch[idx] < attempt) {
                    epoch[idx] = attempt;
                    m.attacks[idx] = reference[i];
                } else if (m.attacks[idx] != reference[i]) {
                    break;
                }
            }
        }
        next += size;
    }
}

} // namespace

void init_bitboards() {
    constexpr int KNIGHT_D[8][2] = {{1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
    constexpr int KING_D[8][2] = {{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}};
    constexpr int WPAWN_D[2][2] = {{-1, 1}, {1, 1}};
    constexpr int BPAWN_D[2][2] = {{-1, -1}, {1, -1}};

    for (int sq = 0; sq < 64; sq++) {
        KnightAttacks[sq] = step(sq, KNIGHT_D, 8);
        KingAttacks[sq] = step(sq, KING_D, 8);
        PawnAttacks[WHITE][sq] = step(sq, WPAWN_D, 2);
        PawnAttacks[BLACK][sq] = step(sq, BPAWN_D, 2);
    }

    init_magics(BISHOP_DIRS, BishopMagics, BishopTable);
    init_magics(ROOK_DIRS, RookMagics, RookTable);

    for (int a = 0; a < 64; a++) {
        for (int b = 0; b < 64; b++) {
            Between[a][b] = Line[a][b] = 0;
            if (a == b) continue;
            if (slide(BISHOP_DIRS, a, 0) & bb(b)) {
                Line[a][b] = (slide(BISHOP_DIRS, a, 0) & slide(BISHOP_DIRS, b, 0)) | bb(a) | bb(b);
                Between[a][b] = slide(BISHOP_DIRS, a, bb(b)) & slide(BISHOP_DIRS, b, bb(a));
            } else if (slide(ROOK_DIRS, a, 0) & bb(b)) {
                Line[a][b] = (slide(ROOK_DIRS, a, 0) & slide(ROOK_DIRS, b, 0)) | bb(a) | bb(b);
                Between[a][b] = slide(ROOK_DIRS, a, bb(b)) & slide(ROOK_DIRS, b, bb(a));
            }
        }
    }
}
