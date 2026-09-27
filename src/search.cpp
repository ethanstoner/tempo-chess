#include "search.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "eval.h"

namespace {

int LMR[64][64];

constexpr int SCORE_TT = 30'000'000;
constexpr int SCORE_GOOD_NOISY = 20'000'000;
constexpr int SCORE_KILLER = 15'000'000;
constexpr int SCORE_BAD_NOISY = -20'000'000;
constexpr int HISTORY_MAX = 16384;

int mvv_lva(const Position& pos, Move m) {
    int victim = flag_of(m) == EP_CAPTURE ? PAWN : is_capture(m) ? type_of(pos.piece_on(to_sq(m))) : NO_PIECE_TYPE;
    int value = victim == NO_PIECE_TYPE ? 0 : SEE_VALUE[victim] * 16;
    if (is_promo(m)) value += SEE_VALUE[promo_type(m)] * 16;
    return value - type_of(pos.piece_on(from_sq(m)));
}

Move pick_next(MoveList& list, int* scores, int i) {
    int best = i;
    for (int j = i + 1; j < list.size; j++)
        if (scores[j] > scores[best]) best = j;
    std::swap(list.moves[i], list.moves[best]);
    std::swap(scores[i], scores[best]);
    return list.moves[i];
}

std::string score_string(int score) {
    if (score >= MATE_BOUND) return "mate " + std::to_string((MATE - score + 1) / 2);
    if (score <= -MATE_BOUND) return "mate " + std::to_string(-(MATE + score) / 2);
    return "cp " + std::to_string(score);
}

} // namespace

void init_search() {
    for (int d = 1; d < 64; d++)
        for (int m = 1; m < 64; m++) LMR[d][m] = int(0.75 + std::log(d) * std::log(m) / 2.25);
}

void Search::clear() {
    tt.clear();
    std::memset(history, 0, sizeof(history));
    std::memset(killers, 0, sizeof(killers));
    std::fill(contHist.begin(), contHist.end(), 0);
}

i64 Search::elapsed() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
}

void Search::check_limits() {
    if (rootDepth <= 1) return; // always finish depth 1 so there is a move to play
    if (limits.nodes && nodes >= limits.nodes) stop = true;
    if (hardLimit && elapsed() >= hardLimit) stop = true;
}

namespace {
void gravity(int& h, int bonus) { h += bonus - h * std::abs(bonus) / HISTORY_MAX; }
} // namespace

// Table row for the move made `back` plies before this node, or null when
// that ply was the root's parent or a null move.
const int* Search::cont_entry(int ply, int back) const {
    if (ply < back || movedPiece[ply - back] == NO_PIECE) return nullptr;
    return &contHist[(movedPiece[ply - back] * 64 + movedTo[ply - back]) * 12 * 64];
}
int* Search::cont_entry(int ply, int back) {
    return const_cast<int*>(static_cast<const Search*>(this)->cont_entry(ply, back));
}

int Search::quiet_score(const Position& pos, int ply, Move m) const {
    const int idx = pos.piece_on(from_sq(m)) * 64 + to_sq(m);
    int s = history[pos.side_to_move()][from_sq(m)][to_sq(m)];
    for (int back = 1; back <= 2; back++)
        if (const int* c = cont_entry(ply, back)) s += c[idx];
    return s;
}

void Search::update_quiet(const Position& pos, int ply, Move m, int bonus) {
    gravity(history[pos.side_to_move()][from_sq(m)][to_sq(m)], bonus);
    const int idx = pos.piece_on(from_sq(m)) * 64 + to_sq(m);
    for (int back = 1; back <= 2; back++)
        if (int* c = cont_entry(ply, back)) gravity(c[idx], bonus);
}

void Search::score_moves(const Position& pos, MoveList& list, int* scores, Move ttMove, int ply) const {
    for (int i = 0; i < list.size; i++) {
        Move m = list.moves[i];
        if (m == ttMove) scores[i] = SCORE_TT;
        else if (!is_quiet(m)) scores[i] = (pos.see_ge(m, 0) ? SCORE_GOOD_NOISY : SCORE_BAD_NOISY) + mvv_lva(pos, m);
        else if (m == killers[ply][0]) scores[i] = SCORE_KILLER;
        else if (m == killers[ply][1]) scores[i] = SCORE_KILLER - 1;
        else scores[i] = quiet_score(pos, ply, m);
    }
}

int Search::qsearch(Position& pos, int alpha, int beta, int ply) {
    pvLen[ply] = ply;
    if ((nodes & 2047) == 0) check_limits();
    if (stop) return 0;
    nodes++;
    seldepth = std::max(seldepth, ply);

    if (pos.is_draw(ply)) return 0;
    const bool inCheck = pos.in_check();
    if (ply >= MAX_PLY - 1) return inCheck ? 0 : evaluate(pos);

    const TTEntry* e = tt.probe(pos.hash());
    Move ttMove = e ? e->move : NO_MOVE;
    if (e) {
        int s = score_from_tt(e->score, ply);
        int f = e->flag & 3;
        if (f == TT_EXACT || (f == TT_LOWER && s >= beta) || (f == TT_UPPER && s <= alpha)) return s;
    }

    int best, eval = -INF;
    if (inCheck) {
        best = -MATE + ply;
    } else {
        eval = e ? e->eval : evaluate(pos);
        best = eval;
        if (best >= beta) return best;
        alpha = std::max(alpha, best);
    }

    MoveList list;
    if (inCheck) pos.generate<GEN_ALL>(list);
    else pos.generate<GEN_NOISY>(list);
    int scores[256];
    score_moves(pos, list, scores, ttMove, ply);

    const int origAlpha = alpha;
    Move bestMove = NO_MOVE;
    for (int i = 0; i < list.size; i++) {
        Move m = pick_next(list, scores, i);
        if (!pos.is_legal(m)) continue;
        if (!inCheck && scores[i] < SCORE_GOOD_NOISY) break; // remaining noisy moves all lose material

        pos.do_move(m);
        int score = -qsearch(pos, -beta, -alpha, ply + 1);
        pos.undo_move();
        if (stop) return 0;

        if (score > best) {
            best = score;
            if (score > alpha) {
                alpha = score;
                bestMove = m;
                if (alpha >= beta) break;
            }
        }
    }

    TTFlag flag = best >= beta ? TT_LOWER : best > origAlpha ? TT_EXACT : TT_UPPER;
    tt.store(pos.hash(), bestMove, score_to_tt(best, ply), eval, 0, flag);
    return best;
}

int Search::negamax(Position& pos, int alpha, int beta, int depth, int ply, bool doNull) {
    pvLen[ply] = ply;
    const bool root = ply == 0;
    const bool pvNode = beta - alpha > 1;
    const bool inCheck = pos.in_check();
    if (inCheck) depth++;
    if (depth <= 0) return qsearch(pos, alpha, beta, ply);

    if ((nodes & 2047) == 0) check_limits();
    if (stop) return 0;
    nodes++;
    seldepth = std::max(seldepth, ply);

    if (!root) {
        if (pos.is_draw(ply)) return 0;
        if (ply >= MAX_PLY - 1) return inCheck ? 0 : evaluate(pos);
        alpha = std::max(alpha, -MATE + ply);
        beta = std::min(beta, MATE - ply - 1);
        if (alpha >= beta) return alpha;
    }

    const TTEntry* e = tt.probe(pos.hash());
    const Move ttMove = e ? e->move : NO_MOVE;
    if (e && !pvNode && e->depth >= depth) {
        int s = score_from_tt(e->score, ply);
        int f = e->flag & 3;
        if (f == TT_EXACT || (f == TT_LOWER && s >= beta) || (f == TT_UPPER && s <= alpha)) return s;
    }

    const int us = pos.side_to_move();
    int eval = -INF;
    if (!inCheck) eval = e ? e->eval : evaluate(pos);
    staticEval[ply] = eval;
    const bool improving = !inCheck && ply >= 2 && eval > staticEval[ply - 2];

    if (!pvNode && !inCheck) {
        // Reverse futility: far enough above beta that a quiet move won't drop us below it.
        if (depth <= 8 && eval - (80 - 20 * improving) * depth >= beta && std::abs(beta) < MATE_BOUND) return eval;

        if (doNull && depth >= 3 && eval >= beta && pos.has_non_pawn(us)) {
            int R = 3 + depth / 3 + std::min((eval - beta) / 200, 3);
            movedPiece[ply] = NO_PIECE;
            pos.do_null();
            int score = -negamax(pos, -beta, -beta + 1, depth - 1 - R, ply + 1, false);
            pos.undo_null();
            if (stop) return 0;
            if (score >= beta) return score >= MATE_BOUND ? beta : score;
        }
    }

    // Without a hash move the ordering is poor; search shallower to get one.
    if (depth >= 4 && ttMove == NO_MOVE) depth--;

    MoveList list;
    pos.generate<GEN_ALL>(list);
    int scores[256];
    score_moves(pos, list, scores, ttMove, ply);

    const int origAlpha = alpha;
    int best = -INF, legal = 0, quietCount = 0;
    Move bestMove = NO_MOVE;
    Move quiets[256];

    for (int i = 0; i < list.size; i++) {
        Move m = pick_next(list, scores, i);
        if (!pos.is_legal(m)) continue;
        const bool quiet = is_quiet(m);

        if (!root && best > -MATE_BOUND) {
            if (quiet && !inCheck && !pvNode) {
                if (depth <= 6 && quietCount >= (3 + depth * depth) / (2 - improving)) continue;
                if (depth <= 6 && eval + 100 + 90 * depth <= alpha) continue;
            }
            if (depth <= 8 && !pos.see_ge(m, quiet ? -60 * depth : -25 * depth * depth)) continue;
        }

        legal++;
        movedPiece[ply] = pos.piece_on(from_sq(m));
        movedTo[ply] = to_sq(m);
        pos.do_move(m);
        const bool givesCheck = pos.in_check();
        const int newDepth = depth - 1;
        int score;

        if (legal == 1) {
            score = -negamax(pos, -beta, -alpha, newDepth, ply + 1, true);
        } else {
            int R = 0;
            if (depth >= 3 && legal > 1 + pvNode && quiet) {
                R = LMR[std::min(depth, 63)][std::min(legal, 63)];
                R -= pvNode;
                R += !improving;
                R -= givesCheck;
                R -= m == killers[ply][0] || m == killers[ply][1];
                R -= history[us][from_sq(m)][to_sq(m)] / 8192;
                R = std::clamp(R, 0, newDepth - 1);
            }
            score = -negamax(pos, -alpha - 1, -alpha, newDepth - R, ply + 1, true);
            if (score > alpha && R > 0) score = -negamax(pos, -alpha - 1, -alpha, newDepth, ply + 1, true);
            if (score > alpha && score < beta) score = -negamax(pos, -beta, -alpha, newDepth, ply + 1, true);
        }
        pos.undo_move();
        if (stop) return 0;

        if (score > best) {
            best = score;
            if (score > alpha) {
                alpha = score;
                bestMove = m;
                pv[ply][ply] = m;
                for (int j = ply + 1; j < pvLen[ply + 1]; j++) pv[ply][j] = pv[ply + 1][j];
                pvLen[ply] = std::max(pvLen[ply + 1], ply + 1);
                if (root) rootBest = m;

                if (alpha >= beta) {
                    if (quiet) {
                        if (killers[ply][0] != m) {
                            killers[ply][1] = killers[ply][0];
                            killers[ply][0] = m;
                        }
                        int bonus = std::min(depth * depth * 8, 1600);
                        update_quiet(pos, ply, m, bonus);
                        for (int q = 0; q < quietCount; q++) update_quiet(pos, ply, quiets[q], -bonus);
                    }
                    break;
                }
            }
        }
        if (quiet) quiets[quietCount++] = m;
    }

    if (legal == 0) return inCheck ? -MATE + ply : 0;

    TTFlag flag = best >= beta ? TT_LOWER : best > origAlpha ? TT_EXACT : TT_UPPER;
    tt.store(pos.hash(), bestMove ? bestMove : ttMove, score_to_tt(best, ply), eval, depth, flag);
    return best;
}

SearchResult Search::go(Position& pos, const Limits& lim, bool verbose) {
    limits = lim;
    start = std::chrono::steady_clock::now();
    stop = false;
    nodes = 0;
    rootBest = NO_MOVE;
    tt.new_search();
    std::memset(killers, 0, sizeof(killers));

    softLimit = hardLimit = 0;
    const int us = pos.side_to_move();
    if (limits.movetime) {
        softLimit = hardLimit = std::max<i64>(1, limits.movetime - moveOverhead);
    } else if (limits.time[us] && !limits.infinite) {
        const i64 t = std::max<i64>(1, limits.time[us] - moveOverhead);
        const i64 inc = limits.inc[us];
        const int mtg = limits.movestogo ? std::min(limits.movestogo, 40) : 40;
        const i64 optimum = t / mtg + inc * 4 / 5;
        // Each iteration costs roughly twice the previous one, so a new
        // iteration only starts in the first half of the budget; the hard
        // limit bounds the overshoot and never exceeds 1/8 of the clock.
        hardLimit = std::min({optimum * 3, t / 8 + inc / 2, t / 2});
        hardLimit = std::max<i64>(1, hardLimit);
        softLimit = std::min(optimum / 2, hardLimit);
    }

    SearchResult result;
    int prevScore = 0;
    for (rootDepth = 1; rootDepth <= limits.depth; rootDepth++) {
        seldepth = 0;
        int delta = 25, alpha = -INF, beta = INF, score;
        if (rootDepth >= 5) {
            alpha = std::max(-INF, prevScore - delta);
            beta = std::min(INF, prevScore + delta);
        }
        while (true) {
            score = negamax(pos, alpha, beta, rootDepth, 0, true);
            if (stop) break;
            if (score <= alpha) {
                beta = (alpha + beta) / 2;
                alpha = std::max(-INF, score - delta);
            } else if (score >= beta) {
                beta = std::min(INF, score + delta);
            } else {
                break;
            }
            delta += delta / 2;
        }

        if (rootBest != NO_MOVE) result.best = rootBest;
        if (stop && rootDepth > 1) break;

        prevScore = score;
        result.score = score;
        result.depth = rootDepth;
        result.nodes = nodes;

        if (verbose) {
            i64 ms = elapsed();
            std::string line = "info depth " + std::to_string(rootDepth) + " seldepth " + std::to_string(seldepth) +
                               " score " + score_string(score) + " nodes " + std::to_string(nodes) + " nps " +
                               std::to_string(nodes * 1000 / std::max<i64>(ms, 1)) + " time " + std::to_string(ms) +
                               " hashfull " + std::to_string(tt.hashfull()) + " pv";
            for (int i = 0; i < pvLen[0]; i++) line += " " + move_to_uci(pv[0][i]);
            std::printf("%s\n", line.c_str());
            std::fflush(stdout);
        }

        if (softLimit && elapsed() >= softLimit) break;
        // A mate found with depth to spare won't get shorter; save the clock.
        if (!limits.infinite && std::abs(score) >= MATE_BOUND && rootDepth >= MATE - std::abs(score) + 8) break;
        if (limits.nodes && nodes >= limits.nodes) break;
    }

    if (result.best == NO_MOVE) {
        MoveList list;
        pos.generate<GEN_ALL>(list);
        for (Move m : list)
            if (pos.is_legal(m)) {
                result.best = m;
                break;
            }
    }
    result.nodes = nodes;
    return result;
}

int Search::quiet_score(Position& pos) {
    stop = false;
    limits = Limits{};
    hardLimit = 0;
    return qsearch(pos, -INF, INF, 0);
}
