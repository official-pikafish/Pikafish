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

#include <cassert>

#include "movegen.h"
#include "position.h"

namespace Stockfish {

namespace {

  template<Color Us, PieceType Pt, GenType Type>
  ExtMove* generate_moves(const Position& pos, ExtMove* moveList, Bitboard target) {

    static_assert(Pt != KING, "Unsupported piece type in generate_moves()");

    Bitboard bb = pos.pieces(Us, Pt);

    while (bb)
    {
        Square from = pop_lsb(bb);
        Bitboard b = 0;
        if (Pt != CANNON)
            b = (Pt != PAWN ? attacks_bb<Pt>(from, pos.pieces())
                            : pawn_attacks_bb(Us, from)) & target;
        else {
            // Generate cannon capture moves.
            if (Type == CAPTURES || Type == PSEUDO_LEGAL)
                b |= attacks_bb<CANNON>(from, pos.pieces()) & pos.pieces(~Us);

            // Generate cannon quite moves.
            if (Type != CAPTURES)
                b |= attacks_bb<ROOK>(from, pos.pieces()) & ~pos.pieces();
        }

        // To check, you either move freely a blocker or make a direct check.
        if (Type == QUIET_CHECKS && !(pos.blockers_for_king(~Us) & from))
            b &= pos.check_squares(Pt);

        while (b)
            *moveList++ = make_move(from, pop_lsb(b));
    }

    return moveList;
  }


  template<Color Us, GenType Type>
  ExtMove* generate_all(const Position& pos, ExtMove* moveList) {

    const Square ksq = pos.square<KING>(Us);
    Bitboard target  = Type == PSEUDO_LEGAL ? ~pos.pieces( Us)
                     : Type == CAPTURES     ?  pos.pieces(~Us)
                                            : ~pos.pieces(   ); // QUIETS || QUIET_CHECKS

    moveList = generate_moves<Us,    ROOK, Type>(pos, moveList, target);
    moveList = generate_moves<Us, ADVISOR, Type>(pos, moveList, target);
    moveList = generate_moves<Us,  CANNON, Type>(pos, moveList, target);
    moveList = generate_moves<Us,    PAWN, Type>(pos, moveList, target);
    moveList = generate_moves<Us,  KNIGHT, Type>(pos, moveList, target);
    moveList = generate_moves<Us,  BISHOP, Type>(pos, moveList, target);

    if (Type != QUIET_CHECKS || pos.blockers_for_king(~Us) & ksq)
    {
        Bitboard b = attacks_bb<KING>(ksq) & target;
        if (Type == QUIET_CHECKS)
            b &= ~attacks_bb<ROOK>(pos.square<KING>(~Us));

        while (b)
            *moveList++ = make_move(ksq, pop_lsb(b));
    }

    return moveList;
  }

} // namespace


/// <CAPTURES>     Generates all pseudo-legal captures
/// <QUIETS>       Generates all pseudo-legal non-captures
/// <QUIET_CHECKS> Generates all pseudo-legal non-captures giving check
/// <PSEUDO_LEGAL> Generates all pseudo-legal captures and non-captures
///
/// Returns a pointer to the end of the move list.

template<GenType Type>
ExtMove* generate(const Position& pos, ExtMove* moveList) {

  static_assert(Type != LEGAL, "Unsupported type in generate()");

  Color us = pos.side_to_move();

  return us == WHITE ? generate_all<WHITE, Type>(pos, moveList)
                     : generate_all<BLACK, Type>(pos, moveList);
}

// Explicit template instantiations
template ExtMove* generate<CAPTURES>(const Position&, ExtMove*);
template ExtMove* generate<QUIETS>(const Position&, ExtMove*);
template ExtMove* generate<QUIET_CHECKS>(const Position&, ExtMove*);
template ExtMove* generate<PSEUDO_LEGAL>(const Position&, ExtMove*);


/// generate<LEGAL> generates all the legal moves in the given position

template<>
ExtMove* generate<LEGAL>(const Position& pos, ExtMove* moveList) {

  Color us = pos.side_to_move();
  Bitboard pinned = pos.blockers_for_king(us) & pos.pieces(us);
  Square ksq = pos.square<KING>(us);
  ExtMove* cur = moveList;

  moveList = generate<PSEUDO_LEGAL>(pos, moveList);
  while (cur != moveList)
      // A move is legal when not in check and not moving the king or a pinned piece
      // We also have to take special cares about the hollow cannon
      if (   (pos.checkers() || (pinned && pinned & from_sq(*cur)) || from_sq(*cur) == ksq ||
              attacks_bb<ROOK>(ksq, pos.pieces() & pos.pieces(~us, CANNON)))
          && !pos.legal(*cur))
          *cur = (--moveList)->move;
      else
          ++cur;

  return moveList;
}

} // namespace Stockfish
