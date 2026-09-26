#include "eval.h"

#include <algorithm>

#include "eval_params.h"
#include "pesto_tables.h"

Score PARAMS[param::COUNT];

namespace {

Bitboard PassedMask[2][64]; // squares ahead on own and adjacent files
Bitboard AdjacentFiles[8];
Bitboard ShieldMask[2][64]; // two ranks in front of the king, three files wide

Bitboard file_bb(int f) { return FILE_A << f; }

// Starting values before any tuning: PeSTO material and tables, plus rough
// hand-set values for the extra terms.
void default_params() {
    using namespace param;
    for (int pt = PAWN; pt <= KING; pt++) {
        PARAMS[MATERIAL + pt] = {PESTO_MG_VALUE[pt], PESTO_EG_VALUE[pt]};
        for (int sq = 0; sq < 64; sq++)
            PARAMS[PST + pt * 64 + sq] = {PESTO_MG_PST[pt][sq ^ 56], PESTO_EG_PST[pt][sq ^ 56]};
    }
    const int passed[8] = {0, 5, 10, 20, 40, 70, 110, 0};
    for (int r = 0; r < 8; r++) PARAMS[PASSED + r] = {passed[r] / 2, passed[r]};
    PARAMS[DOUBLED] = {-10, -20};
    PARAMS[ISOLATED] = {-10, -10};
    PARAMS[BISHOP_PAIR] = {30, 50};
    PARAMS[ROOK_OPEN] = {25, 10};
    PARAMS[ROOK_SEMI_OPEN] = {10, 5};
    for (int i = 0; i < 9; i++) PARAMS[MOBILITY_N + i] = {4 * (i - 4), 4 * (i - 4)};
    for (int i = 0; i < 14; i++) PARAMS[MOBILITY_B + i] = {4 * (i - 6), 4 * (i - 6)};
    for (int i = 0; i < 15; i++) PARAMS[MOBILITY_R + i] = {2 * (i - 7), 4 * (i - 7)};
    for (int i = 0; i < 28; i++) PARAMS[MOBILITY_Q + i] = {1 * (i - 13), 2 * (i - 13)};
    PARAMS[KING_ATTACK + 0] = {8, 0};
    PARAMS[KING_ATTACK + 1] = {6, 0};
    PARAMS[KING_ATTACK + 2] = {8, 0};
    PARAMS[KING_ATTACK + 3] = {12, 0};
    PARAMS[PAWN_SHIELD] = {10, 0};
    PARAMS[TEMPO] = {10, 5};
}

template <bool TRACE>
struct Evaluator {
    const Position& pos;
    EvalTrace* trace;
    int mg = 0, eg = 0;

    void add(int idx, int color, int count = 1) {
        const int sign = color == WHITE ? count : -count;
        mg += PARAMS[idx].mg * sign;
        eg += PARAMS[idx].eg * sign;
        if constexpr (TRACE) trace->coeff[idx] += sign;
    }

    void pawns(int c) {
        const Bitboard own = pos.pcs(c, PAWN), enemy = pos.pcs(c ^ 1, PAWN);
        for (Bitboard b = own; b;) {
            const int sq = pop_lsb(b);
            if (!(PassedMask[c][sq] & enemy)) add(param::PASSED + relative_rank(c, sq), c);
            if (!(AdjacentFiles[file_of(sq)] & own)) add(param::ISOLATED, c);
        }
        for (int f = 0; f < 8; f++) {
            const int n = popcount(own & file_bb(f));
            if (n > 1) add(param::DOUBLED, c, n - 1);
        }
    }

    void pieces(int c) {
        const int them = c ^ 1;
        const Bitboard occ = pos.occupied();
        const Bitboard enemyPawns = pos.pcs(them, PAWN);
        const Bitboard pawnAttacks = enemyPawns == 0 ? 0
                                     : them == WHITE ? ((enemyPawns << 7) & ~FILE_H) | ((enemyPawns << 9) & ~FILE_A)
                                                     : ((enemyPawns >> 9) & ~FILE_H) | ((enemyPawns >> 7) & ~FILE_A);
        const Bitboard mobilityArea = ~(pos.pcs(c, PAWN) | pos.pcs(c, KING) | pawnAttacks);
        const int eksq = pos.king_sq(them);
        const Bitboard kingZone = KingAttacks[eksq] | bb(eksq);
        static constexpr int MOB[4] = {param::MOBILITY_N, param::MOBILITY_B, param::MOBILITY_R, param::MOBILITY_Q};

        if (popcount(pos.pcs(c, BISHOP)) >= 2) add(param::BISHOP_PAIR, c);

        for (int pt = KNIGHT; pt <= QUEEN; pt++) {
            for (Bitboard b = pos.pcs(c, pt); b;) {
                const int sq = pop_lsb(b);
                const Bitboard att = attacks_of(pt, sq, occ);
                add(MOB[pt - KNIGHT] + popcount(att & mobilityArea), c);
                if (const int hits = popcount(att & kingZone)) add(param::KING_ATTACK + pt - KNIGHT, c, hits);
                if (pt == ROOK) {
                    const Bitboard file = file_bb(file_of(sq));
                    if (!(file & pos.pcs(c, PAWN))) add(file & enemyPawns ? param::ROOK_SEMI_OPEN : param::ROOK_OPEN, c);
                }
            }
        }

        const int ksq = pos.king_sq(c);
        if (const int shield = popcount(ShieldMask[c][ksq] & pos.pcs(c, PAWN))) add(param::PAWN_SHIELD, c, shield);
    }

    int run() {
        int phase = 0;
        for (int sq = 0; sq < 64; sq++) {
            const int pc = pos.piece_on(sq);
            if (pc == NO_PIECE) continue;
            const int c = color_of(pc), pt = type_of(pc);
            const int rel = c == WHITE ? sq : sq ^ 56;
            add(param::MATERIAL + pt, c);
            add(param::PST + pt * 64 + rel, c);
            phase += PHASE_INC[pt];
        }
        for (int c = WHITE; c <= BLACK; c++) {
            pawns(c);
            pieces(c);
        }
        add(param::TEMPO, pos.side_to_move());

        phase = std::min(phase, 24);
        if constexpr (TRACE) trace->phase = phase;
        return (mg * phase + eg * (24 - phase)) / 24;
    }
};

} // namespace

void init_eval() {
    if constexpr (HAVE_TUNED_PARAMS) {
        for (int i = 0; i < param::COUNT; i++) PARAMS[i] = {TUNED_PARAMS[i][0], TUNED_PARAMS[i][1]};
    } else {
        default_params();
    }

    for (int f = 0; f < 8; f++)
        AdjacentFiles[f] = (f > 0 ? file_bb(f - 1) : 0) | (f < 7 ? file_bb(f + 1) : 0);
    for (int sq = 0; sq < 64; sq++) {
        const int f = file_of(sq), r = rank_of(sq);
        const Bitboard files = file_bb(f) | AdjacentFiles[f];
        Bitboard aheadW = 0, aheadB = 0;
        for (int rr = r + 1; rr < 8; rr++) aheadW |= RANK_1 << (8 * rr);
        for (int rr = r - 1; rr >= 0; rr--) aheadB |= RANK_1 << (8 * rr);
        PassedMask[WHITE][sq] = files & aheadW;
        PassedMask[BLACK][sq] = files & aheadB;
        Bitboard twoW = 0, twoB = 0;
        for (int rr = r + 1; rr <= std::min(r + 2, 7); rr++) twoW |= RANK_1 << (8 * rr);
        for (int rr = r - 1; rr >= std::max(r - 2, 0); rr--) twoB |= RANK_1 << (8 * rr);
        ShieldMask[WHITE][sq] = files & twoW;
        ShieldMask[BLACK][sq] = files & twoB;
    }
}

int evaluate_white(const Position& pos, EvalTrace* trace) {
    if (trace) {
        *trace = EvalTrace{};
        return Evaluator<true>{pos, trace}.run();
    }
    return Evaluator<false>{pos, nullptr}.run();
}

int evaluate(const Position& pos) {
    const int v = evaluate_white(pos, nullptr);
    return pos.side_to_move() == WHITE ? v : -v;
}
