#pragma once

#include <cstdint>
#include <string>

using u64 = std::uint64_t;
using u32 = std::uint32_t;
using u16 = std::uint16_t;
using u8 = std::uint8_t;
using i64 = std::int64_t;
using Bitboard = u64;

enum Color : int { WHITE, BLACK };
enum PieceType : int { PAWN, KNIGHT, BISHOP, ROOK, QUEEN, KING, NO_PIECE_TYPE };

// Piece = color * 6 + type, so 0..5 are white and 6..11 black.
constexpr int NO_PIECE = 12;
constexpr int NO_SQ = 64;

constexpr int make_piece(int c, int pt) { return c * 6 + pt; }
constexpr int type_of(int pc) { return pc % 6; }
constexpr int color_of(int pc) { return pc / 6; }

// Squares run a1 = 0 .. h8 = 63.
constexpr int file_of(int sq) { return sq & 7; }
constexpr int rank_of(int sq) { return sq >> 3; }
constexpr int make_sq(int file, int rank) { return rank * 8 + file; }
constexpr int relative_rank(int c, int sq) { return c == WHITE ? rank_of(sq) : 7 - rank_of(sq); }

enum Castling : int { WK = 1, WQ = 2, BK = 4, BQ = 8 };

// Move: bits 0-5 from, 6-11 to, 12-15 flag.
using Move = u16;
constexpr Move NO_MOVE = 0;

enum MoveFlag : int {
    QUIET = 0,
    DOUBLE_PUSH = 1,
    KING_CASTLE = 2,
    QUEEN_CASTLE = 3,
    CAPTURE = 4,
    EP_CAPTURE = 5,
    PROMO = 8,          // + 0..3 for N, B, R, Q
    PROMO_CAPTURE = 12, // + 0..3 for N, B, R, Q
};

constexpr Move make_move(int from, int to, int flag = QUIET) { return Move(from | (to << 6) | (flag << 12)); }
constexpr int from_sq(Move m) { return m & 63; }
constexpr int to_sq(Move m) { return (m >> 6) & 63; }
constexpr int flag_of(Move m) { return m >> 12; }
constexpr bool is_capture(Move m) { return flag_of(m) & CAPTURE; }
constexpr bool is_promo(Move m) { return flag_of(m) & PROMO; }
constexpr int promo_type(Move m) { return KNIGHT + (flag_of(m) & 3); }
constexpr bool is_castle(Move m) { return flag_of(m) == KING_CASTLE || flag_of(m) == QUEEN_CASTLE; }
constexpr bool is_quiet(Move m) { return !is_capture(m) && !is_promo(m); }

inline std::string sq_name(int sq) {
    return std::string{char('a' + file_of(sq)), char('1' + rank_of(sq))};
}

inline std::string move_to_uci(Move m) {
    if (m == NO_MOVE) return "0000";
    std::string s = sq_name(from_sq(m)) + sq_name(to_sq(m));
    if (is_promo(m)) s += "nbrq"[promo_type(m) - KNIGHT];
    return s;
}

constexpr int MAX_PLY = 128;
constexpr int INF = 32001;
constexpr int MATE = 32000;
constexpr int MATE_BOUND = MATE - MAX_PLY;
