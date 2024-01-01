/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2024 The Stockfish developers (see AUTHORS file)

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

#include "movegen.h"

#include <cassert>

#include "bitboard.h"
#include "position.h"

namespace Stockfish {

namespace {

// Store the places where we can result in hollow cannon discovered check
// by inserting a piece in between the hollow cannon and the king.
thread_local Bitboard HollowCannonDiscover;
// Store opponent's king square to avoid multiple calls to pos.square<KING>(~Us)
// They have to be thread local to avoid multi-threading race conditions.
thread_local Square OpponentKingSquare;

template<Color Us, PieceType Pt, GenType Type>
ExtMove* generate_moves(const Position& pos, ExtMove* moveList, Bitboard target) {

    static_assert(Pt != KING, "Unsupported piece type in generate_moves()");

    Bitboard bb = pos.pieces(Us, Pt);

    while (bb)
    {
        Square   from = pop_lsb(bb);
        Bitboard b    = 0;
        if constexpr (Pt != CANNON)
            b = (Pt != PAWN ? attacks_bb<Pt>(from, pos.pieces()) : pawn_attacks_bb(Us, from))
              & target;
        else
        {
            // Generate cannon capture moves.
            if (Type != QUIETS && Type != QUIET_CHECKS)
                b |= attacks_bb<CANNON>(from, pos.pieces()) & pos.pieces(~Us);

            // Generate cannon quite moves.
            if (Type != CAPTURES)
                b |= attacks_bb<ROOK>(from, pos.pieces()) & ~pos.pieces();

            // Restrict to target if in evasion generation
            if (Type == EVASIONS)
                b &= target;
        }

        // To check, you either move freely a blocker or make a direct check.
        if constexpr (Type == QUIET_CHECKS)
            b &= Pt == CANNON ? ~line_bb(from, OpponentKingSquare)
                                  & (pos.check_squares(Pt) | HollowCannonDiscover)
               : (pos.blockers_for_king(~Us) & from)
                 ? ~line_bb(from, OpponentKingSquare)
                 : (pos.check_squares(Pt) | HollowCannonDiscover);

        while (b)
            *moveList++ = Move(from, pop_lsb(b));
    }

    return moveList;
}

template<Color Us, GenType Type>
ExtMove* generate_moves(const Position& pos, ExtMove* moveList, Bitboard target) {
    moveList = generate_moves<Us, ROOK, Type>(pos, moveList, target);
    moveList = generate_moves<Us, ADVISOR, Type>(pos, moveList, target);
    moveList = generate_moves<Us, CANNON, Type>(pos, moveList, target);
    moveList = generate_moves<Us, PAWN, Type>(pos, moveList, target);
    moveList = generate_moves<Us, KNIGHT, Type>(pos, moveList, target);
    moveList = generate_moves<Us, BISHOP, Type>(pos, moveList, target);
    return moveList;
}

template<Color Us, GenType Type>
ExtMove* generate_all(const Position& pos, ExtMove* moveList) {

    const Square ksq    = pos.square<KING>(Us);
    Bitboard     target = Type == PSEUDO_LEGAL ? ~pos.pieces(Us)
                        : Type == CAPTURES     ? pos.pieces(~Us)
                                               : ~pos.pieces();  // QUIETS || QUIET_CHECKS

    moveList = generate_moves<Us, Type>(pos, moveList, target);

    if (Type != EVASIONS && (Type != QUIET_CHECKS || pos.blockers_for_king(~Us) & ksq))
    {
        Bitboard b = attacks_bb<KING>(ksq) & target;
        if constexpr (Type == QUIET_CHECKS)
            b &= ~attacks_bb<ROOK>(OpponentKingSquare);

        while (b)
            *moveList++ = Move(ksq, pop_lsb(b));
    }

    return moveList;
}

}  // namespace


// <CAPTURES>     Generates all pseudo-legal captures
// <QUIETS>       Generates all pseudo-legal non-captures
// <PSEUDO_LEGAL> Generates all pseudo-legal captures and non-captures
// <QUIET_CHECKS> Generates all pseudo-legal non-captures giving check
//
// Returns a pointer to the end of the move list.
template<GenType Type>
ExtMove* generate(const Position& pos, ExtMove* moveList) {

    static_assert(Type != LEGAL && Type != EVASIONS, "Unsupported type in generate()");

    Color us = pos.side_to_move();

    // Prepare hollow cannon discover bitboard when generate quite check moves
    if constexpr (Type == QUIET_CHECKS)
    {
        OpponentKingSquare     = pos.square<KING>(~us);
        Bitboard hollowCannons = pos.check_squares(ROOK) & pos.pieces(us, CANNON);
        HollowCannonDiscover   = Bitboard(0);
        while (hollowCannons)
            HollowCannonDiscover |= between_bb(OpponentKingSquare, pop_lsb(hollowCannons));
    }

    return us == WHITE ? generate_all<WHITE, Type>(pos, moveList)
                       : generate_all<BLACK, Type>(pos, moveList);
}

// Explicit template instantiations
template ExtMove* generate<CAPTURES>(const Position&, ExtMove*);
template ExtMove* generate<QUIETS>(const Position&, ExtMove*);
template ExtMove* generate<QUIET_CHECKS>(const Position&, ExtMove*);
template ExtMove* generate<PSEUDO_LEGAL>(const Position&, ExtMove*);


// generate<EVASIONS> generates all pseudo-legal check evasions when the side
// to move is in check. Returns a pointer to the end of the move list.

template<>
ExtMove* generate<EVASIONS>(const Position& pos, ExtMove* moveList) {

    assert(bool(pos.checkers()));

    // If there are more than one checker, use slow version
    if (more_than_one(pos.checkers()))
        return generate<PSEUDO_LEGAL>(pos, moveList);

    Color     us      = pos.side_to_move();
    Square    ksq     = pos.square<KING>(us);
    Square    checksq = lsb(pos.checkers());
    PieceType pt      = type_of(pos.piece_on(checksq));

    // Generate evasions for king, capture and non capture moves
    Bitboard b = attacks_bb<KING>(ksq) & ~pos.pieces(us);
    // For all the squares attacked by slider checkers. We will remove them from
    // the king evasions in order to skip known illegal moves, which avoids any
    // useless legality checks later on.
    if (pt == ROOK || pt == CANNON)
        b &= ~line_bb(checksq, ksq) | pos.pieces(~us);
    while (b)
        *moveList++ = Move(ksq, pop_lsb(b));

    // Generate move away hurdle piece evasions for cannon
    if (pt == CANNON)
    {
        Bitboard hurdle = between_bb(ksq, checksq) & pos.pieces(us);
        if (hurdle)
        {
            Square hurdleSq = pop_lsb(hurdle);
            pt              = type_of(pos.piece_on(hurdleSq));
            if (pt == PAWN)
                b = pawn_attacks_bb(us, hurdleSq) & ~line_bb(checksq, hurdleSq) & ~pos.pieces(us);
            else if (pt == CANNON)
                b = (attacks_bb<ROOK>(hurdleSq, pos.pieces()) & ~line_bb(checksq, hurdleSq)
                     & ~pos.pieces())
                  | (attacks_bb<CANNON>(hurdleSq, pos.pieces()) & pos.pieces(~us));
            else
                b = attacks_bb(pt, hurdleSq, pos.pieces()) & ~line_bb(checksq, hurdleSq)
                  & ~pos.pieces(us);
            while (b)
                *moveList++ = Move(hurdleSq, pop_lsb(b));
        }
    }

    // Generate blocking evasions or captures of the checking piece
    Bitboard target = (between_bb(ksq, checksq)) & ~pos.pieces(us);
    return us == WHITE ? generate_moves<WHITE, EVASIONS>(pos, moveList, target)
                       : generate_moves<BLACK, EVASIONS>(pos, moveList, target);
}


// generate<LEGAL> generates all the legal moves in the given position

template<>
ExtMove* generate<LEGAL>(const Position& pos, ExtMove* moveList) {

    ExtMove* cur = moveList;

    moveList =
      pos.checkers() ? generate<EVASIONS>(pos, moveList) : generate<PSEUDO_LEGAL>(pos, moveList);

    while (cur != moveList)
        if (!pos.legal(*cur))
            *cur = *(--moveList);
        else
            ++cur;

    return moveList;
}

}  // namespace Stockfish
