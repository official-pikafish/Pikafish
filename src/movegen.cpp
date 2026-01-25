/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

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
    // Avoid _mm512_mask_compressstoreu_epi16() as it's 256 uOps on Zen4
    _mm512_storeu_si512(reinterpret_cast<__m512i*>(moveList),
                        _mm512_maskz_compress_epi16(mask, vector));
    return moveList + popcount(mask);
}

inline Move* splat_moves(Move* moveList, Square from, Bitboard to_bb) {
    alignas(64) static constexpr auto SPLAT_TABLE = [] {
        std::array<Move, 90> table{};
        for (uint8_t i = 0; i < 90; i++)
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

// Absorption Xiangqi: Generate attacks for a specific piece type ability
// This handles the special cases for each piece type
template<Color Us, GenType Type>
Bitboard generate_ability_attacks(PieceType pt, Square from, const Position& pos, Bitboard target, bool hasAnyAbsorbed) {
    Bitboard b = 0;
    Bitboard occupied = pos.pieces();

    switch (pt) {
    case PAWN:
        b = attacks_bb<PAWN>(from, Us) & target;
        break;

    case ROOK:
        b = attacks_bb<ROOK>(from, occupied) & target;
        break;

    case KNIGHT:
        b = attacks_bb<KNIGHT>(from, occupied) & target;
        break;

    case BISHOP:
        // If piece has absorbed abilities, Bishop can move full board (diagonal 2 steps)
        // otherwise restricted to own half
        if (hasAnyAbsorbed)
            b = attacks_bb<BISHOP>(from, occupied);  // Full board bishop moves
        else
            b = attacks_bb<BISHOP>(from, occupied);  // Already restricted by attack table
        b &= target;
        break;

    case ADVISOR:
        // If piece has absorbed abilities, Advisor can move full board (diagonal 1 step)
        // otherwise restricted to palace
        if (hasAnyAbsorbed)
            b = unconstrained_attacks_bb<ADVISOR>(from) & target;
        else
            b = attacks_bb<ADVISOR>(from) & target;
        break;

    case CANNON:
        // Cannon has special capture vs quiet logic
        if (Type != QUIETS)
            b |= attacks_bb<CANNON>(from, occupied) & pos.pieces(~Us);
        if (Type != CAPTURES)
            b |= attacks_bb<ROOK>(from, occupied) & ~occupied;
        if (Type == EVASIONS)
            b &= target;
        break;

    default:
        break;
    }

    return b;
}

// Absorption Xiangqi: Generate all moves for a piece considering its absorbed abilities
template<Color Us, GenType Type>
Move* generate_piece_with_absorption(const Position& pos, Move* moveList, Square from, PieceType basePt, Bitboard target) {
    AbilityMask abilities = pos.absorbed_abilities(from);
    bool hasAnyAbsorbed = (abilities != 0);

    // Collect all destination squares from base type and absorbed types
    Bitboard destinations = 0;

    // Generate moves for base piece type
    destinations |= generate_ability_attacks<Us, Type>(basePt, from, pos, target, hasAnyAbsorbed);

    // Generate moves for each absorbed ability
    for (PieceType pt = ROOK; pt < KING; ++pt) {
        if (abilities & (1 << pt)) {
            destinations |= generate_ability_attacks<Us, Type>(pt, from, pos, target, hasAnyAbsorbed);
        }
    }

    return splat_moves(moveList, from, destinations);
}

template<Color Us, PieceType Pt, GenType Type>
Move* generate_moves(const Position& pos, Move* moveList, Bitboard target) {

    static_assert(Pt != KING, "Unsupported piece type in generate_moves()");

    Bitboard bb = pos.pieces(Us, Pt);

    while (bb)
    {
        Square from = pop_lsb(bb);
        moveList = generate_piece_with_absorption<Us, Type>(pos, moveList, from, Pt, target);
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
    // Absorption Xiangqi: hurdle piece may have absorbed abilities
    if (pt == CANNON)
    {
        Bitboard hurdle = between_bb(ksq, checksq) & pos.pieces(us);
        if (hurdle)
        {
            Square hurdleSq = pop_lsb(hurdle);
            PieceType hurdlePt = type_of(pos.piece_on(hurdleSq));
            AbilityMask abilities = pos.absorbed_abilities(hurdleSq);
            bool hasAnyAbsorbed = (abilities != 0);

            b = 0;
            Bitboard validTarget = ~line_bb(checksq, hurdleSq) & ~pos.pieces(us);
            Bitboard occupied = pos.pieces();

            // Generate moves for base type
            if (hurdlePt == PAWN)
                b |= attacks_bb<PAWN>(hurdleSq, us) & validTarget;
            else if (hurdlePt == CANNON)
                b |= ((attacks_bb<ROOK>(hurdleSq, occupied) & ~pos.pieces())
                    | (attacks_bb<CANNON>(hurdleSq, occupied) & pos.pieces(~us))) & ~line_bb(checksq, hurdleSq);
            else if (hurdlePt == ADVISOR && hasAnyAbsorbed)
                b |= unconstrained_attacks_bb<ADVISOR>(hurdleSq) & validTarget;
            else
                b |= attacks_bb(hurdlePt, hurdleSq, occupied) & validTarget;

            // Generate moves for absorbed abilities
            for (PieceType apt = ROOK; apt < KING; ++apt) {
                if (abilities & (1 << apt)) {
                    if (apt == PAWN)
                        b |= attacks_bb<PAWN>(hurdleSq, us) & validTarget;
                    else if (apt == CANNON)
                        b |= ((attacks_bb<ROOK>(hurdleSq, occupied) & ~pos.pieces())
                            | (attacks_bb<CANNON>(hurdleSq, occupied) & pos.pieces(~us))) & ~line_bb(checksq, hurdleSq);
                    else if (apt == ADVISOR)
                        b |= unconstrained_attacks_bb<ADVISOR>(hurdleSq) & validTarget;
                    else
                        b |= attacks_bb(apt, hurdleSq, occupied) & validTarget;
                }
            }

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
