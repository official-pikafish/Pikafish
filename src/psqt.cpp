/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2023 The Stockfish developers (see AUTHORS file)

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


#include "psqt.h"

#include <algorithm>

#include "bitboard.h"
#include "types.h"

namespace Stockfish {

namespace
{

auto constexpr S = make_score;

// 'Bonus' contains Piece-Square parameters.
// Scores are explicit for files A to E, implicitly mirrored for E to I.
constexpr Score Bonus[][RANK_NB][int(FILE_NB) / 2 + 1] = {
  { },
  { // Rook
   { S( 161,-294), S(-100, 276), S(-148, 169), S( 191, -21), S(  35, -12)},
   { S(  11, -21), S( -33,-241), S( 122,-356), S(-190, 201), S(-141,-350)},
   { S(  58, -82), S(-168,  64), S(  81,  39), S(  91, 255), S(-161,  -3)},
   { S(-152,  35), S(  71, -98), S(  -1,  72), S( 106, 246), S( 103,-117)},
   { S(-428,-109), S( 134,  17), S( -30,-117), S( 303, -33), S( -69, -56)},
   { S(-148, -15), S( 152,  33), S( 170, 102), S(  13, 191), S( 133, -93)},
   { S( 101,  46), S( 252,-111), S( 107, 127), S( 104, 296), S(  26,-174)},
   { S( 103,  -2), S(-163,  -3), S( -93, 175), S( 273, 173), S(-222, 318)},
   { S( -29,-217), S( -65, 151), S( -55,-202), S( 214, 172), S(  18,  42)},
   { S( -84,  61), S(   3,  20), S( 213,   2), S( -57,   7), S( 238,-217)}
  },
  { // Advisor
   { S(-167,  30), S( -60, 219), S(   8,-218), S(-100,-144), S(-191, 273)},
   { S( 126,  27), S( -64, 113), S(  50, -88), S(  86, 100), S( -36, 133)},
   { S( -79,   3), S( -18, 126), S( 167, 112), S( 147, 194), S( 168,  75)},
   { S(  -9, 226), S( 211,-133), S(  47,  21), S( 181, 142), S( 155,  61)},
   { S(  42,-222), S( -11,-124), S( 151,  83), S(-140, -10), S( 151, 120)},
   { S( -55, 175), S( 225,-149), S( -78, 174), S( 166, 125), S( 183, 130)},
   { S( -36,-158), S( 187, -95), S( 252, 150), S( 236, -19), S( -25,   7)},
   { S(  20,  94), S(-154,-116), S( -77,-166), S( 230, 398), S( -63,  40)},
   { S( 197,-132), S( 155, -99), S( 112, 130), S( 103, -96), S( -14, 123)},
   { S( -49, 142), S(   6,  54), S(-150, -16), S(  45,  66), S( -82, -85)}
  },
  { // Cannon
   { S(  29,-143), S( 105,  59), S( 118,-192), S( 108, -20), S(  21, -57)},
   { S( 159,-179), S(  -5,  77), S(  20, -65), S( 114, -41), S(  49,  -1)},
   { S( 102,  34), S( 145,-419), S(  -6, 151), S( 140, 256), S( 162, -25)},
   { S( -23,-192), S( 165,   9), S( -72, -60), S(-164, 123), S( 139, 110)},
   { S(-247, -72), S(  43,  32), S(-309,  45), S(  73, -20), S(  -8,   9)},
   { S(  54, 174), S( 125,  94), S( -80, -53), S(  43,  82), S( 104, -18)},
   { S(-104,  89), S( -16, -14), S( -96, -15), S(-109,-346), S( 153, 118)},
   { S(  25,-272), S( -29,-208), S(-102,  40), S( -33, -48), S( -48, -77)},
   { S(-112,  49), S( 241, -38), S(-313, 137), S(-223,  60), S(  10, -24)},
   { S( 237,  83), S(  29,  17), S(-115,-156), S(-187, -17), S(  21, -53)}
  },
  { // Pawn
   { S( -95, -27), S( -26,-155), S(-145,  17), S( 143,  10), S(  31, 118)},
   { S( 203,-201), S( -20,-277), S( -86, -22), S( -81,  97), S(-149,-244)},
   { S( -17,  12), S( -77,-388), S(-141, 286), S(-123, -61), S( -91,  82)},
   { S(  17, -37), S( -65, -96), S( -34, 171), S( -66, 164), S( 142,  -7)},
   { S(  29, -10), S(  74,-171), S(  53, -14), S(  -6, -98), S( 125, -17)},
   { S(   0,  52), S( 212, -84), S(  77, -51), S( 117, 153), S(  94, 162)},
   { S( -51,-137), S(  12, -90), S(  85,  52), S(  98,  43), S(  42,  26)},
   { S(-103, -69), S(  83,  48), S(   0,  43), S( 212, 256), S( 125,  56)},
   { S( -14,  -7), S(  64, -42), S(  52, 132), S( -26, 123), S( -49, 265)},
   { S( -54, -20), S(  56,  34), S( 160,  70), S( -46,  79), S( 119, 149)}
  },
  { // Knight
   { S(-412, 127), S(-117,-157), S(-161,-197), S( -65,-261), S( 196, -32)},
   { S(-312, 179), S(  36, -24), S(-199,-244), S( -92,  94), S(-201,-246)},
   { S(-114, -66), S( -43, -36), S( -60, 128), S(-122,  28), S(  51,-123)},
   { S(-189,-113), S( -44, -45), S(  76,-223), S(  81,  90), S( 182,-132)},
   { S(-298, 115), S( -43,  92), S( -78,   8), S(  15,-275), S( -37,  62)},
   { S( -29,-161), S(-111, 251), S(  24, 263), S(  55, 284), S(  95, -95)},
   { S( 364,-114), S(  71,-110), S(  84, 198), S( 349, -21), S( 195, -71)},
   { S( 109,-126), S( -47,-181), S( 106, 150), S( -75,  78), S(  30, 155)},
   { S(-484,-125), S(-142, -11), S( 113,  77), S(  74,-100), S(-276,  55)},
   { S(  40,  39), S(  39,-206), S( -34,  88), S(-172,   1), S( 146,  48)}
  },
  { // Bishop
   { S(-148, -80), S(-246, 177), S( -18, 122), S(  29,  41), S( 117, 156)},
   { S(  47,  23), S( 177, -48), S(  46,-173), S( 120,-145), S(-151,  51)},
   { S( 160,-123), S( -25,  94), S( 113, 165), S(  46, -23), S(-149, 173)},
   { S(  31, 143), S(  69,  62), S( 187, 166), S( 149,  73), S( 155, 171)},
   { S(  91,-110), S( -84, -37), S(-104,  65), S( -52,  81), S(  86,  26)},
   { S(-229,   7), S( -61,  13), S( 110,-105), S(  39,  13), S(  24,  46)},
   { S(  55, -62), S( -48, -16), S( 315, 153), S(  33, -62), S( 134,  76)},
   { S( -36, 118), S(  85,  80), S( -26,  22), S(-153,  64), S(-113,  70)},
   { S( 156, -34), S(-102,  26), S(-122, 128), S(-152, -27), S(  -3, -33)},
   { S(  23,  39), S(-137, -19), S(  67, -34), S(  90, 228), S( 256,   0)}
  },
  { // King
   { S(   0,   0), S(   0,   0), S(   0,   0), S( 378,-228), S( 390,  60)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   3, 117), S( -10, 250)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(-101,-356), S(  64,  56)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)}
  }
};

} // namespace


namespace PSQT
{
Score psqCap[RANK_NB][int(FILE_NB) / 2 + 1] = {
   { S(  49,   1), S( -76, -89), S(  -1,  -6), S(-182, -15), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(  40, 105), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S( -98,-107), S(   0,   0), S(-108, -88), S(   0,   0), S(  -7, -74)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(  86,-103), S(   0,   0), S( 141,  57), S(   0,   0), S(  99,-153)},
   { S(   0,   0), S( -62,  59), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0), S(   0,   0)},
   { S( -58,  92), S(  75, 146), S(   8,-103), S( 235, -19), S(   0,   0)}
};
Score psq[PIECE_NB][SQUARE_NB];

// PSQT::init() initializes piece-square tables: the white halves of the tables are
// copied from Bonus[] and PBonus[], adding the piece value, then the black halves of
// the tables are initialized by flipping and changing the sign of the white scores.
void init() {

  for (Piece pc : {W_ROOK, W_ADVISOR, W_CANNON, W_PAWN, W_KNIGHT, W_BISHOP, W_KING})
  {
    Score score = make_score(PieceValue[MG][pc], PieceValue[EG][pc]);

    for (Square s = SQ_A0; s <= SQ_I9; ++s)
    {
      File f = File(edge_distance(file_of(s)));
      if (f > FILE_E) --f;
      psq[pc  ][s] = score + Bonus[pc][rank_of(s)][f];
      psq[pc+8][flip_rank(s)] = -psq[pc][s];
    }
  }
}

} // namespace PSQT

} // namespace Stockfish
