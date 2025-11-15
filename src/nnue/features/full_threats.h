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

//Definition of input features Simplified_Threats of NNUE evaluation function

#ifndef NNUE_FEATURES_FULL_THREATS_INCLUDED
#define NNUE_FEATURES_FULL_THREATS_INCLUDED

#include <cstdint>

#include "../../misc.h"
#include "../../types.h"
#include "../nnue_common.h"

namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE::Features {

void init_threat_offsets();

class FullThreats {
   public:
    // Feature name
    static constexpr const char* Name = "Full_Threats(Friend)";

    // Hash value embedded in the evaluation file
    static constexpr std::uint32_t HashValue = 0xd17b100;

    // Number of feature dimensions
    static constexpr IndexType Dimensions = 45976;

    // Maximum number of simultaneously active features.
    static constexpr IndexType MaxActiveDimensions = 64;
    using IndexList                                = ValueList<IndexType, MaxActiveDimensions>;
    using DiffType                                 = DirtyThreats;

    template<Color Perspective>
    static IndexType make_index(Piece attkr, Square from, Square to, Piece attkd, bool mirror);

    // Get a list of indices for active features
    template<Color Perspective>
    static void append_active_indices(const Position& pos, IndexList& active);

    // Get a list of indices for recently changed features
    template<Color Perspective>
    static void
    append_changed_indices(bool mirror, const DiffType& diff, IndexList& removed, IndexList& added);

    // Returns whether the change stored in this DirtyPiece means
    // that a full accumulator refresh is required.
    static bool requires_refresh(const DiffType& diff, Color perspective);
};

}  // namespace Stockfish::Eval::NNUE::Features

#endif  // #ifndef NNUE_FEATURES_FULL_THREATS_INCLUDED
