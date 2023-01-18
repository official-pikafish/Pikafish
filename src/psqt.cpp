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
// Scores are explicit for files A to E, implicitly mirrored for F to I.
constexpr Score Bonus[][RANK_NB][int(FILE_NB) / 2 + 1] = {
  { },// NoPiece
  { // Rook
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) }
  },
  { // Advisor
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) }
  },
  { // Cannon
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) }
  },
  { // Pawn
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) }
  },
  { // Knight
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) }
  },
  { // Bishop
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) }
  },
  { // King
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) },
   { S(0, 0), S(0, 0), S(0, 0), S(0, 0), S(0, 0) }
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
