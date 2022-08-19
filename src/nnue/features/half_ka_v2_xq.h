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

//Definition of input features HalfKAv2_xq of NNUE evaluation function

#ifndef NNUE_FEATURES_HALF_KA_V2_XQ_H_INCLUDED
#define NNUE_FEATURES_HALF_KA_V2_XQ_H_INCLUDED

#include "../nnue_common.h"

#include "../../evaluate.h"
#include "../../misc.h"

namespace Stockfish {
  struct StateInfo;
}

namespace Stockfish::Eval::NNUE::Features {

  // Feature HalfKAv2_xq: Combination of the position of own king and the position of pieces.
  class HalfKAv2_xq {

    // unique number for each piece type on each square
    enum {
      PS_NONE      =  0,
      PS_W_ROOK    =  0,
      PS_B_ROOK    =  1  * SQUARE_NB,
      PS_W_ADVISOR =  2  * SQUARE_NB,
      PS_B_ADVISOR =  3  * SQUARE_NB,
      PS_W_CANNON  =  4  * SQUARE_NB,
      PS_B_CANNON  =  5  * SQUARE_NB,
      PS_W_PAWN    =  6  * SQUARE_NB,
      PS_B_PAWN    =  7  * SQUARE_NB,
      PS_W_KNIGHT  =  8  * SQUARE_NB,
      PS_B_KNIGHT  =  9  * SQUARE_NB,
      PS_W_BISHOP  =  10 * SQUARE_NB,
      PS_B_BISHOP  =  11 * SQUARE_NB,
      PS_KING      =  12 * SQUARE_NB,
      PS_NB        =  13 * SQUARE_NB
    };

    static constexpr IndexType PieceSquareIndex[COLOR_NB][PIECE_NB] = {
      // convention: W - us, B - them
      // viewed from other side, W and B are reversed
      { PS_NONE, PS_W_ROOK, PS_W_ADVISOR, PS_W_CANNON, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_KING,
        PS_NONE, PS_B_ROOK, PS_B_ADVISOR, PS_B_CANNON, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_KING },
      { PS_NONE, PS_B_ROOK, PS_B_ADVISOR, PS_B_CANNON, PS_B_PAWN, PS_B_KNIGHT, PS_B_BISHOP, PS_KING,
        PS_NONE, PS_W_ROOK, PS_W_ADVISOR, PS_W_CANNON, PS_W_PAWN, PS_W_KNIGHT, PS_W_BISHOP, PS_KING, }
    };

    // Index of a feature for a given king position and another piece on some square
    static IndexType make_index(Color perspective, Square s, Piece pc, Square ksq);

   public:
    // Feature name
    static constexpr const char* Name = "HalfKAv2_xq";

    // Hash value embedded in the evaluation file
    static constexpr std::uint32_t HashValue = 0x7f234cb8u;

    // Number of feature dimensions
    static constexpr IndexType Dimensions = 9 * static_cast<IndexType>(PS_NB);

    static constexpr int KingBuckets[SQUARE_NB] = {
         0,  0,  0,  0,  1,  2,  0,  0,  0,
         0,  0,  0,  3,  4,  5,  0,  0,  0,
         0,  0,  0,  6,  7,  8,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  6,  7,  8,  0,  0,  0,
         0,  0,  0,  3,  4,  5,  0,  0,  0,
         0,  0,  0,  0,  1,  2,  0,  0,  0,
    };

    // Orient a square according to perspective (rotates by 180 for black)
    static constexpr int Orient[COLOR_NB][SQUARE_NB] = {
      {   0,  1,  2,  3,  4,  5,  6,  7,  8,
          9, 10, 11, 12, 13, 14, 15, 16, 17,
         18, 19, 20, 21, 22, 23, 24, 25, 26,
         27, 28, 29, 30, 31, 32, 33, 34, 35,
         36, 37, 38, 39, 40, 41, 42, 43, 44,
         45, 46, 47, 48, 49, 50, 51, 52, 53,
         54, 55, 56, 57, 58, 59, 60, 61, 62,
         63, 64, 65, 66, 67, 68, 69, 70, 71,
         72, 73, 74, 75, 76, 77, 78, 79, 80,
         81, 82, 83, 84, 85, 86, 87, 88, 89,  },

      {  81, 82, 83, 84, 85, 86, 87, 88, 89,
         72, 73, 74, 75, 76, 77, 78, 79, 80,
         63, 64, 65, 66, 67, 68, 69, 70, 71,
         54, 55, 56, 57, 58, 59, 60, 61, 62,
         45, 46, 47, 48, 49, 50, 51, 52, 53,
         36, 37, 38, 39, 40, 41, 42, 43, 44,
         27, 28, 29, 30, 31, 32, 33, 34, 35,
         18, 19, 20, 21, 22, 23, 24, 25, 26,
          9, 10, 11, 12, 13, 14, 15, 16, 17,
          0,  1,  2,  3,  4,  5,  6,  7,  8,  }
    };

    // Maximum number of simultaneously active features.
    static constexpr IndexType MaxActiveDimensions = 32;
    using IndexList = ValueList<IndexType, MaxActiveDimensions>;

    // Get a list of indices for active features
    static void append_active_indices(
      const Position& pos,
      Color perspective,
      IndexList& active);

    // Get a list of indices for recently changed features
    static void append_changed_indices(
      Square ksq,
      const DirtyPiece& dp,
      Color perspective,
      IndexList& removed,
      IndexList& added
    );

    // Returns the cost of updating one perspective, the most costly one.
    // Assumes no refresh needed.
    static int update_cost(const StateInfo* st);
    static int refresh_cost(const Position& pos);

    // Returns whether the change stored in this StateInfo means that
    // a full accumulator refresh is required.
    static bool requires_refresh(const StateInfo* st, Color perspective);
  };

}  // namespace Stockfish::Eval::NNUE::Features

#endif // #ifndef NNUE_FEATURES_HALF_KA_V2_XQ_H_INCLUDED
