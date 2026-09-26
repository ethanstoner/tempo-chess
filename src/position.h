#pragma once

#include <string>
#include <vector>

#include "bitboard.h"
#include "types.h"

struct MoveList {
    Move moves[256];
    int size = 0;
    void add(Move m) { moves[size++] = m; }
    Move* begin() { return moves; }
    Move* end() { return moves + size; }
};

// Everything do_move can't recompute on undo.
struct Undo {
    u64 key;
    int castling;
    int ep;
    int halfmove;
    int captured;
    Move move;
};

enum GenType { GEN_ALL, GEN_NOISY };

class Position {
public:
    static constexpr const char* START_FEN = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    Position() { set_fen(START_FEN); }
    bool set_fen(const std::string& fen);
    std::string fen() const;

    void do_move(Move m);
    void undo_move();
    void do_null();
    void undo_null();

    template <GenType T>
    void generate(MoveList& list) const;
    bool is_legal(Move m) const;
    bool is_pseudo_legal(Move m) const;
    Move parse_uci(const std::string& s) const;

    Bitboard attackers_to(int sq, Bitboard occ) const;
    bool attacked(int sq, int by) const { return attackers_to(sq, occupied()) & colors[by]; }
    bool in_check() const { return checkers() != 0; }
    Bitboard checkers() const { return attackers_to(king_sq(side), occupied()) & colors[side ^ 1]; }
    int see(Move m) const;
    bool see_ge(Move m, int threshold) const { return see(m) >= threshold; }

    bool is_repetition() const;
    bool is_draw(int ply) const;
    bool insufficient_material() const;
    bool has_non_pawn(int c) const {
        return colors[c] & (pieces[KNIGHT] | pieces[BISHOP] | pieces[ROOK] | pieces[QUEEN]);
    }

    Bitboard occupied() const { return colors[WHITE] | colors[BLACK]; }
    Bitboard pcs(int c, int pt) const { return colors[c] & pieces[pt]; }
    int king_sq(int c) const { return lsb(pcs(c, KING)); }
    int piece_on(int sq) const { return board[sq]; }
    int side_to_move() const { return side; }
    u64 hash() const { return key; }
    int halfmove_clock() const { return halfmove; }
    int game_ply() const { return int(history.size()); }
    Move last_move() const { return history.empty() ? NO_MOVE : history.back().move; }

    u64 compute_key() const;

private:
    void put_piece(int pc, int sq);
    void remove_piece(int sq);
    void move_piece(int from, int to);
    Bitboard pinned(int c) const;

    Bitboard pieces[6] = {};
    Bitboard colors[2] = {};
    int board[64];
    int side = WHITE;
    int castling = 0;
    int ep = NO_SQ;
    int halfmove = 0;
    int fullmove = 1;
    u64 key = 0;
    std::vector<Undo> history;
};

namespace zobrist {
extern u64 psq[12][64];
extern u64 castle[16];
extern u64 ep_file[8];
extern u64 side;
void init();
} // namespace zobrist

extern const int PHASE_INC[6];
extern const int SEE_VALUE[7];
