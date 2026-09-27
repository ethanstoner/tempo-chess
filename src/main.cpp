#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#include "datagen.h"
#include "eval.h"
#include "search.h"

namespace {

constexpr const char* ENGINE_NAME = "Tempo 0.2";

u64 perft(Position& pos, int depth) {
    MoveList list;
    pos.generate<GEN_ALL>(list);
    u64 total = 0;
    for (Move m : list) {
        if (!pos.is_legal(m)) continue;
        if (depth == 1) {
            total++;
            continue;
        }
        pos.do_move(m);
        total += perft(pos, depth - 1);
        pos.undo_move();
    }
    return total;
}

void perft_divide(Position& pos, int depth) {
    auto t0 = std::chrono::steady_clock::now();
    MoveList list;
    pos.generate<GEN_ALL>(list);
    u64 total = 0;
    for (Move m : list) {
        if (!pos.is_legal(m)) continue;
        u64 n = 1;
        if (depth > 1) {
            pos.do_move(m);
            n = perft(pos, depth - 1);
            pos.undo_move();
        }
        std::printf("%s: %llu\n", move_to_uci(m).c_str(), (unsigned long long)n);
        total += n;
    }
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    std::printf("\nNodes: %llu\nTime: %lld ms\nNPS: %llu\n", (unsigned long long)total, (long long)ms,
                (unsigned long long)(total * 1000 / (ms ? ms : 1)));
    std::fflush(stdout);
}

// Fixed-depth search over a spread of middlegame and endgame positions. The
// node total is a deterministic signature: any change to it means search
// behaviour changed.
void bench(Engine& search, int depth) {
    static const char* FENS[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
        "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
        "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
        "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
        "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4",
        "r2q1rk1/pp2bppp/2n1bn2/3p4/3P4/2NBBN2/PP3PPP/R2Q1RK1 w - - 0 11",
        "2r2rk1/1bqnbppp/p2ppn2/1p6/3NPP2/1BN1B3/PPPQ2PP/2KR3R w - - 0 14",
        "6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1",
        "8/8/4k3/8/2pP4/8/4K3/8 b - d3 0 1",
        "8/5pk1/6p1/8/8/3B2P1/5PK1/8 w - - 0 1",
        "4r1k1/1p3pp1/p1p4p/3n4/3P4/P1N2P2/1P3P1P/4R1K1 w - - 0 25",
        "r1b1k2r/ppppnppp/2n2q2/2b5/3NP3/2P1B3/PP3PPP/RN1QKB1R w KQkq - 0 7",
    };
    i64 total = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (const char* fen : FENS) {
        Position pos;
        if (!pos.set_fen(fen)) std::printf("bad bench fen: %s\n", fen);
        search.clear();
        Limits lim;
        lim.depth = depth;
        total += search.go(pos, lim, false).nodes;
    }
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    std::printf("Nodes: %lld\nNPS: %lld\nTime: %lld ms\n", (long long)total, (long long)(total * 1000 / (ms ? ms : 1)),
                (long long)ms);
    std::fflush(stdout);
}

void set_position(Position& pos, std::istringstream& in) {
    std::string token, fen;
    in >> token;
    if (token == "startpos") {
        fen = Position::START_FEN;
        in >> token;
    } else if (token == "fen") {
        while (in >> token && token != "moves") fen += token + " ";
    } else {
        return;
    }
    if (!pos.set_fen(fen)) {
        std::printf("info string invalid fen, using startpos\n");
        pos.set_fen(Position::START_FEN);
    }
    while (in >> token) {
        Move m = pos.parse_uci(token);
        if (m == NO_MOVE) break;
        pos.do_move(m);
    }
}

Limits parse_go(std::istringstream& in) {
    Limits lim;
    std::string token;
    while (in >> token) {
        if (token == "wtime") in >> lim.time[WHITE];
        else if (token == "btime") in >> lim.time[BLACK];
        else if (token == "winc") in >> lim.inc[WHITE];
        else if (token == "binc") in >> lim.inc[BLACK];
        else if (token == "movestogo") in >> lim.movestogo;
        else if (token == "depth") in >> lim.depth;
        else if (token == "nodes") in >> lim.nodes;
        else if (token == "movetime") in >> lim.movetime;
        else if (token == "infinite") lim.infinite = true;
    }
    lim.depth = std::clamp(lim.depth, 1, MAX_PLY - 1);
    return lim;
}

} // namespace

int main(int argc, char** argv) {
    init_bitboards();
    zobrist::init();
    init_eval();
    init_search();

    Engine search;
    Position pos;

    if (argc > 1 && std::string(argv[1]) == "datagen") {
        if (argc < 6) {
            std::printf("usage: tempo datagen <out.bin> <positions> <threads> <nodes-per-move> [hce|nnue]\n");
            return 1;
        }
        USE_NNUE = argc > 6 && std::string(argv[6]) == "nnue";
        std::printf("datagen with the %s evaluation\n", USE_NNUE ? "NNUE" : "hand-written");
        run_datagen(argv[2], std::atoll(argv[3]), std::atoi(argv[4]), std::atoi(argv[5]));
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "bench") {
        bench(search, argc > 2 ? std::atoi(argv[2]) : 10);
        return 0;
    }

    std::thread worker;
    auto join = [&] {
        if (worker.joinable()) worker.join();
    };
    auto stop_search = [&] {
        search.shared.stop = true;
        join();
    };

    std::string line;
    while (std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;

        if (cmd == "uci") {
            std::printf("id name %s\nid author Ethan Stoner\n", ENGINE_NAME);
            std::printf("option name Hash type spin default 64 min 1 max 4096\n");
            std::printf("option name Threads type spin default 1 min 1 max 128\n");
            std::printf("option name Move Overhead type spin default 50 min 0 max 5000\n");
            std::printf("option name UseNNUE type check default true\n");
            std::printf("uciok\n");
        } else if (cmd == "isready") {
            std::printf("readyok\n");
        } else if (cmd == "setoption") {
            std::string token, name, value;
            in >> token;
            while (in >> token && token != "value") name += (name.empty() ? "" : " ") + token;
            in >> value;
            stop_search();
            if (name == "Hash") search.shared.tt.resize(std::clamp(std::atoi(value.c_str()), 1, 4096));
            else if (name == "Threads") search.set_threads(std::clamp(std::atoi(value.c_str()), 1, 128));
            else if (name == "UseNNUE") USE_NNUE = value == "true";
            else if (name == "Move Overhead") search.moveOverhead = std::max(0, std::atoi(value.c_str()));
        } else if (cmd == "ucinewgame") {
            stop_search();
            search.clear();
        } else if (cmd == "position") {
            stop_search();
            set_position(pos, in);
        } else if (cmd == "go") {
            stop_search();
            std::string first;
            std::istringstream peek(line.substr(2));
            peek >> first;
            if (first == "perft") {
                int depth = 1;
                peek >> depth;
                perft_divide(pos, depth);
                continue;
            }
            Limits lim = parse_go(in);
            worker = std::thread([&search, lim, copy = pos]() mutable {
                SearchResult r = search.go(copy, lim, true);
                // UCI forbids sending bestmove during "go infinite" until told to stop.
                while (lim.infinite && !search.shared.stop) std::this_thread::sleep_for(std::chrono::milliseconds(5));
                std::printf("bestmove %s\n", move_to_uci(r.best).c_str());
                std::fflush(stdout);
            });
        } else if (cmd == "stop") {
            stop_search();
        } else if (cmd == "quit") {
            stop_search();
            break;
        } else if (cmd == "d") {
            std::printf("%s\nkey %016llx eval %d\n", pos.fen().c_str(), (unsigned long long)pos.hash(), evaluate(pos));
        } else if (cmd == "bench") {
            int depth = 10;
            in >> depth;
            stop_search();
            bench(search, depth);
        }
        std::fflush(stdout);
    }
    join();
    return 0;
}
