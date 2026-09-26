#include "position.h"

#include <algorithm>
#include <cstring>
#include <sstream>

#include "pesto_tables.h"

namespace zobrist {
u64 psq[12][64];
u64 castle[16];
u64 ep_file[8];
u64 side;

void init() {
    u64 s = 0x2545F4914F6CDD1DULL;
    auto next = [&] {
        s ^= s >> 12;
        s ^= s << 25;
        s ^= s >> 27;
        return s * 2685821657736338717ULL;
    };
    for (auto& pc : psq)
        for (auto& k : pc) k = next();
    for (auto& k : castle) k = next();
    for (auto& k : ep_file) k = next();
    side = next();
}
} // namespace zobrist

int MG_TABLE[12][64];
int EG_TABLE[12][64];
const int PHASE_INC[6] = {0, 1, 1, 2, 4, 0};
const int SEE_VALUE[7] = {100, 320, 330, 500, 900, 20000, 0};

void init_eval_tables() {
    for (int pt = PAWN; pt <= KING; pt++) {
        for (int sq = 0; sq < 64; sq++) {
            MG_TABLE[make_piece(WHITE, pt)][sq] = PESTO_MG_VALUE[pt] + PESTO_MG_PST[pt][sq ^ 56];
            EG_TABLE[make_piece(WHITE, pt)][sq] = PESTO_EG_VALUE[pt] + PESTO_EG_PST[pt][sq ^ 56];
            MG_TABLE[make_piece(BLACK, pt)][sq] = PESTO_MG_VALUE[pt] + PESTO_MG_PST[pt][sq];
            EG_TABLE[make_piece(BLACK, pt)][sq] = PESTO_EG_VALUE[pt] + PESTO_EG_PST[pt][sq];
        }
    }
}

namespace {

// Castling rights that survive a move touching each square.
int CASTLE_MASK[64];

struct CastleMaskInit {
    CastleMaskInit() {
        for (int& m : CASTLE_MASK) m = 15;
        CASTLE_MASK[0] &= ~WQ;
        CASTLE_MASK[7] &= ~WK;
        CASTLE_MASK[4] &= ~(WK | WQ);
        CASTLE_MASK[56] &= ~BQ;
        CASTLE_MASK[63] &= ~BK;
        CASTLE_MASK[60] &= ~(BK | BQ);
    }
} castle_mask_init;

constexpr const char* PIECE_CHARS = "PNBRQKpnbrqk";

} // namespace

void Position::put_piece(int pc, int sq) {
    board[sq] = pc;
    pieces[type_of(pc)] |= bb(sq);
    colors[color_of(pc)] |= bb(sq);
    key ^= zobrist::psq[pc][sq];
    mg[color_of(pc)] += MG_TABLE[pc][sq];
    eg[color_of(pc)] += EG_TABLE[pc][sq];
    phase += PHASE_INC[type_of(pc)];
}

void Position::remove_piece(int sq) {
    int pc = board[sq];
    board[sq] = NO_PIECE;
    pieces[type_of(pc)] ^= bb(sq);
    colors[color_of(pc)] ^= bb(sq);
    key ^= zobrist::psq[pc][sq];
    mg[color_of(pc)] -= MG_TABLE[pc][sq];
    eg[color_of(pc)] -= EG_TABLE[pc][sq];
    phase -= PHASE_INC[type_of(pc)];
}

void Position::move_piece(int from, int to) {
    int pc = board[from];
    Bitboard fromTo = bb(from) | bb(to);
    board[to] = pc;
    board[from] = NO_PIECE;
    pieces[type_of(pc)] ^= fromTo;
    colors[color_of(pc)] ^= fromTo;
    key ^= zobrist::psq[pc][from] ^ zobrist::psq[pc][to];
    mg[color_of(pc)] += MG_TABLE[pc][to] - MG_TABLE[pc][from];
    eg[color_of(pc)] += EG_TABLE[pc][to] - EG_TABLE[pc][from];
}

bool Position::set_fen(const std::string& fen) {
    std::istringstream in(fen);
    std::string placement, stm, castle, epStr;
    in >> placement >> stm >> castle >> epStr;
    if (placement.empty()) return false;

    std::fill(std::begin(board), std::end(board), NO_PIECE);
    std::fill(std::begin(pieces), std::end(pieces), 0);
    colors[WHITE] = colors[BLACK] = 0;
    mg[0] = mg[1] = eg[0] = eg[1] = phase = 0;
    key = 0;
    history.clear();

    int rank = 7, file = 0;
    for (char c : placement) {
        if (c == '/') {
            rank--;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
        } else {
            const char* p = std::strchr(PIECE_CHARS, c);
            if (!p || file > 7 || rank < 0) return false;
            put_piece(int(p - PIECE_CHARS), make_sq(file, rank));
            file++;
        }
    }
    if (popcount(pcs(WHITE, KING)) != 1 || popcount(pcs(BLACK, KING)) != 1) return false;

    side = stm == "b" ? BLACK : WHITE;
    castling = 0;
    for (char c : castle) {
        if (c == 'K') castling |= WK;
        if (c == 'Q') castling |= WQ;
        if (c == 'k') castling |= BK;
        if (c == 'q') castling |= BQ;
    }
    ep = NO_SQ;
    if (epStr.size() == 2) {
        int sq = make_sq(epStr[0] - 'a', epStr[1] - '1');
        if (PawnAttacks[side ^ 1][sq] & pcs(side, PAWN)) ep = sq;
    }
    halfmove = 0;
    fullmove = 1;
    in >> halfmove >> fullmove;

    if (side == BLACK) key ^= zobrist::side;
    key ^= zobrist::castle[castling];
    if (ep != NO_SQ) key ^= zobrist::ep_file[file_of(ep)];

    // The side that just moved can't be left in check.
    return !attacked(king_sq(side ^ 1), side);
}

std::string Position::fen() const {
    std::string s;
    for (int rank = 7; rank >= 0; rank--) {
        int empty = 0;
        for (int file = 0; file < 8; file++) {
            int pc = board[make_sq(file, rank)];
            if (pc == NO_PIECE) {
                empty++;
                continue;
            }
            if (empty) s += char('0' + empty), empty = 0;
            s += PIECE_CHARS[pc];
        }
        if (empty) s += char('0' + empty);
        if (rank) s += '/';
    }
    s += side == WHITE ? " w " : " b ";
    if (!castling) s += '-';
    if (castling & WK) s += 'K';
    if (castling & WQ) s += 'Q';
    if (castling & BK) s += 'k';
    if (castling & BQ) s += 'q';
    s += ' ';
    s += ep == NO_SQ ? "-" : sq_name(ep);
    s += ' ' + std::to_string(halfmove) + ' ' + std::to_string(fullmove);
    return s;
}

u64 Position::compute_key() const {
    u64 k = 0;
    for (int sq = 0; sq < 64; sq++)
        if (board[sq] != NO_PIECE) k ^= zobrist::psq[board[sq]][sq];
    if (side == BLACK) k ^= zobrist::side;
    k ^= zobrist::castle[castling];
    if (ep != NO_SQ) k ^= zobrist::ep_file[file_of(ep)];
    return k;
}

void Position::do_move(Move m) {
    history.push_back({key, castling, ep, halfmove, NO_PIECE, m, {mg[0], mg[1]}, {eg[0], eg[1]}, phase});
    Undo& u = history.back();

    const int us = side, them = side ^ 1;
    const int from = from_sq(m), to = to_sq(m), flag = flag_of(m);
    const int pc = board[from];

    if (ep != NO_SQ) key ^= zobrist::ep_file[file_of(ep)];
    ep = NO_SQ;
    halfmove++;

    if (flag == EP_CAPTURE) {
        int capSq = to + (us == WHITE ? -8 : 8);
        u.captured = board[capSq];
        remove_piece(capSq);
    } else if (is_capture(m)) {
        u.captured = board[to];
        remove_piece(to);
        halfmove = 0;
    }

    move_piece(from, to);
    if (flag == KING_CASTLE) move_piece(to + 1, to - 1);
    else if (flag == QUEEN_CASTLE) move_piece(to - 2, to + 1);

    if (type_of(pc) == PAWN) {
        halfmove = 0;
        if (is_promo(m)) {
            remove_piece(to);
            put_piece(make_piece(us, promo_type(m)), to);
        } else if (flag == DOUBLE_PUSH) {
            int epSq = from + (us == WHITE ? 8 : -8);
            if (PawnAttacks[us][epSq] & pcs(them, PAWN)) {
                ep = epSq;
                key ^= zobrist::ep_file[file_of(ep)];
            }
        }
    }

    key ^= zobrist::castle[castling];
    castling &= CASTLE_MASK[from] & CASTLE_MASK[to];
    key ^= zobrist::castle[castling];

    if (us == BLACK) fullmove++;
    side = them;
    key ^= zobrist::side;
}

void Position::undo_move() {
    const Undo u = history.back();
    history.pop_back();

    side ^= 1;
    const int us = side;
    const Move m = u.move;
    const int from = from_sq(m), to = to_sq(m), flag = flag_of(m);

    if (is_promo(m)) {
        remove_piece(to);
        put_piece(make_piece(us, PAWN), to);
    }
    move_piece(to, from);
    if (flag == KING_CASTLE) move_piece(to - 1, to + 1);
    else if (flag == QUEEN_CASTLE) move_piece(to + 1, to - 2);

    if (u.captured != NO_PIECE) put_piece(u.captured, flag == EP_CAPTURE ? to + (us == WHITE ? -8 : 8) : to);

    if (us == BLACK) fullmove--;
    key = u.key;
    castling = u.castling;
    ep = u.ep;
    halfmove = u.halfmove;
    mg[0] = u.mg[0], mg[1] = u.mg[1];
    eg[0] = u.eg[0], eg[1] = u.eg[1];
    phase = u.phase;
}

// Repetition detection never looks back past a null move: halfmove is reset
// to zero there, which bounds the scan.
void Position::do_null() {
    history.push_back({key, castling, ep, halfmove, NO_PIECE, NO_MOVE, {mg[0], mg[1]}, {eg[0], eg[1]}, phase});
    if (ep != NO_SQ) key ^= zobrist::ep_file[file_of(ep)];
    ep = NO_SQ;
    halfmove = 0;
    side ^= 1;
    key ^= zobrist::side;
}

void Position::undo_null() {
    const Undo& u = history.back();
    key = u.key;
    ep = u.ep;
    halfmove = u.halfmove;
    side ^= 1;
    history.pop_back();
}

Bitboard Position::attackers_to(int sq, Bitboard occ) const {
    return (PawnAttacks[BLACK][sq] & pcs(WHITE, PAWN)) | (PawnAttacks[WHITE][sq] & pcs(BLACK, PAWN)) |
           (KnightAttacks[sq] & pieces[KNIGHT]) | (KingAttacks[sq] & pieces[KING]) |
           (bishop_attacks(sq, occ) & (pieces[BISHOP] | pieces[QUEEN])) |
           (rook_attacks(sq, occ) & (pieces[ROOK] | pieces[QUEEN]));
}

Bitboard Position::pinned(int c) const {
    const int ksq = king_sq(c), them = c ^ 1;
    Bitboard snipers = (rook_attacks(ksq, 0) & (pcs(them, ROOK) | pcs(them, QUEEN))) |
                       (bishop_attacks(ksq, 0) & (pcs(them, BISHOP) | pcs(them, QUEEN)));
    Bitboard result = 0, occ = occupied();
    while (snipers) {
        Bitboard between = Between[ksq][pop_lsb(snipers)] & occ;
        if (between && !more_than_one(between)) result |= between & colors[c];
    }
    return result;
}

template <GenType T>
void Position::generate(MoveList& list) const {
    const int us = side, them = side ^ 1;
    const Bitboard occ = occupied(), enemy = colors[them], empty = ~occ;
    const Bitboard targets = T == GEN_ALL ? ~colors[us] : enemy;
    const int up = us == WHITE ? 8 : -8;
    const Bitboard promoFrom = us == WHITE ? RANK_7 : RANK_2;
    const Bitboard thirdRank = us == WHITE ? RANK_1 << 16 : RANK_1 << 40;
    auto shift_up = [us](Bitboard b) { return us == WHITE ? b << 8 : b >> 8; };

    const Bitboard pawns = pcs(us, PAWN);
    const Bitboard normal = pawns & ~promoFrom;

    if (T == GEN_ALL) {
        Bitboard single = shift_up(normal) & empty;
        Bitboard dbl = shift_up(single & thirdRank) & empty;
        while (single) {
            int to = pop_lsb(single);
            list.add(make_move(to - up, to));
        }
        while (dbl) {
            int to = pop_lsb(dbl);
            list.add(make_move(to - 2 * up, to, DOUBLE_PUSH));
        }
    }

    for (Bitboard b = normal; b;) {
        int from = pop_lsb(b);
        for (Bitboard att = PawnAttacks[us][from] & enemy; att;) list.add(make_move(from, pop_lsb(att), CAPTURE));
    }
    if (ep != NO_SQ)
        for (Bitboard b = PawnAttacks[them][ep] & normal; b;) list.add(make_move(pop_lsb(b), ep, EP_CAPTURE));

    for (Bitboard b = pawns & promoFrom; b;) {
        int from = pop_lsb(b);
        auto add_promos = [&](int to, int base) {
            list.add(make_move(from, to, base + 3));
            if (T == GEN_ALL)
                for (int p = 0; p < 3; p++) list.add(make_move(from, to, base + p));
        };
        if (empty & bb(from + up)) add_promos(from + up, PROMO);
        for (Bitboard att = PawnAttacks[us][from] & enemy; att;) add_promos(pop_lsb(att), PROMO_CAPTURE);
    }

    for (int pt = KNIGHT; pt <= KING; pt++) {
        for (Bitboard b = pcs(us, pt); b;) {
            int from = pop_lsb(b);
            for (Bitboard att = attacks_of(pt, from, occ) & targets; att;) {
                int to = pop_lsb(att);
                list.add(make_move(from, to, (enemy & bb(to)) ? CAPTURE : QUIET));
            }
        }
    }

    if (T == GEN_ALL && castling) {
        const int ksq = us == WHITE ? 4 : 60;
        const int kRight = us == WHITE ? WK : BK, qRight = us == WHITE ? WQ : BQ;
        if ((castling & (kRight | qRight)) && !attacked(ksq, them)) {
            if ((castling & kRight) && !(occ & (bb(ksq + 1) | bb(ksq + 2))) && !attacked(ksq + 1, them))
                list.add(make_move(ksq, ksq + 2, KING_CASTLE));
            if ((castling & qRight) && !(occ & (bb(ksq - 1) | bb(ksq - 2) | bb(ksq - 3))) &&
                !attacked(ksq - 1, them))
                list.add(make_move(ksq, ksq - 2, QUEEN_CASTLE));
        }
    }
}

template void Position::generate<GEN_ALL>(MoveList&) const;
template void Position::generate<GEN_NOISY>(MoveList&) const;

bool Position::is_legal(Move m) const {
    const int us = side, them = side ^ 1;
    const int from = from_sq(m), to = to_sq(m), ksq = king_sq(us);

    if (flag_of(m) == EP_CAPTURE) {
        int capSq = to + (us == WHITE ? -8 : 8);
        Bitboard occ = (occupied() ^ bb(from) ^ bb(capSq)) | bb(to);
        return !(attackers_to(ksq, occ) & colors[them] & ~bb(capSq));
    }
    if (from == ksq) return !(attackers_to(to, occupied() ^ bb(from)) & colors[them]);

    Bitboard chk = checkers();
    if (chk) {
        if (more_than_one(chk)) return false;
        int c = lsb(chk);
        if (to != c && !(Between[ksq][c] & bb(to))) return false;
    }
    return !(pinned(us) & bb(from)) || (Line[ksq][from] & bb(to));
}

Move Position::parse_uci(const std::string& s) const {
    MoveList list;
    generate<GEN_ALL>(list);
    for (Move m : list)
        if (move_to_uci(m) == s && is_legal(m)) return m;
    return NO_MOVE;
}

int Position::see(Move m) const {
    if (is_castle(m)) return 0;
    const int from = from_sq(m), to = to_sq(m);
    int gain[40], d = 0;
    Bitboard occ = occupied();
    int attackerPt = type_of(board[from]);

    if (flag_of(m) == EP_CAPTURE) {
        gain[0] = SEE_VALUE[PAWN];
        occ ^= bb(to + (side == WHITE ? -8 : 8));
    } else {
        gain[0] = board[to] == NO_PIECE ? 0 : SEE_VALUE[type_of(board[to])];
    }
    if (is_promo(m)) {
        gain[0] += SEE_VALUE[promo_type(m)] - SEE_VALUE[PAWN];
        attackerPt = promo_type(m);
    }
    occ ^= bb(from);

    const Bitboard bq = pieces[BISHOP] | pieces[QUEEN], rq = pieces[ROOK] | pieces[QUEEN];
    Bitboard attackers = attackers_to(to, occ) & occ;
    int stm = side ^ 1;

    while (d < 38) {
        Bitboard mine = attackers & colors[stm];
        if (!mine) break;
        int pt = PAWN;
        while (!(mine & pieces[pt])) pt++;
        if (pt == KING && (attackers & colors[stm ^ 1])) break;
        d++;
        gain[d] = SEE_VALUE[attackerPt] - gain[d - 1];
        occ ^= bb(lsb(mine & pieces[pt]));
        attackers = (attackers | (bishop_attacks(to, occ) & bq) | (rook_attacks(to, occ) & rq)) & occ;
        attackerPt = pt;
        stm ^= 1;
    }
    while (d > 0) {
        gain[d - 1] = -std::max(-gain[d - 1], gain[d]);
        d--;
    }
    return gain[0];
}

bool Position::is_repetition() const {
    const int n = int(history.size());
    const int stop = std::max(0, n - halfmove);
    for (int i = n - 2; i >= stop; i -= 2)
        if (history[i].key == key) return true;
    return false;
}

bool Position::insufficient_material() const {
    if (pieces[PAWN] | pieces[ROOK] | pieces[QUEEN]) return false;
    return popcount(pieces[KNIGHT] | pieces[BISHOP]) <= 1;
}

bool Position::is_draw(int) const {
    return halfmove >= 100 || insufficient_material() || is_repetition();
}
