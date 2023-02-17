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
auto constexpr NEVER_IN_USE = S(0, 0);

// 'Bonus' contains Piece-Square parameters.
// Scores are explicit for files A to E, implicitly mirrored for F to I.
constexpr Score Bonus[][RANK_NB][int(FILE_NB) / 2 + 1] = {
  { },// NoPiece
  { // Rook
   { S(-173, -249), S(  62,  -89), S( -49, -211), S( -99,  -82), S(  57,   77) },
   { S( 106, -252), S(  28,  110), S(-160,   57), S(  58,   21), S( 105,  -88) },
   { S(  47, -144), S(-121,   46), S(   4,  -67), S(-123,   99), S( -44,  109) },
   { S(  18,  294), S(  59,  -83), S( 128, -166), S(-141,  254), S(-129,  118) },
   { S( 131,   87), S(  54, -125), S( 190,  -91), S(  25,  -78), S(-106,   35) },
   { S(-114,  218), S( 146, -238), S( 189, -132), S(  38,   62), S(  22,  104) },
   { S( -71, -328), S( -21,  209), S(  99,   26), S( 148,  -38), S(  25, -384) },
   { S( -86, -211), S(-140, -266), S(  43, -123), S(  57,   92), S(-105,   80) },
   { S( 242, -240), S(  23,  -28), S(   9,  136), S( 212,  164), S(  33, -164) },
   { S( 143,  142), S( -83,  149), S(-117,  100), S(-133,   65), S( 320,  -38) }
  },
  { // Advisor
   { NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , S( -12,   -4), NEVER_IN_USE  },
   { NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , S( -12,  -64) },
   { NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , S( -26,  -78), NEVER_IN_USE  }
  },
  { // Cannon
   { S( -96,  -48), S( -50,  -91), S( -26,   56), S(  41, -129), S(   7,   62) },
   { S(   0,  -50), S( 144,  -40), S(  -3,  -19), S( 109,  -11), S( -45,    0) },
   { S(-150,  -22), S(-132,   89), S( -24,  107), S(  76,   96), S( -76,   82) },
   { S(  85,  -30), S( -15,  -15), S(  60,   56), S( -33,   39), S( 142,  -35) },
   { S( 140,  -83), S(  44,   53), S(  60,  -82), S(  90,   10), S(  13,   60) },
   { S(  55,  -51), S(  56,  -26), S( -45,   24), S( -42,  -17), S( -20,   -2) },
   { S(   5,    5), S(  27,  112), S( -14,  104), S(  59,   74), S( -51,  193) },
   { S(  78,  124), S(-142, -125), S(  20,  156), S(-102,  -82), S( 230,   79) },
   { S(  95,  -68), S( -10,   85), S(  98,  -28), S( -38,   67), S(  22,  -16) },
   { S( -49,  -53), S(  29,    4), S(-115,   33), S( -89,   31), S( -16,   -3) }
  },
  { // Pawn
   { NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE  },
   { NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE  },
   { NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE  },
   { S( -78,   37), NEVER_IN_USE , S( -75,   43), NEVER_IN_USE , S(  -5,   29) },
   { S(  -3,   40), NEVER_IN_USE , S( -56,   48), NEVER_IN_USE , S(  96,  122) },
   { S(  86,  -64), S(  87,   24), S(  -8,   36), S( 115,  -52), S(  53,   41) },
   { S(  10,   58), S( -53,  -33), S( -58,  -12), S(  65,  -39), S( -54,   29) },
   { S(  99,   64), S(-101,  -42), S( -56,  -24), S(  83,   38), S(  77,  -42) },
   { S(   1,  -50), S(  29,   -6), S(  -4,   33), S(   5,   22), S( -25,  -98) },
   { S( 105,    5), S(  37,  -11), S(  60,   58), S(  68,    8), S(  43,  -69) }
  },
  { // Knight
   { S( -50,  224), S(-131,    4), S(  33,  101), S(-141, -164), S(-144,    0) },
   { S(  30,   81), S( -16,   13), S(  86,  100), S( -58,  -80), S( 159,  124) },
   { S(  81,   39), S( -46,  -22), S(   9,  -84), S(  60,   22), S(   8,   25) },
   { S(  -8,  -76), S( -97, -117), S(  27,   99), S(-123,  -72), S(   0,  -86) },
   { S( -88, -119), S(  86,  -26), S( 118,   84), S(-157,  -33), S(-131, -114) },
   { S(  73,  181), S( -75, -143), S(-105, -172), S(  51,   57), S(-101,  -29) },
   { S(-119,   71), S(  56,   63), S(-134,   -5), S( 189,   24), S(  35,   35) },
   { S( -56,   -4), S( -37,  -13), S(  49,    4), S(-168,  -80), S( 103,   -1) },
   { S( -45,   67), S(  28,   -2), S(-107,  -26), S(  51,  112), S(-215,   97) },
   { S(  26,   67), S( 126,   89), S(-211,   12), S( -29,  -60), S(-121,  170) }
  },
  { // Bishop
   { NEVER_IN_USE , NEVER_IN_USE , S(  18,   13), NEVER_IN_USE , NEVER_IN_USE  },
   { NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE  },
   { S(-106,   -5), NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , S( -12,   15) },
   { NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE  },
   { NEVER_IN_USE , NEVER_IN_USE , S( -23,    9), NEVER_IN_USE , NEVER_IN_USE  }
  },
  { // King
   { NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , S(  44,  -73), S(   9,   29) },
   { NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , S( -28,   79), S(  50,    5) },
   { NEVER_IN_USE , NEVER_IN_USE , NEVER_IN_USE , S( -75,   38), S(  26,   68) }
  }
};

} // namespace


namespace PSQT
{

Score psq[PIECE_NB][SQUARE_NB];

// PSQT::init() initializes piece-square tables: the white halves of the tables are
// copied from Bonus[], adding the piece value, then the black halves of the tables
// are initialized by flipping and changing the sign of the white scores.
void init() {

  for (Piece pc : {W_ROOK, W_ADVISOR, W_CANNON, W_PAWN, W_KNIGHT, W_BISHOP, W_KING})
  {
    Score score = make_score(PieceValue[MG][pc], PieceValue[EG][pc]);

    for (Square s = SQ_A0; s <= SQ_I9; ++s)
    {
      File f = File(edge_distance(file_of(s)));
      psq[ pc][s] = score + Bonus[pc][rank_of(s)][f];
      // mg part is used to calculate material sum, eg part is used to calculate material diff
      Value material = mg_value(psq[pc][s]);
      Value     psqt = eg_value(psq[pc][s]);
      psq[~pc][flip_rank(s)] = make_score(material, -psqt);
    }
  }
}

} // namespace PSQT

} // namespace Stockfish
