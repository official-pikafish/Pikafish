/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2022 The Stockfish developers (see AUTHORS file)

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

/// When compiling with provided Makefile (e.g. for Linux and OSX), configuration
/// is done automatically. To get started type 'make help'.
///
/// When Makefile is not used (e.g. with Microsoft Visual Studio) some switches
/// need to be set manually:
///
/// -DNDEBUG      | Disable debugging mode. Always use this for release.
///
/// -DNO_PREFETCH | Disable use of prefetch asm-instruction. You may need this to
///               | run on some very old machines.
///
/// -DUSE_POPCNT  | Add runtime support for use of popcnt asm-instruction. Works
///               | only in 64-bit mode and requires hardware with popcnt support.
///
/// -DUSE_PEXT    | Add runtime support for use of pext asm-instruction. Works
///               | only in 64-bit mode and requires hardware with pext support.

#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <algorithm>

#if defined(_MSC_VER)
// Disable some silly and noisy warning from MSVC compiler
#pragma warning(disable: 4127) // Conditional expression is constant
#pragma warning(disable: 4146) // Unary minus operator applied to unsigned type
#pragma warning(disable: 4800) // Forcing value to bool 'true' or 'false'
#endif

/// Predefined macros hell:
///
/// __GNUC__           Compiler is gcc, Clang or Intel on Linux
/// __INTEL_COMPILER   Compiler is Intel
/// _MSC_VER           Compiler is MSVC or Intel on Windows
/// _WIN32             Building on Windows (any)
/// _WIN64             Building on Windows 64 bit

#if defined(__GNUC__ ) && (__GNUC__ < 9 || (__GNUC__ == 9 && __GNUC_MINOR__ <= 2)) && defined(_WIN32) && !defined(__clang__)
#define ALIGNAS_ON_STACK_VARIABLES_BROKEN
#endif

#define ASSERT_ALIGNED(ptr, alignment) assert(reinterpret_cast<uintptr_t>(ptr) % alignment == 0)

#if defined(_WIN64) && defined(_MSC_VER) // No Makefile used
#  include <intrin.h> // Microsoft header for _BitScanForward64()
#  define IS_64BIT
#endif

#if defined(USE_POPCNT) && (defined(__INTEL_COMPILER) || defined(_MSC_VER))
#  include <nmmintrin.h> // Intel and Microsoft header for _mm_popcnt_u64()
#endif

#if !defined(NO_PREFETCH) && (defined(__INTEL_COMPILER) || defined(_MSC_VER))
#  include <xmmintrin.h> // Intel and Microsoft header for _mm_prefetch()
#endif

#if defined(USE_PEXT)
#  include <immintrin.h> // Header for _pext_u64() intrinsic
#  define pext(b, m, s) ((_pext_u64(b >> 64, m >> 64) << s) | _pext_u64(b, m))
#else
#  define pext(b, m, s) 0
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


// For chasing detection
union ChaseMap {
    uint64_t attacks[4] { };
    uint16_t victims[16];

    // For adding victim <- attacker pair
    void operator |= (int id) {
        attacks[id >> 6] |= 1ULL << (id & 63);
    }

    // For exact diff
    ChaseMap& operator & (const ChaseMap &rhs) {
        attacks[0] &= ~rhs.attacks[0];
        attacks[1] &= ~rhs.attacks[1];
        attacks[2] &= ~rhs.attacks[2];
        attacks[3] &= ~rhs.attacks[3];
        return *this;
    }

    // For victims extraction
    operator uint16_t() {
        uint16_t ret = 0;
        for (int i = 0; i < 16; ++i)
            if (this->victims[i])
                ret |= 1 << i;
        return ret;
    }
};

typedef uint64_t Key;

#if defined(__GNUC__) && defined(IS_64BIT)
typedef __uint128_t Bitboard;
#else

struct Bitboard {
    uint64_t b64[2];

    constexpr Bitboard() : b64{ 0, 0 } {}
    constexpr Bitboard(uint64_t i) : b64{ 0, i } {}
    constexpr Bitboard(uint64_t hi, uint64_t lo) : b64{ hi, lo } {};

    constexpr operator bool() const {
        return b64[0] || b64[1];
    }

    constexpr operator long long unsigned() const {
        return b64[1];
    }

    constexpr operator unsigned() const {
        return b64[1];
    }

    constexpr Bitboard operator << (const unsigned int bits) const {
        return Bitboard(bits >= 64 ? b64[1] << (bits - 64)
            : bits == 0 ? b64[0]
            : ((b64[0] << bits) | (b64[1] >> (64 - bits))),
            bits >= 64 ? 0 : b64[1] << bits);
    }

    constexpr Bitboard operator >> (const unsigned int bits) const {
        return Bitboard(bits >= 64 ? 0 : b64[0] >> bits,
            bits >= 64 ? b64[0] >> (bits - 64)
            : bits == 0 ? b64[1]
            : ((b64[1] >> bits) | (b64[0] << (64 - bits))));
    }

    constexpr Bitboard operator << (const int bits) const {
        return *this << unsigned(bits);
    }

    constexpr Bitboard operator >> (const int bits) const {
        return *this >> unsigned(bits);
    }

    constexpr bool operator == (const Bitboard y) const {
        return (b64[0] == y.b64[0]) && (b64[1] == y.b64[1]);
    }

    constexpr bool operator != (const Bitboard y) const {
        return !(*this == y);
    }

    inline Bitboard& operator |=(const Bitboard x) {
        b64[0] |= x.b64[0];
        b64[1] |= x.b64[1];
        return *this;
    }
    inline Bitboard& operator &=(const Bitboard x) {
        b64[0] &= x.b64[0];
        b64[1] &= x.b64[1];
        return *this;
    }
    inline Bitboard& operator ^=(const Bitboard x) {
        b64[0] ^= x.b64[0];
        b64[1] ^= x.b64[1];
        return *this;
    }

    constexpr Bitboard operator ~ () const {
        return Bitboard(~b64[0], ~b64[1]);
    }

    constexpr Bitboard operator - () const {
        return Bitboard(-b64[0] - (b64[1] > 0), -b64[1]);
    }

    constexpr Bitboard operator | (const Bitboard x) const {
        return Bitboard(b64[0] | x.b64[0], b64[1] | x.b64[1]);
    }

    constexpr Bitboard operator & (const Bitboard x) const {
        return Bitboard(b64[0] & x.b64[0], b64[1] & x.b64[1]);
    }

    constexpr Bitboard operator ^ (const Bitboard x) const {
        return Bitboard(b64[0] ^ x.b64[0], b64[1] ^ x.b64[1]);
    }

    constexpr Bitboard operator - (const Bitboard x) const {
        return Bitboard(b64[0] - x.b64[0] - (b64[1] < x.b64[1]), b64[1] - x.b64[1]);
    }

    constexpr Bitboard operator - (const int x) const {
        return *this - Bitboard(x);
    }

    inline Bitboard operator * (const Bitboard x) const {
        uint64_t a_lo = (uint32_t)b64[1];
        uint64_t a_hi = b64[1] >> 32;
        uint64_t b_lo = (uint32_t)x.b64[1];
        uint64_t b_hi = x.b64[1] >> 32;

        uint64_t t1 = (a_hi * b_lo) + ((a_lo * b_lo) >> 32);
        uint64_t t2 = (a_lo * b_hi) + (t1 & 0xFFFFFFFF);

        return Bitboard(b64[0] * x.b64[1] + b64[1] * x.b64[0] + (a_hi * b_hi) + (t1 >> 32) + (t2 >> 32),
            (t2 << 32) + (a_lo * b_lo & 0xFFFFFFFF));
    }
};
#endif

constexpr int MAX_MOVES = 128;
constexpr int MAX_PLY   = 246;

/// A move needs 16 bits to be stored
///
/// bit  0- 6: destination square (from 0 to 89)
/// bit  7-13: origin square (from 0 to 89)
///
/// Special cases are MOVE_NONE and MOVE_NULL. We can sneak these in because in
/// any normal move destination square is always different from origin square
/// while MOVE_NONE and MOVE_NULL have the same origin and destination square.

enum Move : int {
  MOVE_NONE,
  MOVE_NULL = 129
};

enum Color {
  WHITE, BLACK, COLOR_NB = 2
};

enum Phase {
  PHASE_ENDGAME,
  PHASE_MIDGAME = 128,
  MG = 0, EG = 1, PHASE_NB = 2
};

enum Bound {
  BOUND_NONE,
  BOUND_UPPER,
  BOUND_LOWER,
  BOUND_EXACT = BOUND_UPPER | BOUND_LOWER
};

enum Value : int {
  VALUE_ZERO      = 0,
  VALUE_DRAW      = 0,
  VALUE_KNOWN_WIN = 10000,
  VALUE_MATE      = 32000,
  VALUE_INFINITE  = 32001,
  VALUE_NONE      = 32002,

  VALUE_MATE_IN_MAX_PLY  =  VALUE_MATE - MAX_PLY,
  VALUE_MATED_IN_MAX_PLY = -VALUE_MATE_IN_MAX_PLY,

  RookValueMg    = 1462,  RookValueEg    = 1268,
  AdvisorValueMg = 189 ,  AdvisorValueEg = 156 ,
  CannonValueMg  = 515 ,  CannonValueEg  = 674 ,
  PawnValueMg    = 72  ,  PawnValueEg    = 135 ,
  KnightValueMg  = 554 ,  KnightValueEg  = 744 ,
  BishopValueMg  = 202 ,  BishopValueEg  = 173 ,
};

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

constexpr Value PieceValue[PHASE_NB][PIECE_NB] = {
  { VALUE_ZERO, RookValueMg, AdvisorValueMg, CannonValueMg, PawnValueMg, KnightValueMg, BishopValueMg, VALUE_ZERO,
    VALUE_ZERO, RookValueMg, AdvisorValueMg, CannonValueMg, PawnValueMg, KnightValueMg, BishopValueMg, VALUE_ZERO },
  { VALUE_ZERO, RookValueEg, AdvisorValueEg, CannonValueEg, PawnValueEg, KnightValueEg, BishopValueEg, VALUE_ZERO,
    VALUE_ZERO, RookValueEg, AdvisorValueEg, CannonValueEg, PawnValueEg, KnightValueEg, BishopValueEg, VALUE_ZERO }
};

typedef int Depth;

enum : int {
  DEPTH_QS_CHECKS     =  0,
  DEPTH_QS_NO_CHECKS  = -1,
  DEPTH_QS_RECAPTURES = -5,

  DEPTH_NONE   = -6,

  DEPTH_OFFSET = -7 // value used only for TT entry occupancy check
};

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

enum Direction : int {
  NORTH =  9,
  EAST  =  1,
  SOUTH = -NORTH,
  WEST  = -EAST,

  NORTH_EAST = NORTH + EAST,
  SOUTH_EAST = SOUTH + EAST,
  SOUTH_WEST = SOUTH + WEST,
  NORTH_WEST = NORTH + WEST
};

enum File : int {
  FILE_A, FILE_B, FILE_C, FILE_D, FILE_E, FILE_F, FILE_G, FILE_H, FILE_I, FILE_NB
};

enum Rank : int {
  RANK_0, RANK_1, RANK_2, RANK_3, RANK_4, RANK_5, RANK_6, RANK_7, RANK_8, RANK_9, RANK_NB
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
};

/// Score enum stores a middlegame and an endgame value in a single integer (enum).
/// The least significant 16 bits are used to store the middlegame value and the
/// upper 16 bits are used to store the endgame value. We have to take care to
/// avoid left-shifting a signed int to avoid undefined behavior.
enum Score : int { SCORE_ZERO };

constexpr Score make_score(int mg, int eg) {
  return Score((int)((unsigned int)eg << 16) + mg);
}

/// Extracting the signed lower and upper 16 bits is not so trivial because
/// according to the standard a simple cast to short is implementation defined
/// and so is a right shift of a signed integer.
inline Value eg_value(Score s) {
  union { uint16_t u; int16_t s; } eg = { uint16_t(unsigned(s + 0x8000) >> 16) };
  return Value(eg.s);
}

inline Value mg_value(Score s) {
  union { uint16_t u; int16_t s; } mg = { uint16_t(unsigned(s)) };
  return Value(mg.s);
}

#define ENABLE_BASE_OPERATORS_ON(T)                                \
constexpr T operator+(T d1, int d2) { return T(int(d1) + d2); }    \
constexpr T operator-(T d1, int d2) { return T(int(d1) - d2); }    \
constexpr T operator-(T d) { return T(-int(d)); }                  \
inline T& operator+=(T& d1, int d2) { return d1 = d1 + d2; }       \
inline T& operator-=(T& d1, int d2) { return d1 = d1 - d2; }

#define ENABLE_INCR_OPERATORS_ON(T)                                \
inline T& operator++(T& d) { return d = T(int(d) + 1); }           \
inline T& operator--(T& d) { return d = T(int(d) - 1); }

#define ENABLE_FULL_OPERATORS_ON(T)                                \
ENABLE_BASE_OPERATORS_ON(T)                                        \
constexpr T operator*(int i, T d) { return T(i * int(d)); }        \
constexpr T operator*(T d, int i) { return T(int(d) * i); }        \
constexpr T operator/(T d, int i) { return T(int(d) / i); }        \
constexpr int operator/(T d1, T d2) { return int(d1) / int(d2); }  \
inline T& operator*=(T& d, int i) { return d = T(int(d) * i); }    \
inline T& operator/=(T& d, int i) { return d = T(int(d) / i); }

ENABLE_FULL_OPERATORS_ON(Value)
ENABLE_FULL_OPERATORS_ON(Direction)

ENABLE_INCR_OPERATORS_ON(Piece)
ENABLE_INCR_OPERATORS_ON(PieceType)
ENABLE_INCR_OPERATORS_ON(Square)
ENABLE_INCR_OPERATORS_ON(File)
ENABLE_INCR_OPERATORS_ON(Rank)

ENABLE_BASE_OPERATORS_ON(Score)

#undef ENABLE_FULL_OPERATORS_ON
#undef ENABLE_INCR_OPERATORS_ON
#undef ENABLE_BASE_OPERATORS_ON

/// Additional operators to add a Direction to a Square
constexpr Square operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
constexpr Square operator-(Square s, Direction d) { return Square(int(s) - int(d)); }
inline Square& operator+=(Square& s, Direction d) { return s = s + d; }
inline Square& operator-=(Square& s, Direction d) { return s = s - d; }

/// Only declared but not defined. We don't want to multiply two scores due to
/// a very high risk of overflow. So user should explicitly convert to integer.
Score operator*(Score, Score) = delete;

/// Division of a Score must be handled separately for each term
inline Score operator/(Score s, int i) {
  return make_score(mg_value(s) / i, eg_value(s) / i);
}

/// Multiplication of a Score by an integer. We check for overflow in debug mode.
inline Score operator*(Score s, int i) {

  Score result = Score(int(s) * i);

  assert(eg_value(result) == (i * eg_value(s)));
  assert(mg_value(result) == (i * mg_value(s)));
  assert((i == 0) || (result / i) == s);

  return result;
}

/// Multiplication of a Score by a boolean
inline Score operator*(Score s, bool b) {
  return b ? s : SCORE_ZERO;
}

constexpr Color operator~(Color c) {
  return Color(c ^ BLACK); // Toggle color
}

constexpr Value mate_in(int ply) {
  return VALUE_MATE - ply;
}

constexpr Value mated_in(int ply) {
  return -VALUE_MATE + ply;
}

constexpr Square make_square(File f, Rank r) {
  return Square(r * FILE_NB + f);
}

constexpr Piece make_piece(Color c, PieceType pt) {
  return Piece((c << 3) + pt);
}

constexpr PieceType type_of(Piece pc) {
  return PieceType(pc & 7);
}

inline Color color_of(Piece pc) {
  assert(pc != NO_PIECE);
  return Color(pc >> 3);
}

constexpr bool is_ok(Square s) {
  return s >= SQ_A0 && s <= SQ_I9;
}

constexpr File file_of(Square s) {
  return File(s % FILE_NB);
}

constexpr Rank rank_of(Square s) {
  return Rank(s / FILE_NB);
}

constexpr Square from_sq(Move m) {
  return Square(m >> 7);
}

constexpr Square to_sq(Move m) {
  return Square(m & 0x7F);
}

constexpr int from_to(Move m) {
  return m;
}

constexpr Move make_move(Square from, Square to) {
  return Move((from << 7) + to);
}

constexpr int make_chase(int piece1, int piece2) {
  return (piece1 << 4) + piece2;
}

constexpr bool is_ok(Move m) {
  return from_sq(m) != to_sq(m); // Catch MOVE_NULL and MOVE_NONE
}

/// Based on a congruential pseudo random number generator
constexpr Key make_key(uint64_t seed) {
  return seed * 6364136223846793005ULL + 1442695040888963407ULL;
}

} // namespace Stockfish

#endif // #ifndef TYPES_H_INCLUDED

#include "tune.h" // Global visibility to tuning setup
