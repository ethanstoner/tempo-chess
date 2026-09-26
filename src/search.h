#pragma once

#include <atomic>
#include <chrono>
#include <vector>

#include "position.h"
#include "tt.h"

struct Limits {
    int depth = MAX_PLY - 1;
    i64 nodes = 0;
    i64 movetime = 0;
    i64 time[2] = {0, 0};
    i64 inc[2] = {0, 0};
    int movestogo = 0;
    bool infinite = false;
};

struct SearchResult {
    Move best = NO_MOVE;
    int score = 0;
    int depth = 0;
    i64 nodes = 0;
};

class Search {
public:
    SearchResult go(Position& pos, const Limits& limits, bool verbose);
    void clear();
    int quiet_score(Position& pos); // quiescence score, side to move's view

    std::atomic<bool> stop{false};
    TranspositionTable tt;
    int moveOverhead = 50;

private:
    int negamax(Position& pos, int alpha, int beta, int depth, int ply, bool doNull);
    int qsearch(Position& pos, int alpha, int beta, int ply);
    void score_moves(const Position& pos, MoveList& list, int* scores, Move ttMove, int ply) const;
    void update_quiet(const Position& pos, int ply, Move m, int bonus);
    int quiet_score(const Position& pos, int ply, Move m) const;
    int* cont_entry(int ply, int back);
    const int* cont_entry(int ply, int back) const;
    void check_limits();
    i64 elapsed() const;

    Limits limits;
    std::chrono::steady_clock::time_point start;
    i64 softLimit = 0, hardLimit = 0;
    i64 nodes = 0;
    int seldepth = 0;
    int rootDepth = 0;
    Move rootBest = NO_MOVE;

    Move killers[MAX_PLY][2] = {};
    int history[2][64][64] = {};
    // Continuation history: [previous piece][previous to][piece][to], so a
    // quiet move is scored by how it fared as a reply to the moves before it.
    std::vector<int> contHist = std::vector<int>(12 * 64 * 12 * 64, 0);
    int movedPiece[MAX_PLY + 1] = {};
    int movedTo[MAX_PLY + 1] = {};
    int staticEval[MAX_PLY] = {};
    Move pv[MAX_PLY][MAX_PLY] = {};
    int pvLen[MAX_PLY] = {};
};

void init_search();
