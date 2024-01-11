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

#include "bitboard.h"

#include <algorithm>
#include <bitset>
#include <initializer_list>

#include <set>
#include <utility>
#include "magics.h"

namespace Stockfish {

uint8_t PopCnt16[1 << 16];
uint8_t SquareDistance[SQUARE_NB][SQUARE_NB];

Bitboard SquareBB[SQUARE_NB];
Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];
Bitboard PawnAttacks[COLOR_NB][SQUARE_NB];
Bitboard PawnAttacksTo[COLOR_NB][SQUARE_NB];

Magic RookMagics[SQUARE_NB];
Magic CannonMagics[SQUARE_NB];
Magic BishopMagics[SQUARE_NB];
Magic KnightMagics[SQUARE_NB];
Magic KnightToMagics[SQUARE_NB];

namespace {

Bitboard RookTable[0x108000];    // To store rook attacks
Bitboard CannonTable[0x108000];  // To store cannon attacks
Bitboard BishopTable[0x228];     // To store bishop attacks
Bitboard KnightTable[0x380];     // To store knight attacks
Bitboard KnightToTable[0x3E0];   // To store by knight attacks

const std::set<Direction> KnightDirections{2 * SOUTH + WEST, 2 * SOUTH + EAST, SOUTH + 2 * WEST,
                                           SOUTH + 2 * EAST, NORTH + 2 * WEST, NORTH + 2 * EAST,
                                           2 * NORTH + WEST, 2 * NORTH + EAST};
const std::set<Direction> BishopDirections{2 * NORTH_EAST, 2 * SOUTH_EAST, 2 * SOUTH_WEST,
                                           2 * NORTH_WEST};


template<PieceType pt>
void init_magics(Bitboard table[], Magic magics[], const Bitboard magicsInit[]);

template<PieceType pt>
Bitboard lame_leaper_path(Direction d, Square s);

}

// Returns the bitboard of target square for the given step
// from the given square. If the step is off the board, returns empty bitboard.
inline Bitboard safe_destination(Square s, int step) {
    Square to = Square(s + step);
    return is_ok(to) && distance(s, to) <= 2 ? square_bb(to) : Bitboard(0);
}


// Returns an ASCII representation of a bitboard suitable
// to be printed to standard output. Useful for debugging.
std::string Bitboards::pretty(Bitboard b) {

    std::string s = "+---+---+---+---+---+---+---+---+---+\n";

    for (Rank r = RANK_9; r >= RANK_0; --r)
    {
        for (File f = FILE_A; f <= FILE_I; ++f)
            s += b & make_square(f, r) ? "| X " : "|   ";

        s += "| " + std::to_string(r) + "\n+---+---+---+---+---+---+---+---+---+\n";
    }
    s += "  a   b   c   d   e   f   g   h   i\n";

    return s;
}


// Initializes various bitboard tables. It is called at
// startup and relies on global objects to be already zero-initialized.
void Bitboards::init() {

    for (unsigned i = 0; i < (1 << 16); ++i)
        PopCnt16[i] = uint8_t(std::bitset<16>(i).count());

    for (Square s = SQ_A0; s <= SQ_I9; ++s)
        SquareBB[s] = (Bitboard(1ULL) << std::uint8_t(s));

    for (Square s1 = SQ_A0; s1 <= SQ_I9; ++s1)
        for (Square s2 = SQ_A0; s2 <= SQ_I9; ++s2)
            SquareDistance[s1][s2] = std::max(distance<File>(s1, s2), distance<Rank>(s1, s2));

    init_magics<ROOK>(RookTable, RookMagics, RookMagicsInit);
    init_magics<CANNON>(CannonTable, CannonMagics, RookMagicsInit);
    init_magics<BISHOP>(BishopTable, BishopMagics, BishopMagicsInit);
    init_magics<KNIGHT>(KnightTable, KnightMagics, KnightMagicsInit);
    init_magics<KNIGHT_TO>(KnightToTable, KnightToMagics, KnightToMagicsInit);

    for (Square s1 = SQ_A0; s1 <= SQ_I9; ++s1)
    {
        PawnAttacks[WHITE][s1] = pawn_attacks_bb<WHITE>(s1);
        PawnAttacks[BLACK][s1] = pawn_attacks_bb<BLACK>(s1);

        PawnAttacksTo[WHITE][s1] = pawn_attacks_to_bb<WHITE>(s1);
        PawnAttacksTo[BLACK][s1] = pawn_attacks_to_bb<BLACK>(s1);

        PseudoAttacks[ROOK][s1]   = attacks_bb<ROOK>(s1, 0);
        PseudoAttacks[BISHOP][s1] = attacks_bb<BISHOP>(s1, 0);
        PseudoAttacks[KNIGHT][s1] = attacks_bb<KNIGHT>(s1, 0);

        // Only generate pseudo attacks in the palace squares for king and advisor
        if (Palace & s1)
        {
            for (int step : {NORTH, SOUTH, WEST, EAST})
                PseudoAttacks[KING][s1] |= safe_destination(s1, step);
            PseudoAttacks[KING][s1] &= Palace;

            for (int step : {NORTH_WEST, NORTH_EAST, SOUTH_WEST, SOUTH_EAST})
                PseudoAttacks[ADVISOR][s1] |= safe_destination(s1, step);
            PseudoAttacks[ADVISOR][s1] &= Palace;
        }

        for (Square s2 = SQ_A0; s2 <= SQ_I9; ++s2)
        {
            if (PseudoAttacks[ROOK][s1] & s2)
            {
                LineBB[s1][s2] = (attacks_bb(ROOK, s1, 0) & attacks_bb(ROOK, s2, 0)) | s1 | s2;
                BetweenBB[s1][s2] =
                  (attacks_bb(ROOK, s1, square_bb(s2)) & attacks_bb(ROOK, s2, square_bb(s1)));
            }

            if (PseudoAttacks[KNIGHT][s1] & s2)
                BetweenBB[s1][s2] |= lame_leaper_path<KNIGHT_TO>(Direction(s2 - s1), s1);

            BetweenBB[s1][s2] |= s2;
        }
    }
}

namespace {

template<PieceType pt>
Bitboard sliding_attack(Square sq, Bitboard occupied) {
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
Bitboard lame_leaper_path(Direction d, Square s) {
    Bitboard b  = 0;
    Square   to = s + d;
    if (!is_ok(to) || distance(s, to) >= 4)
        return b;

    // If piece type is by knight attacks, swap the source and destination square
    if (pt == KNIGHT_TO)
    {
        std::swap(s, to);
        d = -d;
    }

    Direction dr = d > 0 ? NORTH : SOUTH;
    Direction df = (std::abs(d % NORTH) < NORTH / 2 ? d % NORTH : -(d % NORTH)) < 0 ? WEST : EAST;

    int diff = std::abs(file_of(to) - file_of(s)) - std::abs(rank_of(to) - rank_of(s));
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
Bitboard lame_leaper_path(Square s) {
    Bitboard b = 0;
    for (const auto& d : pt == BISHOP ? BishopDirections : KnightDirections)
        b |= lame_leaper_path<pt>(d, s);
    if (pt == BISHOP)
        b &= HalfBB[rank_of(s) > RANK_4];
    return b;
}

template<PieceType pt>
Bitboard lame_leaper_attack(Square s, Bitboard occupied) {
    Bitboard b = 0;
    for (const auto& d : pt == BISHOP ? BishopDirections : KnightDirections)
    {
        Square to = s + d;
        if (is_ok(to) && distance(s, to) < 4 && !(lame_leaper_path<pt>(d, s) & occupied))
            b |= to;
    }
    if (pt == BISHOP)
        b &= HalfBB[rank_of(s) > RANK_4];
    return b;
}


// Computes all rook and bishop attacks at startup. Magic
// bitboards are used to look up attacks of sliding pieces. As a reference see
// www.chessprogramming.org/Magic_Bitboards. In particular, here we use the so
// called "fancy" approach.
template<PieceType pt>
void init_magics(Bitboard table[], Magic magics[], const Bitboard magicsInit[]) {

    Bitboard edges, b;
    uint64_t size = 0;

    for (Square s = SQ_A0; s <= SQ_I9; ++s)
    {
        // Board edges are not considered in the relevant occupancies
        edges = ((Rank0BB | Rank9BB) & ~rank_bb(s)) | ((FileABB | FileIBB) & ~file_bb(s));

        // Given a square 's', the mask is the bitboard of sliding attacks from
        // 's' computed on an empty board. The index must be big enough to contain
        // all the attacks for each possible subset of the mask and so is 2 power
        // the number of 1s of the mask.
        Magic& m = magics[s];
        m.mask   = pt == ROOK   ? sliding_attack<pt>(s, 0)
                 : pt == CANNON ? RookMagics[s].mask
                                : lame_leaper_path<pt>(s);
        if (pt != KNIGHT_TO)
            m.mask &= ~edges;

        if (HasPext)
            m.shift = popcount(uint64_t(m.mask));
        else
            m.shift = 128 - popcount(m.mask);

        m.magic = magicsInit[s];

        // Set the offset for the attacks table of the square. We have individual
        // table sizes for each square with "Fancy Magic Bitboards".
        m.attacks = s == SQ_A0 ? table : magics[s - 1].attacks + size;

        // Use Carry-Rippler trick to enumerate all subsets of masks[s] and
        // store the corresponding attack bitboard in m.attacks.
        b = size = 0;
        do
        {
            m.attacks[m.index(b)] =
              pt == ROOK || pt == CANNON ? sliding_attack<pt>(s, b) : lame_leaper_attack<pt>(s, b);

            size++;
            b = (b - m.mask) & m.mask;
        } while (b);
    }
}
}

}  // namespace Stockfish
