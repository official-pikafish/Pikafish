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

//Definition of input features HalfKAv2_hm of NNUE evaluation function

#ifndef NNUE_FEATURES_HALF_KA_V2_HM_H_INCLUDED
#define NNUE_FEATURES_HALF_KA_V2_HM_H_INCLUDED

#include <array>
#include <cstdint>
#include <utility>

#include "../../misc.h"
#include "../../types.h"
#include "../nnue_common.h"

namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE::Features {

// Feature HalfKAv2_hm: Combination of the position of own king and the
// position of pieces. Position mirrored such that king is always on d..e files.
class HalfKAv2_hm {

    // Unique number for each piece type on each square
    enum {
        // clang-format off
        PS_NONE      = 0,
        PS_W_ROOK    = 0,
        PS_B_ROOK    = 1 * SQUARE_NB,
        PS_W_CANNON  = 2 * SQUARE_NB,
        PS_B_CANNON  = 3 * SQUARE_NB,
        PS_W_KNIGHT  = 4 * SQUARE_NB,
        PS_B_KNIGHT  = 5 * SQUARE_NB,
        PS_W_PAWN    = 6 * SQUARE_NB,
        PS_B_PAWN    = 7 * SQUARE_NB,
        PS_W_ADVISOR = 8 * SQUARE_NB,
        PS_B_ADVISOR = 9 * SQUARE_NB,
        PS_W_BISHOP  = 10 * SQUARE_NB,
        PS_B_BISHOP  = 11 * SQUARE_NB,
        PS_WB_KING   = 12 * SQUARE_NB,
        PS_DARK      = 13 * SQUARE_NB,
        PS_NB        = 14 * SQUARE_NB
        // clang-format on
    };

    static constexpr IndexType PieceSquareIndex[COLOR_NB][PIECE_NB + 1] = {
      // Convention: W - us, B - them
      // Viewed from other side, W and B are reversed
      // clang-format off
      { PS_NONE, PS_W_ROOK, PS_W_ADVISOR, PS_W_CANNON, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_WB_KING,
        PS_NONE, PS_B_ROOK, PS_B_ADVISOR, PS_B_CANNON, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_WB_KING, PS_DARK },
      { PS_NONE, PS_B_ROOK, PS_B_ADVISOR, PS_B_CANNON, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_WB_KING,
        PS_NONE, PS_W_ROOK, PS_W_ADVISOR, PS_W_CANNON, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_WB_KING, PS_DARK }
      // clang-format on
    };

    static constexpr IndexType DarkRestIndex[COLOR_NB][PIECE_NB] = {
      // Convention: W - us, B - them
      // Viewed from other side, W and B are reversed
      // clang-format off
      { PS_NONE, PS_DARK +  9, PS_DARK + 10, PS_DARK + 11, PS_DARK + 12, PS_DARK + 13, PS_DARK + 14, PS_NONE,
        PS_NONE, PS_DARK + 72, PS_DARK + 73, PS_DARK + 74, PS_DARK + 75, PS_DARK + 76, PS_DARK + 77, PS_NONE, },
      { PS_NONE, PS_DARK + 72, PS_DARK + 73, PS_DARK + 74, PS_DARK + 75, PS_DARK + 76, PS_DARK + 77, PS_NONE,
        PS_NONE, PS_DARK +  9, PS_DARK + 10, PS_DARK + 11, PS_DARK + 12, PS_DARK + 13, PS_DARK + 14, PS_NONE, }
      // clang-format on
    };

   public:
    // Feature name
    static constexpr const char* Name = "HalfKAv2_hm";

    // Hash value embedded in the evaluation file
    static constexpr std::uint32_t HashValue = 0xd17b100;

    // Number of feature dimensions
    static constexpr IndexType Dimensions = 6 * static_cast<IndexType>(PS_NB);

    // Get king_index and mirror information
    static constexpr auto KingBuckets = []() {
#define M(s) ((1 << 3) | s)
        // Stored as (mirror << 3 | bucket)
        constexpr uint8_t KingBuckets[SQUARE_NB] = {
          // clang-format off
          0,  0,  0,  0,  1, M(0),  0,  0,  0,
          0,  0,  0,  2,  3, M(2),  0,  0,  0,
          0,  0,  0,  4,  5, M(4),  0,  0,  0,
          0,  0,  0,  0,  0,   0 ,  0,  0,  0,
          0,  0,  0,  0,  0,   0 ,  0,  0,  0,
          0,  0,  0,  0,  0,   0 ,  0,  0,  0,
          0,  0,  0,  0,  0,   0 ,  0,  0,  0,
          0,  0,  0,  4,  5, M(4),  0,  0,  0,
          0,  0,  0,  2,  3, M(2),  0,  0,  0,
          0,  0,  0,  0,  1, M(0),  0,  0,  0,
          // clang-format on
        };
#undef M
        std::array<std::array<std::pair<int, bool>, SQUARE_NB>, SQUARE_NB> v{};
        for (uint8_t ksq = SQ_A0; ksq <= SQ_I9; ++ksq)
            for (uint8_t oksq = SQ_A0; oksq <= SQ_I9; ++oksq)
            {
                uint8_t king_bucket_ = KingBuckets[ksq];
                int     king_bucket  = king_bucket_ & 0x7;
                bool    mirror =
                  (king_bucket_ >> 3) || ((king_bucket & 1) && (KingBuckets[oksq] >> 3));
                v[ksq][oksq].first  = king_bucket;
                v[ksq][oksq].second = mirror;
            }
        return v;
    }();

    // Square index mapping based on condition (Mirror, Rotate)
    static constexpr auto IndexMap = []() {
        std::array<std::array<std::array<std::uint8_t, SQUARE_NB>, 2>, 2> v{};
        for (uint8_t m = 0; m < 2; ++m)
            for (uint8_t r = 0; r < 2; ++r)
                for (uint8_t s = 0; s < SQUARE_NB; ++s)
                {
                    uint8_t ss = s;
                    ss         = m ? uint8_t(flip_file(Square(ss))) : ss;
                    ss         = r ? uint8_t(flip_rank(Square(ss))) : ss;
                    v[m][r][s] = ss;
                }
        return v;
    }();

    static constexpr int LayerStackBuckets[33] = {
      -1, -1, 0, 0, 0, 0, 0, 1, 1,  2,  2,  3,  3,  4,  4,  5,  5,
      6,  6,  7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 13, 14, 15,
    };

    // Maximum number of simultaneously active features.
    static constexpr IndexType MaxActiveDimensions = 62;
    using IndexList                                = ValueList<IndexType, MaxActiveDimensions>;

    // Index of a feature for a given king position and another piece on some square
    template<Color Perspective>
    static IndexType make_index(Square s, Piece pc, int bucket, bool mirror);

    template<Color Perspective>
    static IndexType make_dark_rest_index(Piece pc, int bucket);

    // Get layer stack bucket
    static IndexType make_layer_stack_bucket(const Position& pos);

    // Get a list of indices for recently changed features
    template<Color Perspective>
    static void append_changed_indices(
      int bucket, bool mirror, const DirtyPiece& dp, IndexList& removed, IndexList& added);

    // Returns whether the change stored in this DirtyPiece means
    // that a full accumulator refresh is required.
    static bool requires_refresh(const DirtyPiece& dirtyPiece, Color perspective);
};

}  // namespace Stockfish::Eval::NNUE::Features

#endif  // #ifndef NNUE_FEATURES_HALF_KA_V2_HM_H_INCLUDED
