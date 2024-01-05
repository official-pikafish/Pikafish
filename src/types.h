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

#ifndef TYPES_H_INCLUDED
    #define TYPES_H_INCLUDED

// When compiling with provided Makefile (e.g. for Linux and OSX), configuration
// is done automatically. To get started type 'make help'.
//
// When Makefile is not used (e.g. with Microsoft Visual Studio) some switches
// need to be set manually:
//
// -DNDEBUG      | Disable debugging mode. Always use this for release.
//
// -DNO_PREFETCH | Disable use of prefetch asm-instruction. You may need this to
//               | run on some very old machines.
//
// -DUSE_POPCNT  | Add runtime support for use of popcnt asm-instruction. Works
//               | only in 64-bit mode and requires hardware with popcnt support.
//
// -DUSE_PEXT    | Add runtime support for use of pext asm-instruction. Works
//               | only in 64-bit mode and requires hardware with pext support.

    #include <cassert>
    #include <cstdint>

    #if defined(_MSC_VER)
        // Disable some silly and noisy warnings from MSVC compiler
        #pragma warning(disable: 4127)  // Conditional expression is constant
        #pragma warning(disable: 4146)  // Unary minus operator applied to unsigned type
        #pragma warning(disable: 4800)  // Forcing value to bool 'true' or 'false'
    #endif

// Predefined macros hell:
//
// __GNUC__                Compiler is GCC, Clang or ICX
// __clang__               Compiler is Clang or ICX
// __INTEL_LLVM_COMPILER   Compiler is ICX
// _MSC_VER                Compiler is MSVC
// _WIN32                  Building on Windows (any)
// _WIN64                  Building on Windows 64 bit

    #if defined(__GNUC__) && (__GNUC__ < 9 || (__GNUC__ == 9 && __GNUC_MINOR__ <= 2)) \
      && defined(_WIN32) && !defined(__clang__)
        #define ALIGNAS_ON_STACK_VARIABLES_BROKEN
    #endif

    #define ASSERT_ALIGNED(ptr, alignment) assert(reinterpret_cast<uintptr_t>(ptr) % alignment == 0)

    #if defined(_MSC_VER) && !defined(__clang__)
        #include <__msvc_int128.hpp>  // Microsoft header for std::_Unsigned128
using __uint128_t = std::_Unsigned128;
    #endif

    #if defined(_WIN64) && defined(_MSC_VER)  // No Makefile used
        #include <intrin.h>                   // Microsoft header for _BitScanForward64()
        #define IS_64BIT
    #endif

    #if defined(USE_POPCNT) && defined(_MSC_VER)
        #include <nmmintrin.h>  // Microsoft header for _mm_popcnt_u64()
    #endif

    #if !defined(NO_PREFETCH) && defined(_MSC_VER)
        #include <xmmintrin.h>  // Microsoft header for _mm_prefetch()
    #endif

    #if defined(USE_PEXT)
        #include <immintrin.h>  // Header for _pext_u64() intrinsic
        #if defined(_MSC_VER) && !defined(__clang__)
            #define pext(b, m, s) \
                ((_pext_u64(b._Word[1], m._Word[1]) << s) | _pext_u64(b._Word[0], m._Word[0]))
        #else
            #define pext(b, m, s) ((_pext_u64(b >> 64, m >> 64) << s) | _pext_u64(b, m))
        #endif
    #else
        #define pext(b, m, s) 0
    #endif

namespace Stockfish {

    #ifdef USE_POPCNT
constexpr bool HasPopCnt = true;
    #else
constexpr bool HasPopCnt = false;
    #endif

    #ifdef USE_PEXT
constexpr bool HasPext = true;
    #else
constexpr bool HasPext = false;
    #endif

    #ifdef IS_64BIT
constexpr bool Is64Bit = true;
    #else
constexpr bool Is64Bit = false;
    #endif

using Key      = uint64_t;
using Bitboard = __uint128_t;

constexpr int MAX_MOVES = 128;
constexpr int MAX_PLY   = 246;

enum Color {
    WHITE,
    BLACK,
    COLOR_NB = 2
};

enum Bound {
    BOUND_NONE,
    BOUND_UPPER,
    BOUND_LOWER,
    BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};

// Value is used as an alias for int16_t, this is done to differentiate between
// a search value and any other integer value. The values used in search are always
// supposed to be in the range (-VALUE_NONE, VALUE_NONE] and should not exceed this range.
using Value = int;

constexpr Value VALUE_ZERO     = 0;
constexpr Value VALUE_DRAW     = 0;
constexpr Value VALUE_NONE     = 32002;
constexpr Value VALUE_INFINITE = 32001;

constexpr Value VALUE_MATE             = 32000;
constexpr Value VALUE_MATE_IN_MAX_PLY  = VALUE_MATE - MAX_PLY;
constexpr Value VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY;

constexpr Value RookValue    = 1203;
constexpr Value AdvisorValue = 245;
constexpr Value CannonValue  = 686;
constexpr Value PawnValue    = 127;
constexpr Value KnightValue  = 893;
constexpr Value BishopValue  = 204;

// clang-format off
enum PieceType {
    NO_PIECE_TYPE, ROOK, ADVISOR, CANNON, PAWN, KNIGHT, BISHOP, KING, KNIGHT_TO,
    ALL_PIECES = 0,
    PIECE_TYPE_NB = 8
};

enum Piece {
    NO_PIECE,
    W_ROOK           , W_ADVISOR, W_CANNON, W_PAWN, W_KNIGHT, W_BISHOP, W_KING,
    B_ROOK = ROOK + 8, B_ADVISOR, B_CANNON, B_PAWN, B_KNIGHT, B_BISHOP, B_KING,
    PIECE_NB
};

constexpr Value PieceValue[PIECE_NB] = {
  VALUE_ZERO, RookValue,   AdvisorValue, CannonValue, PawnValue,  KnightValue, BishopValue,  VALUE_ZERO,
  VALUE_ZERO, RookValue,   AdvisorValue, CannonValue, PawnValue,  KnightValue, BishopValue,  VALUE_ZERO};
// clang-format on

using Depth = int;

enum : int {
    DEPTH_QS_CHECKS    = 0,
    DEPTH_QS_NO_CHECKS = -1,

    DEPTH_NONE = -6,

    DEPTH_OFFSET = -7  // value used only for TT entry occupancy check
};

// clang-format off
enum Square : int {
    SQ_A0, SQ_B0, SQ_C0, SQ_D0, SQ_E0, SQ_F0, SQ_G0, SQ_H0, SQ_I0,
    SQ_A1, SQ_B1, SQ_C1, SQ_D1, SQ_E1, SQ_F1, SQ_G1, SQ_H1, SQ_I1,
    SQ_A2, SQ_B2, SQ_C2, SQ_D2, SQ_E2, SQ_F2, SQ_G2, SQ_H2, SQ_I2,
    SQ_A3, SQ_B3, SQ_C3, SQ_D3, SQ_E3, SQ_F3, SQ_G3, SQ_H3, SQ_I3,
    SQ_A4, SQ_B4, SQ_C4, SQ_D4, SQ_E4, SQ_F4, SQ_G4, SQ_H4, SQ_I4,
    SQ_A5, SQ_B5, SQ_C5, SQ_D5, SQ_E5, SQ_F5, SQ_G5, SQ_H5, SQ_I5,
    SQ_A6, SQ_B6, SQ_C6, SQ_D6, SQ_E6, SQ_F6, SQ_G6, SQ_H6, SQ_I6,
    SQ_A7, SQ_B7, SQ_C7, SQ_D7, SQ_E7, SQ_F7, SQ_G7, SQ_H7, SQ_I7,
    SQ_A8, SQ_B8, SQ_C8, SQ_D8, SQ_E8, SQ_F8, SQ_G8, SQ_H8, SQ_I8,
    SQ_A9, SQ_B9, SQ_C9, SQ_D9, SQ_E9, SQ_F9, SQ_G9, SQ_H9, SQ_I9,
    SQ_NONE,

    SQUARE_ZERO = 0,
    SQUARE_NB   = 90
};
// clang-format on

enum Direction : int {
    NORTH = 9,
    EAST  = 1,
    SOUTH = -NORTH,
    WEST  = -EAST,

    NORTH_EAST = NORTH + EAST,
    SOUTH_EAST = SOUTH + EAST,
    SOUTH_WEST = SOUTH + WEST,
    NORTH_WEST = NORTH + WEST
};

enum File : int {
    FILE_A,
    FILE_B,
    FILE_C,
    FILE_D,
    FILE_E,
    FILE_F,
    FILE_G,
    FILE_H,
    FILE_I,
    FILE_NB
};

enum Rank : int {
    RANK_0,
    RANK_1,
    RANK_2,
    RANK_3,
    RANK_4,
    RANK_5,
    RANK_6,
    RANK_7,
    RANK_8,
    RANK_9,
    RANK_NB
};

// For fast repetition checks
struct BloomFilter {
    constexpr static uint64_t FILTER_SIZE = 1 << 14;
    uint8_t                   operator[](Key key) const { return table[key & (FILTER_SIZE - 1)]; }
    uint8_t&                  operator[](Key key) { return table[key & (FILTER_SIZE - 1)]; }

   private:
    uint8_t table[1 << 14];
};

// Keep track of what a move changes on the board (used by NNUE)
struct DirtyPiece {

    // Number of changed pieces
    int dirty_num;

    // Max 2 pieces can change in one move. A capture moves the captured
    // piece to SQ_NONE and the piece to the capture square.
    Piece piece[2];

    // From and to squares, which may be SQ_NONE
    Square from[2];
    Square to[2];

    bool requires_refresh[2];
};

    #define ENABLE_INCR_OPERATORS_ON(T) \
        inline T& operator++(T& d) { return d = T(int(d) + 1); } \
        inline T& operator--(T& d) { return d = T(int(d) - 1); }

ENABLE_INCR_OPERATORS_ON(PieceType)
ENABLE_INCR_OPERATORS_ON(Square)
ENABLE_INCR_OPERATORS_ON(File)
ENABLE_INCR_OPERATORS_ON(Rank)

    #undef ENABLE_INCR_OPERATORS_ON

constexpr Direction operator-(Direction d) { return Direction(-int(d)); }
constexpr Direction operator+(Direction d1, Direction d2) { return Direction(int(d1) + int(d2)); }
constexpr Direction operator*(int i, Direction d) { return Direction(i * int(d)); }

// Additional operators to add a Direction to a Square
constexpr Square operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
constexpr Square operator-(Square s, Direction d) { return Square(int(s) - int(d)); }
inline Square&   operator+=(Square& s, Direction d) { return s = s + d; }
inline Square&   operator-=(Square& s, Direction d) { return s = s - d; }

// Toggle color
constexpr Color operator~(Color c) { return Color(c ^ BLACK); }

// Swap color of piece B_KNIGHT <-> W_KNIGHT
constexpr Piece operator~(Piece pc) { return Piece(pc ^ 8); }

constexpr Value mate_in(int ply) { return VALUE_MATE - ply; }

constexpr Value mated_in(int ply) { return -VALUE_MATE + ply; }

constexpr Square make_square(File f, Rank r) { return Square(r * FILE_NB + f); }

constexpr Piece make_piece(Color c, PieceType pt) { return Piece((c << 3) + pt); }

constexpr PieceType type_of(Piece pc) { return PieceType(pc & 7); }

inline Color color_of(Piece pc) {
    assert(pc != NO_PIECE);
    return Color(pc >> 3);
}

constexpr bool is_ok(Square s) { return s >= SQ_A0 && s <= SQ_I9; }

constexpr File file_of(Square s) { return File(s % FILE_NB); }

constexpr Rank rank_of(Square s) { return Rank(s / FILE_NB); }

// Swap A0 <-> A9
constexpr Square flip_rank(Square s) { return make_square(file_of(s), Rank(RANK_9 - rank_of(s))); }

// Swap A0 <-> I0
constexpr Square flip_file(Square s) { return make_square(File(FILE_I - file_of(s)), rank_of(s)); }

// Based on a congruential pseudo-random number generator
constexpr Key make_key(uint64_t seed) {
    return seed * 6364136223846793005ULL + 1442695040888963407ULL;
}

// A move needs 16 bits to be stored
//
// bit  0- 6: destination square (from 0 to 89)
// bit  7-13: origin square (from 0 to 89)
//
// Special cases are Move::none() and Move::null(). We can sneak these in because in
// any normal move destination square is always different from origin square
// while Move::none() and Move::null() have the same origin and destination square.
class Move {
   public:
    Move() = default;
    constexpr explicit Move(std::uint16_t d) :
        data(d) {}

    constexpr Move(Square from, Square to) :
        data((from << 7) + to) {}

    static constexpr Move make(Square from, Square to) { return Move((from << 7) + to); }

    constexpr Square from_sq() const {
        assert(is_ok());
        return Square((data >> 7) & 0x7F);
    }

    constexpr Square to_sq() const {
        assert(is_ok());
        return Square(data & 0x7F);
    }

    constexpr int from_to() const { return data & 0x3FFF; }

    constexpr bool is_ok() const { return none().data != data && null().data != data; }

    static constexpr Move null() { return Move(129); }
    static constexpr Move none() { return Move(0); }

    constexpr bool operator==(const Move& m) const { return data == m.data; }
    constexpr bool operator!=(const Move& m) const { return data != m.data; }

    constexpr explicit operator bool() const { return data != 0; }

    constexpr std::uint16_t raw() const { return data; }

    struct MoveHash {
        std::size_t operator()(const Move& m) const { return m.data; }
    };

   protected:
    std::uint16_t data;
};

}  // namespace Stockfish

#endif  // #ifndef TYPES_H_INCLUDED

#include "tune.h"  // Global visibility to tuning setup
