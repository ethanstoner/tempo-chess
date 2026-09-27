#pragma once

#include "position.h"

struct Score {
    int mg = 0, eg = 0;
};

// Flat parameter layout shared by the evaluator and the tuner.
namespace param {
constexpr int MATERIAL = 0;             // [6]
constexpr int PST = MATERIAL + 6;       // [6][64], white's view, a1 = 0
constexpr int PASSED = PST + 6 * 64;    // [8] by relative rank
constexpr int DOUBLED = PASSED + 8;
constexpr int ISOLATED = DOUBLED + 1;
constexpr int BISHOP_PAIR = ISOLATED + 1;
constexpr int ROOK_OPEN = BISHOP_PAIR + 1;
constexpr int ROOK_SEMI_OPEN = ROOK_OPEN + 1;
constexpr int MOBILITY_N = ROOK_SEMI_OPEN + 1; // [9]
constexpr int MOBILITY_B = MOBILITY_N + 9;     // [14]
constexpr int MOBILITY_R = MOBILITY_B + 14;    // [15]
constexpr int MOBILITY_Q = MOBILITY_R + 15;    // [28]
constexpr int KING_ATTACK = MOBILITY_Q + 28;   // [4] per king-zone square hit, N/B/R/Q
constexpr int PAWN_SHIELD = KING_ATTACK + 4;
constexpr int TEMPO = PAWN_SHIELD + 1;
constexpr int COUNT = TEMPO + 1;
} // namespace param

extern Score PARAMS[param::COUNT];

// Per-parameter feature counts (white minus black) for the tuner.
struct EvalTrace {
    int coeff[param::COUNT];
    int phase;
};

// Selects the neural network (default) or the hand-written evaluation.
extern bool USE_NNUE;

void init_eval();
int evaluate(const Position& pos);   // side to move's point of view
int evaluate_white(const Position& pos, EvalTrace* trace);
