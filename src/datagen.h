#pragma once

#include "position.h"

// 32-byte training record. Pieces are listed in occupancy order (a1 first),
// two per byte, low nibble first, using the engine's piece codes (0-11).
#pragma pack(push, 1)
struct PackedPosition {
    u64 occupancy;
    u8 pieces[16];
    std::int16_t score; // search score, white's point of view, centipawns
    u8 result;          // 0 black won, 1 draw, 2 white won
    u8 stm;             // side to move
    u8 pad[4];
};
#pragma pack(pop)
static_assert(sizeof(PackedPosition) == 32);

void run_datagen(const char* path, i64 targetPositions, int threads, int nodesPerMove);
