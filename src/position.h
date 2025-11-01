/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

  Stockfish is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Stockfish is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef POSITION_H_INCLUDED
#define POSITION_H_INCLUDED

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <deque>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>

#include "bitboard.h"
#include "nnue/features/half_ka_v2_hm.h"
#include "types.h"

namespace Stockfish {

class TranspositionTable;

// StateInfo struct stores information needed to restore a Position object to
// its previous state when we retract a move. Whenever a move is made on the
// board (by calling Position::do_move), a StateInfo object must be passed.

struct StateInfo {

    // Copied when making a move
    Key     pawnKey;
    Key     minorPieceKey;
    Key     nonPawnKey[COLOR_NB];
    Value   majorMaterial[COLOR_NB];
    int16_t check10[COLOR_NB];
    int     rule60;
    int     pliesFromNull;

    // Not copied when making a move (will be recomputed anyhow)
    Key        key;
    Bitboard   checkersBB;
    StateInfo* previous;
    Bitboard   blockersForKing[COLOR_NB];
    Bitboard   pinners[COLOR_NB];
    Bitboard   checkSquares[PIECE_TYPE_NB];
    bool       needSlowCheck;
    Piece      capturedPiece;
    Move       move;
};


// A list to keep track of the position states along the setup moves (from the
// start position to the position just before the search starts). Needed by
// 'draw by repetition' detection. Use a std::deque because pointers to
// elements are not invalidated upon list resizing.
using StateListPtr = std::unique_ptr<std::deque<StateInfo>>;

// Position class stores information regarding the board representation as
// pieces, side to move, hash keys, etc. Important methods are
// do_move() and undo_move(), used by the search to update node info when
// traversing the search tree.
class Position {
   public:
    static void init();

    Position()                           = default;
    Position(const Position&)            = delete;
    Position& operator=(const Position&) = delete;

    // FEN string input/output
    Position&   set(const std::string& fenStr, StateInfo* si);
    Position&   set(const Position& pos, StateInfo* si);
    std::string fen() const;

    // Position representation
    Bitboard pieces() const;  // All pieces
    template<typename... PieceTypes>
    Bitboard pieces(PieceTypes... pts) const;
    Bitboard pieces(Color c) const;
    template<typename... PieceTypes>
    Bitboard     pieces(Color c, PieceTypes... pts) const;
    Piece        piece_on(Square s) const;
    const Piece* piece_array() const;
    bool         empty(Square s) const;
    template<PieceType Pt>
    int count(Color c) const;
    template<PieceType Pt>
    int      count() const;
    Square   king_square(Color c) const;
    uint64_t mid_encoding(Color c) const;

    // Checking
    Bitboard checkers() const;
    Bitboard blockers_for_king(Color c) const;
    Bitboard check_squares(PieceType pt) const;
    Bitboard pinners(Color c) const;

    // Attacks to/from a given square
    Bitboard attackers_to(Square s) const;
    Bitboard attackers_to(Square s, Bitboard occupied) const;
    template<Color c>
    void     update_blockers() const;
    Bitboard checkers_to(Color c, Square s) const;
    Bitboard checkers_to(Color c, Square s, Bitboard occupied) const;
    template<PieceType Pt>
    Bitboard attacks_by(Color c) const;

    // Properties of moves
    bool  legal(Move m) const;
    bool  pseudo_legal(const Move m) const;
    bool  capture(Move m) const;
    bool  gives_check(Move m) const;
    Piece moved_piece(Move m) const;
    Piece captured_piece() const;

    // Doing and undoing moves
    void       do_move(Move m, StateInfo& newSt, const TranspositionTable* tt);
    DirtyPiece do_move(Move m, StateInfo& newSt, bool givesCheck, const TranspositionTable* tt);
    void       undo_move(Move m);
    void       do_null_move(StateInfo& newSt, const TranspositionTable& tt);
    void       undo_null_move();

    // Static Exchange Evaluation
    bool see_ge(Move m, int threshold = 0) const;

    // Accessing hash keys
    Key key() const;
    Key pawn_key() const;
    Key minor_piece_key() const;
    Key defender_piece_key() const;
    Key non_pawn_key(Color c) const;

    // Other properties of the position
    Color    side_to_move() const;
    int      game_ply() const;
    bool     rule_judge(Value& result, int ply = 0);
    int      rule60_count() const;
    uint16_t chased(Color c);
    Value    major_material(Color c) const;
    Value    major_material() const;

    // Position consistency check, for debugging
    bool pos_is_ok() const;
    void flip();

    StateInfo* state() const;

    void put_piece(Piece pc, Square s);
    void remove_piece(Square s);

   private:
    // Initialization helpers (used while setting up a position)
    void set_state() const;
    void set_check_info() const;

    // Other helpers
    void                  move_piece(Square from, Square to);
    std::pair<Piece, int> do_move(Move m);
    void                  undo_move(Move m, Piece captured, int id = 0);
    Value                 detect_chases(int d, int ply = 0);
    bool                  chase_legal(Move m) const;
    Key                   adjust_key60(Key k) const;

    // Data members
    Piece      board[SQUARE_NB];
    Bitboard   byTypeBB[PIECE_TYPE_NB];
    Bitboard   byColorBB[COLOR_NB];
    Square     kingSquare[COLOR_NB];
    int        pieceCount[PIECE_NB];
    uint64_t   midEncoding[COLOR_NB];
    StateInfo* st;
    int        gamePly;
    Color      sideToMove;

    // Bloom filter for fast repetition filtering
    BloomFilter filter;

    // Board for chasing detection
    int idBoard[SQUARE_NB];
};

std::ostream& operator<<(std::ostream& os, const Position& pos);

inline Color Position::side_to_move() const { return sideToMove; }

inline Piece Position::piece_on(Square s) const {
    assert(is_ok(s));
    return board[s];
}

inline const Piece* Position::piece_array() const { return board; }

inline bool Position::empty(Square s) const { return piece_on(s) == NO_PIECE; }

inline Piece Position::moved_piece(Move m) const { return piece_on(m.from_sq()); }

inline Bitboard Position::pieces() const { return byTypeBB[ALL_PIECES]; }

template<typename... PieceTypes>
inline Bitboard Position::pieces(PieceTypes... pts) const {
    return (byTypeBB[pts] | ...);
}

inline Bitboard Position::pieces(Color c) const { return byColorBB[c]; }

template<typename... PieceTypes>
inline Bitboard Position::pieces(Color c, PieceTypes... pts) const {
    return pieces(c) & pieces(pts...);
}

template<PieceType Pt>
inline int Position::count(Color c) const {
    return pieceCount[make_piece(c, Pt)];
}

template<PieceType Pt>
inline int Position::count() const {
    return count<Pt>(WHITE) + count<Pt>(BLACK);
}

inline Square Position::king_square(Color c) const { return kingSquare[c]; }

inline uint64_t Position::mid_encoding(Color c) const { return midEncoding[c]; }

inline Bitboard Position::attackers_to(Square s) const { return attackers_to(s, pieces()); }

inline Bitboard Position::checkers_to(Color c, Square s) const {
    return checkers_to(c, s, pieces());
}

template<PieceType Pt>
inline Bitboard Position::attacks_by(Color c) const {

    Bitboard threats   = 0;
    Bitboard attackers = pieces(c, Pt);
    while (attackers)
        if (Pt == PAWN)
            threats |= attacks_bb<PAWN>(pop_lsb(attackers), c);
        else
            threats |= attacks_bb<Pt>(pop_lsb(attackers), pieces());
    return threats;
}

inline Bitboard Position::checkers() const { return st->checkersBB; }

inline Bitboard Position::blockers_for_king(Color c) const { return st->blockersForKing[c]; }

inline Bitboard Position::pinners(Color c) const { return st->pinners[c]; }

inline Bitboard Position::check_squares(PieceType pt) const { return st->checkSquares[pt]; }

inline Key Position::key() const { return adjust_key60(st->key); }

inline Key Position::adjust_key60(Key k) const {
    return (st->rule60 < 14 ? k : k ^ make_key((st->rule60 - 14) / 8))
         ^ (filter[st->key] ? make_key(14) : 0);
}

inline Key Position::pawn_key() const { return st->pawnKey; }

inline Key Position::minor_piece_key() const { return st->minorPieceKey; }

inline Key Position::non_pawn_key(Color c) const { return st->nonPawnKey[c]; }

inline Value Position::major_material(Color c) const { return st->majorMaterial[c]; }

inline Value Position::major_material() const {
    return major_material(WHITE) + major_material(BLACK);
}

inline int Position::game_ply() const { return gamePly; }

inline int Position::rule60_count() const { return st->rule60; }

inline bool Position::capture(Move m) const {
    assert(m.is_ok());
    return !empty(m.to_sq());
}

inline Piece Position::captured_piece() const { return st->capturedPiece; }

inline void Position::put_piece(Piece pc, Square s) {

    board[s] = pc;
    byTypeBB[ALL_PIECES] |= byTypeBB[type_of(pc)] |= s;
    byColorBB[color_of(pc)] |= s;
    pieceCount[pc]++;
    pieceCount[make_piece(color_of(pc), ALL_PIECES)]++;
    midEncoding[color_of(pc)] += Eval::NNUE::Features::HalfKAv2_hm::MidMirrorEncoding[pc][s];
}

inline void Position::remove_piece(Square s) {

    Piece pc = board[s];
    byTypeBB[ALL_PIECES] ^= s;
    byTypeBB[type_of(pc)] ^= s;
    byColorBB[color_of(pc)] ^= s;
    board[s] = NO_PIECE;
    pieceCount[pc]--;
    pieceCount[make_piece(color_of(pc), ALL_PIECES)]--;
    midEncoding[color_of(pc)] -= Eval::NNUE::Features::HalfKAv2_hm::MidMirrorEncoding[pc][s];
}

inline void Position::move_piece(Square from, Square to) {

    Piece    pc     = board[from];
    Bitboard fromTo = from | to;
    byTypeBB[ALL_PIECES] ^= fromTo;
    byTypeBB[type_of(pc)] ^= fromTo;
    byColorBB[color_of(pc)] ^= fromTo;
    board[from] = NO_PIECE;
    board[to]   = pc;
    if (type_of(pc) == KING)
        kingSquare[color_of(pc)] = to;
    midEncoding[color_of(pc)] -= Eval::NNUE::Features::HalfKAv2_hm::MidMirrorEncoding[pc][from];
    midEncoding[color_of(pc)] += Eval::NNUE::Features::HalfKAv2_hm::MidMirrorEncoding[pc][to];
}

inline void Position::do_move(Move m, StateInfo& newSt, const TranspositionTable* tt = nullptr) {
    do_move(m, newSt, gives_check(m), tt);
}

inline StateInfo* Position::state() const { return st; }

inline Position& Position::set(const Position& pos, StateInfo* si) {

    set(pos.fen(), si);

    // Special cares for bloom filter
    std::memcpy(&filter, &pos.filter, sizeof(BloomFilter));

    return *this;
}

}  // namespace Stockfish

#endif  // #ifndef POSITION_H_INCLUDED
