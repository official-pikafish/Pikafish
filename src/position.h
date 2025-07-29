/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2022 The Stockfish developers (see AUTHORS file)

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

#include <cassert>
#include <deque>
#include <memory> // For std::unique_ptr
#include <string>
#include <map>
#include <vector>
#include <stdlib.h>
#include <time.h>
#include "bitboard.h"
#include "psqt.h"
#include "types.h"

#include "nnue/nnue_accumulator.h"

namespace Stockfish {

#define MAXDARKDEPTH    4
#define QDARKDEPTH      1
#define MAXDARKTYPES    43

/// StateInfo struct stores information needed to restore a Position object to
/// its previous state when we retract a move. Whenever a move is made on the
/// board (by calling Position::do_move), a StateInfo object must be passed.
namespace PieceExchange {
    Piece charToPiece(unsigned char);
}
struct StateInfo {

  // Copied when making a move
  Key    materialKey;
  Value  material[COLOR_NB];
  int    pliesFromNull;
  int        darkDepth;
  int        darkTypes;

  // Not copied when making a move (will be recomputed anyhow)
  Key        key;
  Bitboard   checkersBB;
  StateInfo* previous;
  StateInfo* previousDark;
  Bitboard   blockersForKing[COLOR_NB];
  Bitboard   pinners[COLOR_NB];
  Bitboard   checkSquares[PIECE_TYPE_NB];
  bool       needSlowCheck;
  Piece      capturedPiece;

  Piece      darkPiece;
  Square     darkSquare;
  int        darkTypeIndex;
  
  
  uint16_t   chased;
  Move       move;
  // TODO: 这里可能按需要加一些结构表示和暗子有关的东西

  // Used by NNUE
  Eval::NNUE::Accumulator accumulator;
  DirtyPiece dirtyPiece;
};


/// A list to keep track of the position states along the setup moves (from the
/// start position to the position just before the search starts). Needed by
/// 'draw by repetition' detection. Use a std::deque because pointers to
/// elements are not invalidated upon list resizing.
typedef std::unique_ptr<std::deque<StateInfo>> StateListPtr;


/// Position class stores information regarding the board representation as
/// pieces, side to move, hash keys, etc. Important methods are
/// do_move() and undo_move(), used by the search to update node info when
/// traversing the search tree.
class Thread;

class Position {
public:
  static void init();

  Position() = default;
  Position(const Position&) = delete;
  Position& operator=(const Position&) = delete;

  // FEN string input/output
  Position& set(const std::string& fenStr, StateInfo* si, Thread* th);
  Position& set(const Position& pos, StateInfo* si, Thread* th);
  std::string fen() const;

  // Position representation
  Bitboard pieces(PieceType pt) const;
  Bitboard pieces(PieceType pt1, PieceType pt2) const;
  Bitboard pieces(Color c) const;
  Bitboard pieces(Color c, PieceType pt) const;
  Bitboard pieces(Color c, PieceType pt1, PieceType pt2) const;
  Bitboard pieces(Color c, PieceType pt1, PieceType pt2, PieceType pt3) const;
  Piece piece_on(Square s) const;
  int value_on(Square s) const;
  Piece nodarkPiece_on(Square s) const;
  bool isDark(Square s) const;
  Dark Darkof(Square s)const;
  Dark Darkof(Piece p)const;


  bool empty(Square s) const;
  template<PieceType Pt> int count(Color c) const;
  template<PieceType Pt> int count() const;
  template<PieceType Pt> int darkcount(Color c) const;
  template<PieceType Pt> int darkcount() const;
  template<PieceType Pt> Square square(Color c) const;

  // Checking
  Bitboard checkers() const;
  Bitboard blockers_for_king(Color c) const;
  Bitboard blockers_for_king(Bitboard sliders, Square s, Bitboard& pinners) const;
  Bitboard check_squares(PieceType pt) const;
  Bitboard pinners(Color c) const;
  // Attacks to/from a given square
  Bitboard attackers_to(Square s) const;
  Bitboard attackers_to(Square s, Bitboard occupied) const;
  Bitboard checkers_to(Color c, Square s) const;
  Bitboard checkers_to(Color c, Square s, Bitboard occupied) const;
  template<PieceType Pt> Bitboard attacks_by(Color c) const;

  // Properties of moves
  bool legal(Move m) const;
  bool pseudo_legal(const Move m) const;
  bool capture(Move m) const;
  bool gives_check(Move m);
  Piece moved_piece(Move m) const;
  Piece captured_piece() const;

  // Doing and undoing moves
  bool getDark(StateInfo& newSt, int& typecount, bool& isDarkDepth);
  void setDark();
  bool do_move(Move m, StateInfo& newSt);
  bool do_move(Move m, StateInfo& newSt, bool givesCheck);
  void undo_move(Move m);
  void do_null_move(StateInfo& newSt);
  void undo_null_move();

  // Static Exchange Evaluation
  bool see_ge(Move m, Value threshold = VALUE_ZERO) const;

  // Accessing hash keys
  Key key() const;
  Key key_after(Move m) const;
  Key material_key() const;

  // Other properties of the position
  bool isFirstSide() const;
  Color side_to_move() const;
  int game_ply() const;
  Thread* this_thread() const;
  bool is_repeated(Value& result, int ply = 0) const;
  ChaseMap chased(Color c);
  Score psq_score() const;
  Value material_sum() const;
  Value material_diff() const;

  // Position consistency check, for debugging
  bool pos_is_ok() const;
  void flip();

  // Used by NNUE
  StateInfo* state() const;

  void put_piece(Piece pc, Square s, bool updatepsq = true);
  void remove_piece(Square s, bool updatepsq = true);

private:
  // Initialization helpers (used while setting up a position)
  void set_state(StateInfo* si) const;
  void set_check_info(StateInfo* si) const;

  // Other helpers
  void move_piece(Square from, Square to);
  std::pair<Piece, int> light_do_move(Move m);
  void light_undo_move(Move m, Piece captured, int id = 0);
  void set_chase_info(int d);
  bool chase_legal(Move m, Bitboard b) const;

  // Data members
  Piece board[SQUARE_NB];

  Bitboard byTypeBB[PIECE_TYPE_NB];
  Bitboard byColorBB[COLOR_NB];
  int pieceCount[PIECE_NB];
  Thread* thisThread;
  StateInfo* st;
  int gamePly;
  Color sideToMove;
  Color firstSideMove;
  Score psq;

  // Bloom filter for fast repetition filtering
  BloomFilter filter;

  // Board for chasing detection
  int idBoard[SQUARE_NB];

  RestList restPieces[COLOR_NB];
};

extern std::ostream& operator<<(std::ostream& os, const Position& pos);

inline bool Position::isFirstSide() const {
    return sideToMove == firstSideMove;
}
inline Color Position::side_to_move() const {
  return sideToMove;
}

inline Piece Position::piece_on(Square s) const {
  assert(is_ok(s));
  return board[s];
}

inline int Position::value_on(Square s) const {
    Piece p = piece_on(s);
    return Darkof(p) == UNKNOWN ? 
        restPieces[color_of(p)].evgValue() : 
        PieceValue[MG][piece_on(s)];
}

inline Piece Position::nodarkPiece_on(Square s) const {
    assert(is_ok(s));
    return Piece(board[s] & 15);
}

inline bool Position::isDark(Square s) const {
    assert(is_ok(s));
    return Darkof(s) == UNKNOWN;
}

inline Dark Position::Darkof(Square s)const {
    assert(is_ok(s));
    return Dark((board[s] & 16) >> 4);
}

inline Dark Position::Darkof(Piece p)const {
    return Dark((p & 16) >> 4);
}

inline bool Position::empty(Square s) const {
  return piece_on(s) == NO_PIECE;
}

inline Piece Position::moved_piece(Move m) const {
  return piece_on(from_sq(m));
}

inline Bitboard Position::pieces(PieceType pt = ALL_PIECES) const {
  return byTypeBB[pt];
}

inline Bitboard Position::pieces(PieceType pt1, PieceType pt2) const {
  return pieces(pt1) | pieces(pt2);
}

inline Bitboard Position::pieces(Color c) const {
  return byColorBB[c];
}

inline Bitboard Position::pieces(Color c, PieceType pt) const {
  return pieces(c) & pieces(pt);
}

inline Bitboard Position::pieces(Color c, PieceType pt1, PieceType pt2) const {
  return pieces(c) & (pieces(pt1) | pieces(pt2));
}

inline Bitboard Position::pieces(Color c, PieceType pt1, PieceType pt2, PieceType pt3) const {
  return pieces(c) & (pieces(pt1) | pieces(pt2) | pieces(pt3));
}

template<PieceType Pt> inline int Position::count(Color c) const {
  return pieceCount[make_piece(c, Pt)];
}

template<PieceType Pt> inline int Position::count() const {
  return count<Pt>(WHITE) + count<Pt>(BLACK);
}

template<PieceType Pt> inline int Position::darkcount(Color c) const {
    return pieceCount[make_piece(c, Pt) ^ 16];
}

template<PieceType Pt> inline int Position::darkcount() const {
    return darkcount<Pt>(WHITE) + darkcount<Pt>(BLACK);
}

template<PieceType Pt> inline Square Position::square(Color c) const {
  assert(count<Pt>(c) == 1);
  return lsb(pieces(c, Pt));
}

inline Bitboard Position::attackers_to(Square s) const {
  return attackers_to(s, pieces());
}

inline Bitboard Position::checkers_to(Color c, Square s) const {
  return checkers_to(c, s, pieces());
}

template<PieceType Pt>
inline Bitboard Position::attacks_by(Color c) const {

  Bitboard threats = 0;
  Bitboard attackers = pieces(c, Pt);
  while (attackers)
      if (Pt == PAWN)
          threats |= pawn_attacks_bb(c, pop_lsb(attackers));
      else
          threats |= attacks_bb<Pt>(pop_lsb(attackers), pieces());
  return threats;
}

inline Bitboard Position::checkers() const {
  return st->checkersBB;
}

inline Bitboard Position::blockers_for_king(Color c) const {
  return st->blockersForKing[c];
}

inline Bitboard Position::pinners(Color c) const {
  return st->pinners[c];
}

inline Bitboard Position::check_squares(PieceType pt) const {
  return st->checkSquares[pt];
}

inline Key Position::key() const {
  return st->key;
}

inline Key Position::material_key() const {
    return st->materialKey;
}

inline Score Position::psq_score() const {
  return psq;
}

inline Value Position::material_sum() const {
  return st->material[WHITE] + st->material[BLACK];
}

inline Value Position::material_diff() const {
  return st->material[sideToMove] - st->material[~sideToMove];
}

inline int Position::game_ply() const {
  return gamePly;
}

inline bool Position::capture(Move m) const {
  assert(is_ok(m));
  // Castling is encoded as "king captures rook"
  return !empty(to_sq(m));
}

inline Piece Position::captured_piece() const {
  return st->capturedPiece;
}

inline Thread* Position::this_thread() const {
  return thisThread;
}

inline void Position::put_piece(Piece pc, Square s, bool updatepsq) {

  board[s] = pc;
  byTypeBB[ALL_PIECES] |= byTypeBB[type_of(pc)] |= s;
  byColorBB[color_of(pc)] |= s;
  pieceCount[pc]++;
  pieceCount[make_piece(color_of(pc), ALL_PIECES)]++;
  if (!updatepsq)return;
  if (Darkof(pc) == UNKNOWN) {
      Color c = color_of(pc);
      int eg = restPieces[c].evgValue();
      eg *= (c == BLACK ? -1 : 1);
      File f = File(edge_distance(file_of(s)));
      if (f > FILE_E) --f;
      psq += make_score(eg, eg) + PSQT::psqCap[rank_of(s)][f];
  }
  else
  {
      psq += PSQT::psq[pc][s];
  }
}

inline void Position::remove_piece(Square s, bool updatepsq) {

  Piece pc = board[s];
  byTypeBB[ALL_PIECES] ^= s;
  byTypeBB[type_of(pc)] ^= s;
  byColorBB[color_of(pc)] ^= s;
  board[s] = NO_PIECE;
  pieceCount[pc]--;
  pieceCount[make_piece(color_of(pc), ALL_PIECES)]--;
  if (!updatepsq)return;
  if (Darkof(pc) == UNKNOWN) {
      Color c = color_of(pc);
      int eg = restPieces[c].evgValue();
      eg *= (c == BLACK ? -1 : 1);
      File f = File(edge_distance(file_of(s)));
      if (f > FILE_E) --f;
      psq -= make_score(eg, eg) + PSQT::psqCap[rank_of(s)][f];
  }
  else
  {
      psq -= PSQT::psq[pc][s];
  }
}

inline void Position::move_piece(Square from, Square to) {

  Piece pc = board[from];
  Bitboard fromTo = from | to;
  byTypeBB[ALL_PIECES] ^= fromTo;
  byTypeBB[type_of(pc)] ^= fromTo;
  byColorBB[color_of(pc)] ^= fromTo;
  board[from] = NO_PIECE;
  board[to] = pc;
  psq += PSQT::psq[pc][to] - PSQT::psq[pc][from];
}

inline bool Position::do_move(Move m, StateInfo& newSt) {
    return do_move(m, newSt, gives_check(m));
}

inline StateInfo* Position::state() const {

  return st;
}

inline Position& Position::set(const Position& pos, StateInfo* si, Thread* th) {

  set(pos.fen(), si, th);

  // Special cares for bloom filter
  std::memcpy(&filter, &pos.filter, sizeof(BloomFilter));

  return *this;
}

} // namespace Stockfish

#endif // #ifndef POSITION_H_INCLUDED
