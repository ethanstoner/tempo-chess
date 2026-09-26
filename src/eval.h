#pragma once

#include <algorithm>

#include "position.h"

// Tapered PeSTO evaluation from the side to move's point of view. The mg/eg
// sums are maintained incrementally by Position, so this is O(1).
inline int evaluate(const Position& pos) {
    const int us = pos.side_to_move(), them = us ^ 1;
    const int mg = pos.mg[us] - pos.mg[them];
    const int eg = pos.eg[us] - pos.eg[them];
    const int phase = std::min(pos.phase, 24);
    return (mg * phase + eg * (24 - phase)) / 24;
}
