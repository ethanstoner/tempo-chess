#include "nnue.h"

#include <algorithm>

#include "nnue_weights.h"
#include "position.h"

namespace {

using namespace nnue_weights;
static_assert(HIDDEN == NNUE_HIDDEN, "nnue.h NNUE_HIDDEN must match the compiled-in network");

// Feature index of a piece seen from `view`'s side: own pieces first, board
// flipped vertically for black so both sides share one set of weights.
inline int feature(int view, int pc, int sq) {
    const int c = color_of(pc), pt = type_of(pc);
    return view == WHITE ? c * 384 + pt * 64 + sq : (c ^ 1) * 384 + pt * 64 + (sq ^ 56);
}

// SCReLU dot product: clamp(x)^2 * w, summed in 64 bits (a single term can
// reach 255^2 * 32767).
inline std::int64_t screlu_dot(const std::int16_t* acc, const std::int16_t* w) {
    std::int64_t sum = 0;
    for (int i = 0; i < HIDDEN; i++) {
        const std::int64_t c = std::clamp<int>(acc[i], 0, QA);
        sum += c * c * w[i];
    }
    return sum;
}

} // namespace

void nnue_reset(Accumulator& acc) {
    std::copy(FT_BIAS, FT_BIAS + HIDDEN, acc.v[WHITE]);
    std::copy(FT_BIAS, FT_BIAS + HIDDEN, acc.v[BLACK]);
}

void nnue_add(Accumulator& acc, int pc, int sq) {
    for (int view = WHITE; view <= BLACK; view++) {
        const std::int16_t* w = FT_WEIGHTS + feature(view, pc, sq) * HIDDEN;
        for (int i = 0; i < HIDDEN; i++) acc.v[view][i] += w[i];
    }
}

void nnue_sub(Accumulator& acc, int pc, int sq) {
    for (int view = WHITE; view <= BLACK; view++) {
        const std::int16_t* w = FT_WEIGHTS + feature(view, pc, sq) * HIDDEN;
        for (int i = 0; i < HIDDEN; i++) acc.v[view][i] -= w[i];
    }
}

int nnue_evaluate(const Position& pos) {
    Accumulator scratch;
    const Accumulator* acc = &pos.accumulator();
    if (!pos.tracks_accumulator()) {
        nnue_reset(scratch);
        for (Bitboard b = pos.occupied(); b;) {
            const int sq = pop_lsb(b);
            nnue_add(scratch, pos.piece_on(sq), sq);
        }
        acc = &scratch;
    }
    const int us = pos.side_to_move();
    const std::int64_t sum = screlu_dot(acc->v[us], OUT_WEIGHTS) + screlu_dot(acc->v[us ^ 1], OUT_WEIGHTS + HIDDEN);
    return int((sum / QA + OUT_BIAS) * SCALE / (std::int64_t(QA) * QB));
}
