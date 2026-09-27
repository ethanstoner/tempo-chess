#include "nnue.h"

#include <algorithm>

#include "nnue_weights.h"

namespace {

using namespace nnue_weights;

// Feature index of a piece seen from `view`'s side: own pieces first, board
// flipped vertically for black so both sides share one set of weights.
inline int feature(int view, int pc, int sq) {
    const int c = color_of(pc), pt = type_of(pc);
    return view == WHITE ? c * 384 + pt * 64 + sq : (c ^ 1) * 384 + pt * 64 + (sq ^ 56);
}

inline void add_feature(std::int16_t* acc, int f) {
    const std::int16_t* w = FT_WEIGHTS + f * HIDDEN;
    for (int i = 0; i < HIDDEN; i++) acc[i] += w[i];
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

int nnue_evaluate(const Position& pos) {
    alignas(64) std::int16_t acc[2][HIDDEN];
    std::copy(FT_BIAS, FT_BIAS + HIDDEN, acc[WHITE]);
    std::copy(FT_BIAS, FT_BIAS + HIDDEN, acc[BLACK]);
    for (Bitboard b = pos.occupied(); b;) {
        const int sq = pop_lsb(b), pc = pos.piece_on(sq);
        add_feature(acc[WHITE], feature(WHITE, pc, sq));
        add_feature(acc[BLACK], feature(BLACK, pc, sq));
    }
    const int us = pos.side_to_move();
    const std::int64_t sum = screlu_dot(acc[us], OUT_WEIGHTS) + screlu_dot(acc[us ^ 1], OUT_WEIGHTS + HIDDEN);
    return int((sum / QA + OUT_BIAS) * SCALE / (std::int64_t(QA) * QB));
}
