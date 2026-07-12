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

#ifndef ATTACKS_H_INCLUDED
#define ATTACKS_H_INCLUDED

#include <array>
#include <cassert>
#include <initializer_list>
#include <utility>

#include "bitboard.h"
#include "types.h"

namespace Stockfish::Attacks {

#ifdef USE_PEXT
    #define IF_NOT_PEXT(...)
using MagicMask = u32;
#else
    #define IF_NOT_PEXT(...) __VA_ARGS__
using MagicMask = Bitboard;
#endif

// Magic holds all magic bitboards relevant data for a single square
struct Magic {
    Bitboard   mask;
    MagicMask* attacks;
    unsigned   shift;
#ifdef USE_PEXT
    Bitboard pseudoAttacks;
#else
    Bitboard magic;
#endif

    // Compute the attack's index using the 'magic bitboards' approach
    unsigned index(Bitboard occupied) const {

#ifdef USE_PEXT
        return unsigned(pext(occupied, mask, shift));
#else
        return unsigned(((occupied & mask) * magic) >> shift);
#endif
    }

    Bitboard attack_bb(Bitboard occupied) const {
#ifdef USE_PEXT
        return pdep(attacks[index(occupied)], pseudoAttacks);
#else
        return attacks[index(occupied)];
#endif
    }
};

void         init();
const Magic& magic(Square s, PieceType pt);

Bitboard line_bb(Square s1, Square s2);
Bitboard between_bb(Square s1, Square s2);
Bitboard ray_pass_bb(Square s1, Square s2);
Bitboard leaper_pass_bb(Square s1, Square s2);

// Returns true if the squares s1, s2 and s3 are aligned either on a
// straight or on a diagonal line.
inline bool aligned(Square s1, Square s2, Square s3) { return bool(line_bb(s1, s2) & s3); }

// Returns the squares attacked by pawns of the given color
// from the given square.
template<Color C>
constexpr Bitboard pawn_attacks_bb(Square s) {
    Bitboard b      = square_bb(s);
    Bitboard attack = shift<C == WHITE ? NORTH : SOUTH>(b);
    if ((C == WHITE && rank_of(s) > RANK_4) || (C == BLACK && rank_of(s) < RANK_5))
        attack |= shift<WEST>(b) | shift<EAST>(b);
    return attack;
}

// Returns the squares that if there is a pawn
// of the given color in there, it can attack the square s.
template<Color C>
constexpr Bitboard pawn_attacks_to_bb(Square s) {
    Bitboard b      = square_bb(s);
    Bitboard attack = shift<C == WHITE ? SOUTH : NORTH>(b);
    if ((C == WHITE && rank_of(s) > RANK_4) || (C == BLACK && rank_of(s) < RANK_5))
        attack |= shift<WEST>(b) | shift<EAST>(b);
    return attack;
}

// Returns the bitboard of target square for the given step
// from the given square. If the step is off the board, returns empty bitboard.
constexpr Bitboard safe_destination(Square s, int step) {
    Square to = Square(s + step);
    return is_ok(to) && distance(s, to) <= 2 ? square_bb(to) : Bitboard(0);
}

template<PieceType pt>
constexpr Bitboard sliding_attack(Square sq, Bitboard occupied) {
    assert(pt == ROOK || pt == CANNON);
    Bitboard attack = 0;

    for (auto const& d : {NORTH, SOUTH, EAST, WEST})
    {
        bool hurdle = false;
        for (Square s = sq + d; is_ok(s) && distance(s - d, s) == 1; s += d)
        {
            if (pt == ROOK || hurdle)
                attack |= s;

            if (occupied & s)
            {
                if (pt == CANNON && !hurdle)
                    hurdle = true;
                else
                    break;
            }
        }
    }

    return attack;
}

template<PieceType pt>
constexpr Bitboard lame_leaper_path(Direction d, Square s) {
    Bitboard b  = 0;
    Square   to = s + d;
    if (!is_ok(to) || distance(s, to) > 3)
        return b;

    // If piece type is by knight attacks, swap the source and destination square
    if (pt == KNIGHT_TO)
    {
        std::swap(s, to);
        d = -d;
    }

    Direction dr = d > 0 ? NORTH : SOUTH;
    Direction df = (abs(d % NORTH) < NORTH / 2 ? d % NORTH : -(d % NORTH)) < 0 ? WEST : EAST;

    int diff = abs(file_of(to) - file_of(s)) - abs(rank_of(to) - rank_of(s));
    if (diff > 0)
        s += df;
    else if (diff < 0)
        s += dr;
    else
        s += df + dr;

    b |= s;
    return b;
}

template<PieceType pt>
constexpr auto get_directions() {
    std::array<Direction, 4> BishopDirections{2 * NORTH_EAST, 2 * SOUTH_EAST, 2 * SOUTH_WEST,
                                              2 * NORTH_WEST};
    std::array<Direction, 8> KnightDirections{2 * SOUTH + WEST, 2 * SOUTH + EAST, SOUTH + 2 * WEST,
                                              SOUTH + 2 * EAST, NORTH + 2 * WEST, NORTH + 2 * EAST,
                                              2 * NORTH + WEST, 2 * NORTH + EAST};
    if constexpr (pt == BISHOP)
        return BishopDirections;
    else
        return KnightDirections;
}

template<PieceType pt>
constexpr Bitboard lame_leaper_path(Square s) {
    Bitboard b = 0;
    for (const auto& d : get_directions<pt>())
        b |= lame_leaper_path<pt>(d, s);

    if constexpr (pt == BISHOP)
        b &= HalfBB[rank_of(s) > RANK_4];
    return b;
}

template<PieceType pt>
constexpr Bitboard lame_leaper_attack(Square s, Bitboard occupied) {
    Bitboard b = 0;
    for (const auto& d : get_directions<pt>())
    {
        Square to = s + d;
        if (is_ok(to) && distance(s, to) < 3 && !(lame_leaper_path<pt>(d, s) & occupied))
            b |= to;
    }
    if (pt == BISHOP)
        b &= HalfBB[rank_of(s) > RANK_4];
    return b;
}

inline constexpr auto PseudoAttacks = [] {
    std::array<std::array<Bitboard, SQUARE_NB>, PIECE_TYPE_NB + 3> PseudoAttacks{};

    for (Square s1 = SQ_A0; s1 <= SQ_I9; ++s1)
    {
        PseudoAttacks[NO_PIECE_TYPE][s1] = pawn_attacks_bb<WHITE>(s1);
        PseudoAttacks[PAWN][s1]          = pawn_attacks_bb<BLACK>(s1);

        PseudoAttacks[PAWN_TO - 1][s1] = pawn_attacks_to_bb<WHITE>(s1);
        PseudoAttacks[PAWN_TO][s1]     = pawn_attacks_to_bb<BLACK>(s1);

        PseudoAttacks[ROOK][s1]   = sliding_attack<ROOK>(s1, 0);
        PseudoAttacks[BISHOP][s1] = lame_leaper_attack<BISHOP>(s1, 0);
        PseudoAttacks[KNIGHT][s1] = lame_leaper_attack<KNIGHT>(s1, 0);

        // Only generate pseudo attacks in the palace squares for king and advisor
        for (int step : {NORTH, SOUTH, WEST, EAST})
        {
            if (Palace & s1)
                PseudoAttacks[KING][s1] |= safe_destination(s1, step) & Palace;
            // Unconstrained king
            PseudoAttacks[KING + 3][s1] |= safe_destination(s1, step);
        }

        for (int step : {NORTH_WEST, NORTH_EAST, SOUTH_WEST, SOUTH_EAST})
        {
            if (Palace & s1)
                PseudoAttacks[ADVISOR][s1] |= safe_destination(s1, step) & Palace;
            // Unconstrained advisor
            PseudoAttacks[ADVISOR + 1][s1] |= safe_destination(s1, step);
        }
    }

    return PseudoAttacks;
}();

// Returns the pseudo attacks of the given piece type
// assuming an empty board.
template<PieceType Pt>
inline Bitboard attacks_bb(Square s, Color c = COLOR_NB) {

    assert(Pt != KNIGHT_TO && ((Pt != PAWN && Pt != PAWN_TO) || c < COLOR_NB) && is_ok(s));
    if constexpr (Pt != PAWN && Pt != PAWN_TO)
        return PseudoAttacks[Pt][s];
    else if constexpr (Pt == PAWN)
        return PseudoAttacks[c == WHITE ? NO_PIECE_TYPE : PAWN][s];
    else  // if constexpr (Pt == PAWN_TO)
        return PseudoAttacks[c == WHITE ? PAWN_TO - 1 : PAWN_TO][s];
}

// Returns the attacks by the given piece
// assuming the board is occupied according to the passed Bitboard.
// Sliding piece attacks do not continue passed an occupied square.
template<PieceType Pt>
inline Bitboard attacks_bb(Square s, Bitboard occupied) {

    assert(Pt != PAWN && Pt != PAWN_TO && is_ok(s));

    switch (Pt)
    {
    case ROOK :
    case CANNON :
    case BISHOP :
    case KNIGHT :
    case KNIGHT_TO :
        return magic(s, Pt).attack_bb(occupied);
    default :
        return PseudoAttacks[Pt][s];
    }
}

// Returns the attacks by the given piece
// assuming the board is occupied according to the passed Bitboard.
// Sliding piece attacks do not continue passed an occupied square.
inline Bitboard attacks_bb(PieceType pt, Square s, Bitboard occupied) {

    assert(pt != PAWN && pt < KNIGHT_TO && is_ok(s));

    switch (pt)
    {
    case ROOK :
        return attacks_bb<ROOK>(s, occupied);
    case CANNON :
        return attacks_bb<CANNON>(s, occupied);
    case BISHOP :
        return attacks_bb<BISHOP>(s, occupied);
    case KNIGHT :
        return attacks_bb<KNIGHT>(s, occupied);
    default :
        return PseudoAttacks[pt][s];
    }
}

template<PieceType Pt>
inline Bitboard unconstrained_attacks_bb(Square s) {

    assert((Pt == KING || Pt == ADVISOR) && is_ok(s));

    switch (Pt)
    {
    case KING :
        return PseudoAttacks[KING + 3][s];
    case ADVISOR :
        return PseudoAttacks[ADVISOR + 1][s];
    default :
        return PseudoAttacks[Pt][s];
    }
}

}  // namespace Stockfish::Attacks

#endif  // #ifndef ATTACKS_H_INCLUDED
