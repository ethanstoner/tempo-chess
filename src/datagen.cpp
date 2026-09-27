// Self-play data generation for NNUE training.
//
// Each thread plays games from a few random opening plies, searching every
// move to a fixed node budget. Quiet positions (not in check, best move not a
// capture or promotion) are recorded with the search score, and every record
// is stamped with the game's final result once it is known.

#include "datagen.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "search.h"

namespace {

constexpr int ADJUDICATE_SCORE = 2500; // |score| held this long ends the game
constexpr int ADJUDICATE_PLIES = 8;
constexpr int MAX_GAME_PLIES = 400;
constexpr int RANDOM_PLIES = 8;

PackedPosition pack(const Position& pos, int score) {
    PackedPosition p{};
    const Bitboard occ = pos.occupied();
    p.occupancy = occ;
    int i = 0;
    for (Bitboard b = occ; b; i++) {
        const int pc = pos.piece_on(pop_lsb(b));
        p.pieces[i / 2] |= u8(pc << (4 * (i % 2)));
    }
    const int white = pos.side_to_move() == WHITE ? score : -score;
    p.score = std::int16_t(std::clamp(white, -32000, 32000));
    p.stm = u8(pos.side_to_move());
    return p;
}

bool play_random_opening(Position& pos, std::mt19937_64& rng) {
    pos.set_fen(Position::START_FEN);
    for (int i = 0; i < RANDOM_PLIES; i++) {
        MoveList list, legal;
        pos.generate<GEN_ALL>(list);
        for (Move m : list)
            if (pos.is_legal(m)) legal.add(m);
        if (!legal.size) return false;
        pos.do_move(legal.moves[rng() % legal.size]);
    }
    return true;
}

bool has_legal_move(Position& pos) {
    MoveList list;
    pos.generate<GEN_ALL>(list);
    for (Move m : list)
        if (pos.is_legal(m)) return true;
    return false;
}

} // namespace

void run_datagen(const char* path, i64 targetPositions, int threads, int nodesPerMove) {
    FILE* out = std::fopen(path, "ab");
    if (!out) {
        std::printf("cannot open %s\n", path);
        return;
    }
    std::mutex outMutex;
    std::atomic<i64> written{0}, games{0};
    const auto t0 = std::chrono::steady_clock::now();

    auto worker = [&](int id) {
        std::mt19937_64 rng(0x9E3779B97F4A7C15ULL * (id + 1) ^ u64(t0.time_since_epoch().count()));
        Search search;
        search.table().resize(16);
        Limits limits;
        limits.nodes = nodesPerMove;
        std::vector<PackedPosition> game;

        while (written.load() < targetPositions) {
            Position pos;
            if (!play_random_opening(pos, rng)) continue;
            search.clear();
            game.clear();
            int result = 1; // 0 black wins, 1 draw, 2 white wins
            int decisiveStreak = 0;

            for (int ply = 0; ply < MAX_GAME_PLIES; ply++) {
                if (!has_legal_move(pos)) {
                    if (pos.in_check()) result = pos.side_to_move() == WHITE ? 0 : 2;
                    break;
                }
                if (pos.is_draw(1)) break;

                SearchResult r = search.go(pos, limits, false);
                const int whiteScore = pos.side_to_move() == WHITE ? r.score : -r.score;
                if (std::abs(r.score) >= ADJUDICATE_SCORE) {
                    if (++decisiveStreak >= ADJUDICATE_PLIES) {
                        result = whiteScore > 0 ? 2 : 0;
                        break;
                    }
                } else {
                    decisiveStreak = 0;
                }
                if (!pos.in_check() && is_quiet(r.best) && std::abs(r.score) < MATE_BOUND)
                    game.push_back(pack(pos, r.score));
                pos.do_move(r.best);
            }

            for (auto& p : game) p.result = u8(result);
            {
                std::lock_guard<std::mutex> lock(outMutex);
                std::fwrite(game.data(), sizeof(PackedPosition), game.size(), out);
            }
            const i64 total = written += i64(game.size());
            const i64 g = ++games;
            if (id == 0 && g % 50 == 0) {
                const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
                std::printf("%lld games, %lld positions, %.0f pos/s\n", (long long)g, (long long)total, total / secs);
                std::fflush(stdout);
            }
        }
    };

    std::vector<std::thread> pool;
    for (int i = 0; i < threads; i++) pool.emplace_back(worker, i);
    for (auto& t : pool) t.join();
    std::fclose(out);
    std::printf("done: %lld games, %lld positions -> %s\n", (long long)games.load(), (long long)written.load(), path);
}
