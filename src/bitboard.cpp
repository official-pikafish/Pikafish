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

#include "bitboard.h"

#include <bitset>
#ifndef USE_PEXT
    #include "magics.h"
#endif

namespace Stockfish {

uint8_t PopCnt16[1 << 16];

Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard RayPassBB[SQUARE_NB][SQUARE_NB];
Bitboard LeaperPassBB[SQUARE_NB][SQUARE_NB];

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


template<PieceType pt>
void init_magics(Bitboard table[], Magic magics[] IF_NOT_PEXT(, const Bitboard magicsInit[]));

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

    init_magics<ROOK>(RookTable, RookMagics IF_NOT_PEXT(, RookMagicsInit));
    init_magics<CANNON>(CannonTable, CannonMagics IF_NOT_PEXT(, RookMagicsInit));
    init_magics<BISHOP>(BishopTable, BishopMagics IF_NOT_PEXT(, BishopMagicsInit));
    init_magics<KNIGHT>(KnightTable, KnightMagics IF_NOT_PEXT(, KnightMagicsInit));
    init_magics<KNIGHT_TO>(KnightToTable, KnightToMagics IF_NOT_PEXT(, KnightToMagicsInit));

    for (Square s1 = SQ_A0; s1 <= SQ_I9; ++s1)
    {
        for (Square s2 = SQ_A0; s2 <= SQ_I9; ++s2)
        {
            if (PseudoAttacks[ROOK][s1] & s2)
            {
                LineBB[s1][s2] = (attacks_bb<ROOK>(s1) & attacks_bb<ROOK>(s2)) | s1 | s2;
                BetweenBB[s1][s2] =
                  (attacks_bb<ROOK>(s1, square_bb(s2)) & attacks_bb<ROOK>(s2, square_bb(s1)));
                RayPassBB[s1][s2] = attacks_bb<CANNON>(s1, square_bb(s2));
            }

            if (PseudoAttacks[KNIGHT][s1] & s2)
                BetweenBB[s1][s2] |= lame_leaper_path<KNIGHT_TO>(Direction(s2 - s1), s1);

            BetweenBB[s1][s2] |= s2;
        }
    }

    for (Square s1 = SQ_A0; s1 <= SQ_I9; ++s1)
    {
        for (Square s2 = SQ_A0; s2 <= SQ_I9; ++s2)
        {
            if (unconstrained_attacks_bb<KING>(s1) & s2)
                LeaperPassBB[s1][s2] |=
                  attacks_bb<KNIGHT>(s1) & unconstrained_attacks_bb<ADVISOR>(s2);
            if (unconstrained_attacks_bb<ADVISOR>(s1) & s2)
                LeaperPassBB[s1][s2] |=
                  attacks_bb<BISHOP>(s1) & unconstrained_attacks_bb<ADVISOR>(s2);
        }
    }
}

namespace {

// Computes all rook and bishop attacks at startup. Magic
// bitboards are used to look up attacks of sliding pieces. As a reference see
// https://www.chessprogramming.org/Magic_Bitboards. In particular, here we use
// the so called "fancy" approach.
template<PieceType pt>
void init_magics(Bitboard table[], Magic magics[] IF_NOT_PEXT(, const Bitboard magicsInit[])) {

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
        m.mask   = pt == ROOK   ? Bitboards::sliding_attack<pt>(s, 0)
                 : pt == CANNON ? RookMagics[s].mask
                                : Bitboards::lame_leaper_path<pt>(s);
        if (pt != KNIGHT_TO)
            m.mask &= ~edges;

#ifdef USE_PEXT
        m.shift = popcount(uint64_t(m.mask));
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
            m.attacks[m.index(b)] = pt == ROOK || pt == CANNON
                                    ? Bitboards::sliding_attack<pt>(s, b)
                                    : Bitboards::lame_leaper_attack<pt>(s, b);

            size++;
            b = (b - m.mask) & m.mask;
        } while (b);
    }
}
}

}  // namespace Stockfish
