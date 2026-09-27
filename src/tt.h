#pragma once

#include <algorithm>
#include <cstring>
#include <vector>

#include "types.h"

enum TTFlag : u8 { TT_NONE, TT_UPPER, TT_LOWER, TT_EXACT };

struct TTEntry {
    u64 key;
    Move move;
    std::int16_t score;
    std::int16_t eval;
    u8 depth;
    u8 flag; // low 2 bits TTFlag, high 6 bits generation
};

class TranspositionTable {
public:
    TranspositionTable() { resize(64); }

    void resize(size_t mb) {
        size_t count = mb * 1024 * 1024 / sizeof(TTEntry);
        size_t pow2 = 1;
        while (pow2 * 2 <= count) pow2 *= 2;
        table.assign(pow2, TTEntry{});
        mask = pow2 - 1;
    }
    void clear() { std::fill(table.begin(), table.end(), TTEntry{}); }
    void new_search() { generation = (generation + 1) & 63; }

    // Copies the entry out: other search threads may overwrite the slot.
    bool probe(u64 key, TTEntry& out) const {
        out = table[key & mask];
        return out.key == key && (out.flag & 3);
    }

    void store(u64 key, Move move, int score, int eval, int depth, TTFlag flag) {
        TTEntry& e = table[key & mask];
        const bool sameKey = e.key == key;
        if (!sameKey || flag == TT_EXACT || depth + 3 >= e.depth || (e.flag >> 2) != generation) {
            if (move != NO_MOVE || !sameKey) e.move = move;
            e.key = key;
            e.score = std::int16_t(score);
            e.eval = std::int16_t(eval);
            e.depth = u8(std::max(depth, 0));
            e.flag = u8(flag | (generation << 2));
        }
    }

    // Permille of a sample of entries written in the current search.
    int hashfull() const {
        int used = 0;
        for (size_t i = 0; i < 1000 && i < table.size(); i++)
            used += (table[i].flag & 3) && (table[i].flag >> 2) == generation;
        return used;
    }

private:
    std::vector<TTEntry> table;
    size_t mask = 0;
    u8 generation = 0;
};

// Mate scores are stored relative to the node so they stay valid when the
// same position is reached at a different ply.
inline int score_to_tt(int s, int ply) { return s >= MATE_BOUND ? s + ply : s <= -MATE_BOUND ? s - ply : s; }
inline int score_from_tt(int s, int ply) { return s >= MATE_BOUND ? s - ply : s <= -MATE_BOUND ? s + ply : s; }
