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

#include "attacks.h"

#include <array>

#ifndef USE_PEXT
    #include "magics.h"
#endif

#include "misc.h"

namespace Stockfish::Attacks {

namespace {

std::array<MagicMask, 0x108000> RookTable;      // To store rook attacks
std::array<MagicMask, 0x108000> CannonTable;    // To store cannon attacks
std::array<MagicMask, 0x228>    BishopTable;    // To store bishop attacks
std::array<MagicMask, 0x380>    KnightTable;    // To store knight attacks
std::array<MagicMask, 0x3E0>    KnightToTable;  // To store by knight attacks

template<PieceType pt>
std::array<Magic, SQUARE_NB>
init_magics(MagicMask table[] IF_NOT_PEXT(, const Bitboard magicsInit[]));

// Initialized at startup in declaration order, the cannon magics reuse the rook masks
const std::array<Magic, SQUARE_NB> RookMagics =
  init_magics<ROOK>(RookTable.data() IF_NOT_PEXT(, RookMagicsInit));
const std::array<Magic, SQUARE_NB> CannonMagics =
  init_magics<CANNON>(CannonTable.data() IF_NOT_PEXT(, RookMagicsInit));
const std::array<Magic, SQUARE_NB> BishopMagics =
  init_magics<BISHOP>(BishopTable.data() IF_NOT_PEXT(, BishopMagicsInit));
const std::array<Magic, SQUARE_NB> KnightMagics =
  init_magics<KNIGHT>(KnightTable.data() IF_NOT_PEXT(, KnightMagicsInit));
const std::array<Magic, SQUARE_NB> KnightToMagics =
  init_magics<KNIGHT_TO>(KnightToTable.data() IF_NOT_PEXT(, KnightToMagicsInit));

}

alignas(64) extern const SquarePairBB LineBB = []() {
    SquarePairBB t{};
    for (Square s1 = SQ_A0; s1 <= SQ_I9; ++s1)
        for (Square s2 = SQ_A0; s2 <= SQ_I9; ++s2)
            if (attacks_bb(ROOK, s1) & s2)
                t[s1][s2] = (attacks_bb(ROOK, s1) & attacks_bb(ROOK, s2)) | s1 | s2;
    return t;
}();

alignas(64) extern const SquarePairBB BetweenBB = []() {
    SquarePairBB t{};
    for (Square s1 = SQ_A0; s1 <= SQ_I9; ++s1)
        for (Square s2 = SQ_A0; s2 <= SQ_I9; ++s2)
        {
            if (attacks_bb(ROOK, s1) & s2)
                t[s1][s2] =
                  (attacks_bb(ROOK, s1, square_bb(s2)) & attacks_bb(ROOK, s2, square_bb(s1)));

            if (attacks_bb(KNIGHT, s1) & s2)
                t[s1][s2] |= lame_leaper_path<KNIGHT_TO>(Direction(s2 - s1), s1);

            t[s1][s2] |= s2;
        }
    return t;
}();

alignas(64) extern const SquarePairBB inverse_RayPassBB = []() {
    SquarePairBB t{};
    for (Square s1 = SQ_A0; s1 <= SQ_I9; ++s1)
        for (Square s2 = SQ_A0; s2 <= SQ_I9; ++s2)
            if (attacks_bb(ROOK, s1) & s2)
                t[s2][s1] = attacks_bb(CANNON, s1, square_bb(s2));
    return t;
}();

alignas(64) extern const SquarePairBB LeaperPassBB = []() {
    SquarePairBB t{};
    for (Square s1 = SQ_A0; s1 <= SQ_I9; ++s1)
        for (Square s2 = SQ_A0; s2 <= SQ_I9; ++s2)
        {
            if (unconstrained_attacks_bb(KING, s1) & s2)
                t[s1][s2] |= attacks_bb(KNIGHT, s1) & unconstrained_attacks_bb(ADVISOR, s2);
            if (unconstrained_attacks_bb(ADVISOR, s1) & s2)
                t[s1][s2] |= attacks_bb(BISHOP, s1) & unconstrained_attacks_bb(ADVISOR, s2);
        }
    return t;
}();

const Magic& magic(Square s, PieceType pt) {

    assert(is_ok(s));

    switch (pt)
    {
    case ROOK :
        return RookMagics[s];
    case CANNON :
        return CannonMagics[s];
    case BISHOP :
        return BishopMagics[s];
    case KNIGHT :
        return KnightMagics[s];
    case KNIGHT_TO :
        return KnightToMagics[s];
    default :
        sf_unreachable();
    }
}

namespace {

// Computes all rook, cannon, bishop and knight attacks at startup.
template<PieceType pt>
std::array<Magic, SQUARE_NB>
init_magics(MagicMask table[] IF_NOT_PEXT(, const Bitboard magicsInit[])) {

    std::array<Magic, SQUARE_NB> magics{};
    Bitboard                     edges, b;
    u64                          size = 0;

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

#ifdef USE_PEXT
        if constexpr (pt == ROOK || pt == CANNON)
            m.pseudoAttacks = sliding_attack<ROOK>(s, 0);
        else
            m.pseudoAttacks = lame_leaper_attack<pt>(s, 0);

        m.shift = popcount(u64(m.mask));
#else
        m.magic = magicsInit[s];
        m.shift = 128 - popcount(m.mask);
#endif

        // Set the offset for the attacks table of the square. We have individual
        // table sizes for each square with "Fancy Magic Bitboards".
        m.attacks = s == SQ_A0 ? table : magics[s - 1].attacks + size;

        // Use Carry-Rippler trick to enumerate all subsets of masks[s] and
        // store the corresponding attack bitboard in m.attacks.
        b = size = 0;
        do
        {
            Bitboard attacks =
              pt == ROOK || pt == CANNON ? sliding_attack<pt>(s, b) : lame_leaper_attack<pt>(s, b);

#ifdef USE_PEXT
            m.attacks[m.index(b)] = u32(pext(attacks, m.pseudoAttacks, 16));
#else
            m.attacks[m.index(b)] = attacks;
#endif

            size++;
            b = (b - m.mask) & m.mask;
        } while (b);
    }

    return magics;
}

}

}  // namespace Stockfish::Attacks
