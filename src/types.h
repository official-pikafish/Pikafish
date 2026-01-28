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

#ifndef TYPES_H_INCLUDED
#define TYPES_H_INCLUDED

#include <cstdint>

#include "misc.h"

namespace Stockfish {

// -------------------------------------------------------------------------
// 【编码助手修改】这里添加了 TianTian 和 Chinese 规则定义
// -------------------------------------------------------------------------
enum Rule {
  SkyRule,
  Chinese,  // 天天象棋/中国规则
  Asian,
  TianTian, // 兼容天天象棋的别名
  RuleNone
};
// -------------------------------------------------------------------------

using Key      = uint64_t;
using Bitboard = uint64_t;

constexpr int MAX_MOVES = 256;
constexpr int MAX_PLY   = 246;

enum Color {
  WHITE,
  BLACK,
  COLOR_NB = 2
};

enum CastlingRights {
  NO_CASTLING,
  WHITE_OO,
  WHITE_OOO = WHITE_OO << 1,
  BLACK_OO  = WHITE_OO << 2,
  BLACK_OOO = WHITE_OO << 3,

  KING_SIDE      = WHITE_OO | BLACK_OO,
  QUEEN_SIDE     = WHITE_OOO | BLACK_OOO,
  WHITE_CASTLING = WHITE_OO | WHITE_OOO,
  BLACK_CASTLING = BLACK_OO | BLACK_OOO,
  ANY_CASTLING   = WHITE_CASTLING | BLACK_CASTLING,

  CASTLING_RIGHT_NB = 16
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
  VALUE_MATE      = 32000,
  VALUE_INFINITE  = 32001,
  VALUE_NONE      = 32002,
  
  PawnValueMg   = 168, 
  PawnValueEg   = 208,
  AdvisorValueMg = 224, 
  AdvisorValueEg = 255,
  BishopValueMg = 224, 
  BishopValueEg = 255,
  CannonValueMg = 586, 
  CannonValueEg = 604,
  KnightValueMg = 586, 
  KnightValueEg = 604,
  RookValueMg   = 1162,
  RookValueEg   = 1184,

  PawnValue       = 208,
  AdvisorValue    = 255,
  BishopValue     = 255,
  CannonValue     = 604,
  KnightValue     = 604,
  RookValue       = 1184,
};

enum PieceType {
  NO_PIECE_TYPE,
  PAWN,
  ADVISOR,
  BISHOP,
  KNIGHT,
  ROOK,
  CANNON,
  KING,
  ALL_PIECES = 0,
  PIECE_TYPE_NB = 8
};

enum Piece {
  NO_PIECE,
  W_PAWN = PAWN,     W_ADVISOR, W_BISHOP, W_KNIGHT, W_ROOK, W_CANNON, W_KING,
  B_PAWN = PAWN + 8, B_ADVISOR, B_BISHOP, B_KNIGHT, B_ROOK, B_CANNON, B_KING,
  PIECE_NB = 16
};

constexpr Value PieceValue[PIECE_NB] = {
  VALUE_ZERO,
  PawnValue, AdvisorValue, BishopValue, KnightValue, RookValue, CannonValue, VALUE_ZERO, VALUE_ZERO,
  PawnValue, AdvisorValue, BishopValue, KnightValue, RookValue, CannonValue, VALUE_ZERO, VALUE_ZERO
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
  SQ_NB,
  SQ_ZERO = 0
};

enum Direction : int {
  NORTH =  9,
  EAST  =  1,
  SOUTH = -9,
  WEST  = -1,

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

// 宏定义和工具函数
#define ENABLE_BASE_OPERATORS_ON(T)                                \
  constexpr T operator+(T d1, int d2) { return T(int(d1) + d2); }  \
  constexpr T operator-(T d1, int d2) { return T(int(d1) - d2); }  \
  constexpr T operator-(T d,) { return T(-int(d)); }               \
  inline T&   operator+=(T& d1, int d2) { return d1 = d1 + d2; }   \
  inline T&   operator-=(T& d1, int d2) { return d1 = d1 - d2; }

ENABLE_BASE_OPERATORS_ON(Value)
ENABLE_BASE_OPERATORS_ON(Direction)

constexpr Square operator+(Square s, Direction d) { return Square(int(s) + int(d)); }
constexpr Square operator-(Square s, Direction d) { return Square(int(s) - int(d)); }
inline Square&   operator+=(Square& s, Direction d) { return s = s + d; }
inline Square&   operator-=(Square& s, Direction d) { return s = s - d; }

constexpr File file_of(Square s) { return File(s % 9); }
constexpr Rank rank_of(Square s) { return Rank(s / 9); }
constexpr Square make_square(File f, Rank r) { return Square(r * 9 + f); }

constexpr bool is_ok(Square s) { return s >= SQ_A0 && s < SQ_NB; }
constexpr File relative_file(Color c, File f) { return f; } 
constexpr Rank relative_rank(Color c, Rank r) { return (c == WHITE ? r : Rank(RANK_9 - r)); }
constexpr Square relative_square(Color c, Square s) { return make_square(relative_file(c, file_of(s)), relative_rank(c, rank_of(s))); }
constexpr Direction relative_dir(Color c, Direction d) { return c == WHITE ? d : Direction(-d); }

constexpr Color operator~(Color c) { return Color(c ^ BLACK); }
constexpr Square from_sq(uint16_t m) { return Square((m >> 0) & 0xFF); }
constexpr Square to_sq(uint16_t m)   { return Square((m >> 8) & 0xFF); }

} // namespace Stockfish

#endif // TYPES_H_INCLUDED
