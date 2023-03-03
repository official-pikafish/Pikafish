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


#include "material.h"

namespace Stockfish {

namespace Material
{

Score value[PIECE_NB];

// Material::init() initializes material tables, mg part is used to
// calculate material sum, eg part is used to calculate material diff.
void init() {

  for (Piece pc : { W_ROOK, W_ADVISOR, W_CANNON, W_PAWN, W_KNIGHT, W_BISHOP, W_KING })
  {
    value[ pc] = make_score(PieceValue[MG][pc], PieceValue[EG][pc]);
    value[~pc] = make_score(PieceValue[MG][pc], -PieceValue[EG][pc]);
  }
}

} // namespace Material

} // namespace Stockfish
