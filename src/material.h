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

#ifndef MATERIAL_H_INCLUDED
#define MATERIAL_H_INCLUDED
  
#include "misc.h"
#include "position.h"
#include "types.h"

namespace Stockfish::Material {

    /// Material::Entry contains various information about a material configuration.
    /// It contains a material imbalance evaluation, a function pointer to a special
    /// endgame evaluation function (which in most cases is nullptr, meaning that the
    /// standard evaluation function will be used), and scale factors.
    ///
    /// The scale factors are used to scale the evaluation score up or down. For
    /// instance, in KRB vs KR endgames, the score is scaled down by a factor of 4,
    /// which will result in scores of absolute value less than one pawn.

    struct Entry {

        Score imbalance() const { return score; }

        Key key;
        Score score;
    };

    typedef HashTable<Entry, 8192> Table;

    Entry* probe(const Position& pos);

} // namespace Stockfish::Material

#endif // #ifndef MATERIAL_H_INCLUDED