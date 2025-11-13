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

// Definition of input features HalfKAv2_hm of NNUE evaluation function

#include "half_ka_v2_hm.h"

#include "../../position.h"
#include "../../types.h"
#include "../nnue_common.h"

namespace Stockfish::Eval::NNUE::Features {

// Lookup array for indexing
uint16_t PSQOffsets[PIECE_NB][SQUARE_NB];

void init_psq_offsets() {
    int cumulativeOffset = 0;
    for (Piece pc : HalfKAv2_hm::AllPieces)
        for (Square sq = SQ_A0; sq <= SQ_I9; ++sq)
            if (HalfKAv2_hm::ValidBB[pc] & sq)
                PSQOffsets[pc][sq] = cumulativeOffset++;
}

bool HalfKAv2_hm::requires_mid_mirror(const Position& pos, Color c) {
    return ((1ULL << 63) & pos.mid_encoding(c) & pos.mid_encoding(~c))
        && (pos.mid_encoding(c) < BalanceEncoding
            || (pos.mid_encoding(c) == BalanceEncoding && pos.mid_encoding(~c) < BalanceEncoding));
}

// Get attack bucket
IndexType HalfKAv2_hm::make_attack_bucket(const Position& pos, Color c) {
    return AttackBucket[pos.count<ROOK>(c)][pos.count<KNIGHT>(c)][pos.count<CANNON>(c)];
}

// Get layer stack bucket
IndexType HalfKAv2_hm::make_layer_stack_bucket(const Position& pos) {
    Color us = pos.side_to_move();
    return LayerStackBuckets[pos.count<ROOK>(us)][pos.count<ROOK>(~us)]
                            [pos.count<KNIGHT>(us) + pos.count<CANNON>(us)]
                            [pos.count<KNIGHT>(~us) + pos.count<CANNON>(~us)];
}

// Index of a feature for a given king position and another piece on some square
IndexType HalfKAv2_hm::make_index(Color perspective, Square s, Piece pc, int bucket, bool mirror) {
    s = (Square) HalfKAv2_hm::IndexMap[mirror][perspective == BLACK][s];

    if (perspective == BLACK)
        pc = ~pc;

    return PSQOffsets[pc][s] + PS_NB * bucket;
}

// Get a list of indices for recently changed features
void HalfKAv2_hm::append_changed_indices(Color           perspective,
                                         int             bucket,
                                         bool            mirror,
                                         const DiffType& diff,
                                         IndexList&      removed,
                                         IndexList&      added) {
    removed.push_back(make_index(perspective, diff.from, diff.pc, bucket, mirror));

    if (diff.to != SQ_NONE)
        added.push_back(make_index(perspective, diff.to, diff.pc, bucket, mirror));

    if (diff.remove_sq != SQ_NONE)
        removed.push_back(make_index(perspective, diff.remove_sq, diff.remove_pc, bucket, mirror));
}

bool HalfKAv2_hm::requires_refresh(const DiffType& diff, Color perspective) {
    return diff.requires_refresh[perspective];
}

}  // namespace Stockfish::Eval::NNUE::Features
