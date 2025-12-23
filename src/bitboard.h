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

#ifndef BITBOARD_H_INCLUDED
#define BITBOARD_H_INCLUDED

#include <algorithm>
#include <cassert>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "types.h"

#ifdef USE_PEXT
    #define IF_NOT_PEXT(...)
#else
    #define IF_NOT_PEXT(...) __VA_ARGS__
#endif

namespace Stockfish {

namespace Bitboards {

void        init();
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

extern uint8_t PopCnt16[1 << 16];

extern Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
extern Bitboard RayPassBB[SQUARE_NB][SQUARE_NB];
extern Bitboard LeaperPassBB[SQUARE_NB][SQUARE_NB];
extern Bitboard LineBB[SQUARE_NB][SQUARE_NB];

// Magic holds all magic bitboards relevant data for a single square
struct Magic {
    Bitboard  mask;
    Bitboard* attacks;
    unsigned  shift;
    IF_NOT_PEXT(Bitboard magic;)

    // Compute the attack's index using the 'magic bitboards' approach
    unsigned index(Bitboard occupied) const {

#ifdef USE_PEXT
        return unsigned(pext(occupied, mask, shift));
#else
        return unsigned(((occupied & mask) * magic) >> shift);
#endif
    }
};

extern Magic RookMagics[SQUARE_NB];
extern Magic CannonMagics[SQUARE_NB];
extern Magic BishopMagics[SQUARE_NB];
extern Magic KnightMagics[SQUARE_NB];
extern Magic KnightToMagics[SQUARE_NB];

constexpr auto SquareBB = [] {
    std::array<Bitboard, SQUARE_NB> SquareBB{};
    for (Square s = SQ_A0; s <= SQ_I9; ++s)
        SquareBB[s] = (Bitboard(1ULL) << std::uint8_t(s));
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

constexpr Bitboard file_bb(File f) { return FileABB << std::uint8_t(f); }

constexpr Bitboard file_bb(Square s) { return file_bb(file_of(s)); }


// Moves a bitboard one or two steps as specified by the direction D
template<Direction D>
constexpr Bitboard shift(Bitboard b) {
    return D == NORTH         ? (b & ~Rank9BB) << std::uint8_t(NORTH)
         : D == SOUTH         ? b >> std::uint8_t(NORTH)
         : D == NORTH + NORTH ? (b & ~Rank9BB & ~Rank8BB) << std::uint8_t(NORTH + NORTH)
         : D == SOUTH + SOUTH ? b >> std::uint8_t(NORTH + NORTH)
         : D == EAST          ? (b & ~FileIBB) << std::uint8_t(EAST)
         : D == WEST          ? (b & ~FileABB) >> std::uint8_t(EAST)
         : D == NORTH_EAST    ? (b & ~FileIBB) << std::uint8_t(NORTH_EAST)
         : D == NORTH_WEST    ? (b & ~FileABB) << std::uint8_t(NORTH_WEST)
         : D == SOUTH_EAST    ? (b & ~FileIBB) >> std::uint8_t(NORTH_WEST)
         : D == SOUTH_WEST    ? (b & ~FileABB) >> std::uint8_t(NORTH_EAST)
                              : Bitboard(0);
}


// Returns the squares attacked by pawns of the given color
// from the squares in the given bitboard.
template<Color C>
constexpr Bitboard pawn_attacks_bb(Square s) {
    Bitboard b      = square_bb(s);
    Bitboard attack = shift<C == WHITE ? NORTH : SOUTH>(b);
    if ((C == WHITE && rank_of(s) > RANK_4) || (C == BLACK && rank_of(s) < RANK_5))
        attack |= shift<WEST>(b) | shift<EAST>(b);
    return attack;
}


// Returns the squares that if there is a pawn
// of the given color in there, it can attack the square s
template<Color C>
constexpr Bitboard pawn_attacks_to_bb(Square s) {
    Bitboard b      = square_bb(s);
    Bitboard attack = shift<C == WHITE ? SOUTH : NORTH>(b);
    if ((C == WHITE && rank_of(s) > RANK_4) || (C == BLACK && rank_of(s) < RANK_5))
        attack |= shift<WEST>(b) | shift<EAST>(b);
    return attack;
}


// Returns a bitboard representing an entire line (from board edge
// to board edge) that intersects the two given squares. If the given squares
// are not on a same file/rank/diagonal, the function returns 0. For instance,
// line_bb(SQ_C4, SQ_F7) will return a bitboard with the A2-G8 diagonal.
inline Bitboard line_bb(Square s1, Square s2) {

    assert(is_ok(s1) && is_ok(s2));
    return LineBB[s1][s2];
}


// Returns a bitboard representing the squares in the semi-open
// segment between the squares s1 and s2 (excluding s1 but including s2). If the
// given squares are not on a same file/rank/diagonal, it returns s2. For instance,
// between_bb(SQ_C4, SQ_F7) will return a bitboard with squares D5, E6 and F7, but
// between_bb(SQ_E6, SQ_F8) will return a bitboard with the square F8. This trick
// allows to generate non-king evasion moves faster: the defending piece must either
// interpose itself to cover the check or capture the checking piece.
inline Bitboard between_bb(Square s1, Square s2) {

    assert(is_ok(s1) && is_ok(s2));
    return BetweenBB[s1][s2];
}


inline Bitboard ray_pass_bb(Square s1, Square s2) {

    assert(is_ok(s1) && is_ok(s2));
    return RayPassBB[s1][s2];
}


inline Bitboard leaper_pass_bb(Square s1, Square s2) {

    assert(is_ok(s1) && is_ok(s2));
    return LeaperPassBB[s1][s2];
}


// Returns true if the squares s1, s2 and s3 are aligned either on a
// straight or on a diagonal line.
inline bool aligned(Square s1, Square s2, Square s3) { return bool(line_bb(s1, s2) & s3); }

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
    std::array<std::array<uint8_t, SQUARE_NB>, SQUARE_NB> SquareDistance{};
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

#ifndef USE_POPCNT

    std::uint16_t indices[8];
    std::memcpy(indices, &b, sizeof(b));
    return PopCnt16[indices[0]] + PopCnt16[indices[1]] + PopCnt16[indices[2]] + PopCnt16[indices[3]]
         + PopCnt16[indices[4]] + PopCnt16[indices[5]] + PopCnt16[indices[6]]
         + PopCnt16[indices[7]];

#elif defined(_MSC_VER)

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

    if (uint64_t(b))
        return Square(__builtin_ctzll(b));
    return Square(__builtin_ctzll(b >> 64) + 64);

#endif
}

// Returns the most significant bit in a non-zero bitboard.
inline Square msb(Bitboard b) {
    assert(b);

#if defined(_MSC_VER)

    unsigned long idx;
    if (b._Word[1])
    {
        _BitScanReverse(&idx, b._Word[1]);
        return Square(idx);
    }
    else
    {
        _BitScanReverse(&idx, b._Word[0]);
        return Square(idx + 64);
    }

#else  // Assumed gcc or compatible compiler

    if (uint64_t(b >> 64))
        return Square(__builtin_clzll(b));
    return Square(__builtin_clzll(b) + 64);

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

namespace Bitboards {
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

}

constexpr auto PseudoAttacks = [] {
    std::array<std::array<Bitboard, SQUARE_NB>, PIECE_TYPE_NB + 3> PseudoAttacks{};

    for (Square s1 = SQ_A0; s1 <= SQ_I9; ++s1)
    {
        PseudoAttacks[NO_PIECE_TYPE][s1] = pawn_attacks_bb<WHITE>(s1);
        PseudoAttacks[PAWN][s1]          = pawn_attacks_bb<BLACK>(s1);

        PseudoAttacks[PAWN_TO - 1][s1] = pawn_attacks_to_bb<WHITE>(s1);
        PseudoAttacks[PAWN_TO][s1]     = pawn_attacks_to_bb<BLACK>(s1);

        PseudoAttacks[ROOK][s1]   = Bitboards::sliding_attack<ROOK>(s1, 0);
        PseudoAttacks[BISHOP][s1] = Bitboards::lame_leaper_attack<BISHOP>(s1, 0);
        PseudoAttacks[KNIGHT][s1] = Bitboards::lame_leaper_attack<KNIGHT>(s1, 0);

        // Only generate pseudo attacks in the palace squares for king and advisor
        for (int step : {NORTH, SOUTH, WEST, EAST})
        {
            if (Palace & s1)
                PseudoAttacks[KING][s1] |= Bitboards::safe_destination(s1, step) & Palace;
            // Unconstrained king
            PseudoAttacks[KING + 3][s1] |= Bitboards::safe_destination(s1, step);
        }

        for (int step : {NORTH_WEST, NORTH_EAST, SOUTH_WEST, SOUTH_EAST})
        {
            if (Palace & s1)
                PseudoAttacks[ADVISOR][s1] |= Bitboards::safe_destination(s1, step) & Palace;
            // Unconstrained advisor
            PseudoAttacks[ADVISOR + 1][s1] |= Bitboards::safe_destination(s1, step);
        }
    }

    return PseudoAttacks;
}();

// Returns the pseudo attacks of the given piece type
// assuming an empty board.
template<PieceType Pt>
inline Bitboard attacks_bb(Square s, Color c = COLOR_NB) {

    assert((Pt != KNIGHT_TO) && ((Pt != PAWN && Pt != PAWN_TO) || c < COLOR_NB) && (is_ok(s)));
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

    assert((Pt != PAWN) && (Pt != PAWN_TO) && (is_ok(s)));

    switch (Pt)
    {
    case ROOK :
        return RookMagics[s].attacks[RookMagics[s].index(occupied)];
    case CANNON :
        return CannonMagics[s].attacks[CannonMagics[s].index(occupied)];
    case BISHOP :
        return BishopMagics[s].attacks[BishopMagics[s].index(occupied)];
    case KNIGHT :
        return KnightMagics[s].attacks[KnightMagics[s].index(occupied)];
    case KNIGHT_TO :
        return KnightToMagics[s].attacks[KnightToMagics[s].index(occupied)];
    default :
        return PseudoAttacks[Pt][s];
    }
}

// Returns the attacks by the given piece
// assuming the board is occupied according to the passed Bitboard.
// Sliding piece attacks do not continue passed an occupied square.
inline Bitboard attacks_bb(PieceType pt, Square s, Bitboard occupied) {

    assert((pt != PAWN) && (pt < KNIGHT_TO) && (is_ok(s)));

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

    assert((Pt == KING || Pt == ADVISOR) && (is_ok(s)));

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

}  // namespace Stockfish

#endif  // #ifndef BITBOARD_H_INCLUDED
