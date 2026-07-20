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

#ifndef BITBOARD_H_INCLUDED
#define BITBOARD_H_INCLUDED

#include <algorithm>
#include <array>
#include <cassert>
#include <string>
#include <type_traits>

#include "types.h"
#include "misc.h"

namespace Stockfish {

namespace Bitboards {

std::string pretty(Bitboard b);

}  // namespace Stockfish::Bitboards

constexpr Bitboard Palace = Bitboard(0x70381CULL) << 64 | Bitboard(0xE07038ULL);

constexpr Bitboard FileABB = Bitboard(0x20100ULL) << 64 | Bitboard(0x8040201008040201ULL);
constexpr Bitboard FileBBB = FileABB << 1;
constexpr Bitboard FileCBB = FileABB << 2;
constexpr Bitboard FileDBB = FileABB << 3;
constexpr Bitboard FileEBB = FileABB << 4;
constexpr Bitboard FileFBB = FileABB << 5;
constexpr Bitboard FileGBB = FileABB << 6;
constexpr Bitboard FileHBB = FileABB << 7;
constexpr Bitboard FileIBB = FileABB << 8;

constexpr Bitboard Rank0BB = 0x1FF;
constexpr Bitboard Rank1BB = Rank0BB << (FILE_NB * 1);
constexpr Bitboard Rank2BB = Rank0BB << (FILE_NB * 2);
constexpr Bitboard Rank3BB = Rank0BB << (FILE_NB * 3);
constexpr Bitboard Rank4BB = Rank0BB << (FILE_NB * 4);
constexpr Bitboard Rank5BB = Rank0BB << (FILE_NB * 5);
constexpr Bitboard Rank6BB = Rank0BB << (FILE_NB * 6);
constexpr Bitboard Rank7BB = Rank0BB << (FILE_NB * 7);
constexpr Bitboard Rank8BB = Rank0BB << (FILE_NB * 8);
constexpr Bitboard Rank9BB = Rank0BB << (FILE_NB * 9);


constexpr Bitboard PawnFileBB = FileABB | FileCBB | FileEBB | FileGBB | FileIBB;
constexpr Bitboard HalfBB[2]  = {Rank0BB | Rank1BB | Rank2BB | Rank3BB | Rank4BB,
                                 Rank5BB | Rank6BB | Rank7BB | Rank8BB | Rank9BB};
constexpr Bitboard PawnBB[2]  = {HalfBB[BLACK] | ((Rank3BB | Rank4BB) & PawnFileBB),
                                 HalfBB[WHITE] | ((Rank6BB | Rank5BB) & PawnFileBB)};

constexpr auto SquareBB = [] {
    std::array<Bitboard, SQUARE_NB> SquareBB{};
    for (Square s = SQ_A0; s <= SQ_I9; ++s)
        SquareBB[s] = (Bitboard(1ULL) << u8(s));
    return SquareBB;
}();

constexpr Bitboard square_bb(Square s) {
    assert(is_ok(s));
    return SquareBB[s];
}


// Overloads of bitwise operators between a Bitboard and a Square for testing
// whether a given bit is set in a bitboard, and for setting and clearing bits.

constexpr Bitboard  operator&(Bitboard b, Square s) { return b & square_bb(s); }
constexpr Bitboard  operator|(Bitboard b, Square s) { return b | square_bb(s); }
constexpr Bitboard  operator^(Bitboard b, Square s) { return b ^ square_bb(s); }
constexpr Bitboard& operator|=(Bitboard& b, Square s) { return b |= square_bb(s); }
constexpr Bitboard& operator^=(Bitboard& b, Square s) { return b ^= square_bb(s); }

constexpr Bitboard operator&(Square s, Bitboard b) { return b & s; }
constexpr Bitboard operator|(Square s, Bitboard b) { return b | s; }
constexpr Bitboard operator^(Square s, Bitboard b) { return b ^ s; }

constexpr Bitboard operator|(Square s1, Square s2) { return square_bb(s1) | s2; }

constexpr bool more_than_one(Bitboard b) { return bool(b & (b - 1)); }


// rank_bb() and file_bb() return a bitboard representing all the squares on
// the given file or rank.

constexpr Bitboard rank_bb(Rank r) { return Rank0BB << (FILE_NB * r); }

constexpr Bitboard rank_bb(Square s) { return rank_bb(rank_of(s)); }

constexpr Bitboard file_bb(File f) { return FileABB << u8(f); }

constexpr Bitboard file_bb(Square s) { return file_bb(file_of(s)); }


// Moves a bitboard one or two steps as specified by the direction D
template<Direction D>
constexpr Bitboard shift(Bitboard b) {
    return D == NORTH         ? (b & ~Rank9BB) << u8(NORTH)
         : D == SOUTH         ? b >> u8(NORTH)
         : D == NORTH + NORTH ? (b & ~Rank9BB & ~Rank8BB) << u8(NORTH + NORTH)
         : D == SOUTH + SOUTH ? b >> u8(NORTH + NORTH)
         : D == EAST          ? (b & ~FileIBB) << u8(EAST)
         : D == WEST          ? (b & ~FileABB) >> u8(EAST)
         : D == NORTH_EAST    ? (b & ~FileIBB) << u8(NORTH_EAST)
         : D == NORTH_WEST    ? (b & ~FileABB) << u8(NORTH_WEST)
         : D == SOUTH_EAST    ? (b & ~FileIBB) >> u8(NORTH_WEST)
         : D == SOUTH_WEST    ? (b & ~FileABB) >> u8(NORTH_EAST)
                              : Bitboard(0);
}


constexpr auto abs = [](int x) { return x > 0 ? x : -x; };

// distance() functions return the distance between x and y, defined as the
// number of steps for a king in x to reach y.

template<typename T1 = Square>
constexpr int distance(Square x, Square y);

template<>
constexpr int distance<File>(Square x, Square y) {
    return abs(file_of(x) - file_of(y));
}

template<>
constexpr int distance<Rank>(Square x, Square y) {
    return abs(rank_of(x) - rank_of(y));
}

constexpr auto SquareDistance = [] {
    std::array<std::array<u8, SQUARE_NB>, SQUARE_NB> SquareDistance{};
    for (Square s1 = SQ_A0; s1 <= SQ_I9; ++s1)
        for (Square s2 = SQ_A0; s2 <= SQ_I9; ++s2)
            SquareDistance[s1][s2] = std::max(distance<File>(s1, s2), distance<Rank>(s1, s2));
    return SquareDistance;
}();

template<>
constexpr int distance<Square>(Square x, Square y) {
    return SquareDistance[x][y];
}


// Counts the number of non-zero bits in a bitboard
inline int popcount(Bitboard b) {

#ifdef _MSC_VER

    return int(_mm_popcnt_u64(b._Word[1])) + int(_mm_popcnt_u64(b._Word[0]));

#else  // Assumed gcc or compatible compiler

    return __builtin_popcountll(b >> 64) + __builtin_popcountll(b);

#endif
}

// Return the least significant bit in a non-zero bitboard
inline Square lsb(Bitboard b) {
    assert(b);

#if defined(_MSC_VER)  // MSVC

    unsigned long idx;
    if (b._Word[0])
    {
        _BitScanForward64(&idx, b._Word[0]);
        return Square(idx);
    }
    else
    {
        _BitScanForward64(&idx, b._Word[1]);
        return Square(idx + 64);
    }

#else  // Assumed gcc or compatible compiler

    if (u64(b))
        return Square(__builtin_ctzll(b));
    return Square(__builtin_ctzll(b >> 64) + 64);

#endif
}

// Returns the most significant bit in a 64 bit integer.
inline int msb(u64 b) {
    assert(b);

#if defined(_MSC_VER)

    unsigned long idx;
    _BitScanReverse64(&idx, b);
    return idx;

#else  // Assumed gcc or compatible compiler

    return 63 ^ __builtin_clzll(b);

#endif
}

// Returns the bitboard of the least significant
// square of a non-zero bitboard. It is equivalent to square_bb(lsb(bb)).
inline Bitboard least_significant_square_bb(Bitboard b) {
    assert(b);
    return b & -b;
}

// Finds and clears the least significant bit in a non-zero bitboard
inline Square pop_lsb(Bitboard& b) {
    assert(b);
    const Square s = lsb(b);
    b &= b - 1;
    return s;
}

// Visits the squares of a bitboard in ascending order using 64-bit scans.
template<typename F>
inline void for_each_square(Bitboard b, F&& f) {
    u64 bits = u64(b);
    while (bits)
    {
        f(lsb(Bitboard(bits)));
        bits &= bits - 1;
    }

    bits = u64(b >> 64);
    while (bits)
    {
        f(Square(lsb(Bitboard(bits)) + 64));
        bits &= bits - 1;
    }
}

}  // namespace Stockfish

#endif  // #ifndef BITBOARD_H_INCLUDED
