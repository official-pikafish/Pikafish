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

#include "movegen.h"

#include <cassert>

#include "bitboard.h"
#include "position.h"

#if defined(USE_AVX512ICL)
    #include <array>
    #include <algorithm>
    #include <immintrin.h>
#endif

namespace Stockfish {

namespace {

#if defined(USE_AVX512ICL)

inline Move* write_moves(Move* moveList, uint32_t mask, __m512i vector) {
    _mm512_storeu_si512(reinterpret_cast<__m512i*>(moveList),
                        _mm512_maskz_compress_epi16(mask, vector));
    return moveList + popcount(mask);
}

inline Move* splat_moves(Move* moveList, Square from, Bitboard to_bb) {
    alignas(64) static constexpr auto SPLAT_TABLE = [] {
        std::array<Move, 90> table{};
        for (int8_t i = 0; i < 90; i++)
            table[i] = {Move(SQUARE_ZERO, Square{i})};
        return table;
    }();

    __m512i fromVec = _mm512_set1_epi16(Move(from, SQUARE_ZERO).raw());

    auto table = reinterpret_cast<const __m512i*>(SPLAT_TABLE.data());

    moveList = write_moves(moveList, static_cast<uint32_t>(to_bb >> 0),
                           _mm512_or_si512(_mm512_load_si512(table + 0), fromVec));
    moveList = write_moves(moveList, static_cast<uint32_t>(to_bb >> 32),
                           _mm512_or_si512(_mm512_load_si512(table + 1), fromVec));
    moveList = write_moves(moveList, static_cast<uint32_t>(to_bb >> 64),
                           _mm512_or_si512(_mm512_load_si512(table + 2), fromVec));

    return moveList;
}

#else

inline Move* splat_moves(Move* moveList, Square from, Bitboard to_bb) {
    while (to_bb)
        *moveList++ = Move(from, pop_lsb(to_bb));
    return moveList;
}

#endif

template<Color Us, PieceType Pt, GenType Type>
Move* generate_moves(const Position& pos, Move* moveList, Bitboard target) {

    static_assert(Pt != KING, "Unsupported piece type in generate_moves()");

    Bitboard bb = pos.pieces(Us, Pt);

    while (bb)
    {
        Square   from = pop_lsb(bb);
        Bitboard b    = 0;
        if constexpr (Pt != CANNON)
            b = (Pt != PAWN ? attacks_bb<Pt>(from, pos.pieces()) : attacks_bb<PAWN>(from, Us))
              & target;
        else
        {
            // Generate cannon capture moves.
            if (Type != QUIETS)
                b |= attacks_bb<CANNON>(from, pos.pieces()) & pos.pieces(~Us);

            // Generate cannon quite moves.
            if (Type != CAPTURES)
                b |= attacks_bb<ROOK>(from, pos.pieces()) & ~pos.pieces();

            // Restrict to target if in evasion generation
            if (Type == EVASIONS)
                b &= target;
        }

        moveList = splat_moves(moveList, from, b);
    }

    return moveList;
}

template<Color Us, GenType Type>
Move* generate_moves(const Position& pos, Move* moveList, Bitboard target) {
    moveList = generate_moves<Us, PAWN, Type>(pos, moveList, target);
    moveList = generate_moves<Us, BISHOP, Type>(pos, moveList, target);
    moveList = generate_moves<Us, ADVISOR, Type>(pos, moveList, target);
    moveList = generate_moves<Us, KNIGHT, Type>(pos, moveList, target);
    moveList = generate_moves<Us, CANNON, Type>(pos, moveList, target);
    moveList = generate_moves<Us, ROOK, Type>(pos, moveList, target);
    return moveList;
}

template<Color Us, GenType Type>
Move* generate_all(const Position& pos, Move* moveList) {

    const Square ksq    = pos.king_square(Us);
    Bitboard     target = Type == PSEUDO_LEGAL ? ~pos.pieces(Us)
                        : Type == CAPTURES     ? pos.pieces(~Us)
                                               : ~pos.pieces();  // QUIETS

    moveList = generate_moves<Us, Type>(pos, moveList, target);

    if (Type != EVASIONS)
    {
        Bitboard b = attacks_bb<KING>(ksq) & target;

        moveList = splat_moves(moveList, ksq, b);
    }

    return moveList;
}

}  // namespace


// <CAPTURES>     Generates all pseudo-legal captures
// <QUIETS>       Generates all pseudo-legal non-captures
// <PSEUDO_LEGAL> Generates all pseudo-legal captures and non-captures
//
// Returns a pointer to the end of the move list.
template<GenType Type>
Move* generate(const Position& pos, Move* moveList) {

    static_assert(Type != LEGAL && Type != EVASIONS, "Unsupported type in generate()");

    return pos.side_to_move() == WHITE ? generate_all<WHITE, Type>(pos, moveList)
                                       : generate_all<BLACK, Type>(pos, moveList);
}

// Explicit template instantiations
template Move* generate<CAPTURES>(const Position&, Move*);
template Move* generate<QUIETS>(const Position&, Move*);
template Move* generate<PSEUDO_LEGAL>(const Position&, Move*);


// generate<EVASIONS> generates all pseudo-legal check evasions when the side
// to move is in check. Returns a pointer to the end of the move list.

template<>
Move* generate<EVASIONS>(const Position& pos, Move* moveList) {

    assert(bool(pos.checkers()));

    // If there are more than one checker, use slow version
    if (more_than_one(pos.checkers()))
        return generate<PSEUDO_LEGAL>(pos, moveList);

    Color     us      = pos.side_to_move();
    Square    ksq     = pos.king_square(us);
    Square    checksq = lsb(pos.checkers());
    PieceType pt      = type_of(pos.piece_on(checksq));

    // Generate blocking evasions or captures of the checking piece
    Bitboard target = (between_bb(ksq, checksq)) & ~pos.pieces(us);
    moveList        = us == WHITE ? generate_moves<WHITE, EVASIONS>(pos, moveList, target)
                                  : generate_moves<BLACK, EVASIONS>(pos, moveList, target);

    // Generate evasions for king, capture and non capture moves
    Bitboard b = attacks_bb<KING>(ksq) & ~pos.pieces(us);
    // For all the squares attacked by slider checkers. We will remove them from
    // the king evasions in order to skip known illegal moves, which avoids any
    // useless legality checks later on.
    if (pt == ROOK || pt == CANNON)
        b &= ~line_bb(checksq, ksq) | pos.pieces(~us);
    moveList = splat_moves(moveList, ksq, b);

    // Generate move away hurdle piece evasions for cannon
    if (pt == CANNON)
    {
        Bitboard hurdle = between_bb(ksq, checksq) & pos.pieces(us);
        if (hurdle)
        {
            Square hurdleSq = pop_lsb(hurdle);
            pt              = type_of(pos.piece_on(hurdleSq));
            if (pt == PAWN)
                b = attacks_bb<PAWN>(hurdleSq, us) & ~line_bb(checksq, hurdleSq) & ~pos.pieces(us);
            else if (pt == CANNON)
                b = (attacks_bb<ROOK>(hurdleSq, pos.pieces()) & ~line_bb(checksq, hurdleSq)
                     & ~pos.pieces())
                  | (attacks_bb<CANNON>(hurdleSq, pos.pieces()) & pos.pieces(~us));
            else
                b = attacks_bb(pt, hurdleSq, pos.pieces()) & ~line_bb(checksq, hurdleSq)
                  & ~pos.pieces(us);
            moveList = splat_moves(moveList, hurdleSq, b);
        }
    }

    return moveList;
}


// generate<LEGAL> generates all the legal moves in the given position

template<>
Move* generate<LEGAL>(const Position& pos, Move* moveList) {

    Move* cur = moveList;

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
