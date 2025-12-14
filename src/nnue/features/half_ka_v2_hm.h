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
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <utility>

#include "../../bitboard.h"
#include "../../misc.h"
#include "../../types.h"
#include "../nnue_common.h"

namespace Stockfish {
class Position;
}

namespace Stockfish::Eval::NNUE::Features {

void init_psq_offsets();

// Feature HalfKAv2_hm: Combination of the position of own king and the
// position of pieces. Position mirrored such that king is always on d..e files.
class HalfKAv2_hm {
   public:
    // Feature name
    static constexpr const char* Name = "HalfKAv2_hm";

    // Hash value embedded in the evaluation file
    static constexpr std::uint32_t HashValue = 0xd17b100;

    // Number of features per plane
    static constexpr IndexType PS_NB = 689;

    // Number of attack buckets
    static constexpr IndexType AttackBucketNB = 4;

    // Number of feature dimensions
    static constexpr IndexType Dimensions = 6 * AttackBucketNB * PS_NB;

    static constexpr Bitboard ValidBB[PIECE_NB]{
      0,                                                                                        // _
      HalfBB[WHITE] | HalfBB[BLACK],                                                            // R
      ((Rank0BB | Rank2BB) & (FileDBB | FileFBB)) | (Rank1BB & FileEBB),                        // A
      HalfBB[WHITE] | HalfBB[BLACK],                                                            // C
      PawnBB[WHITE],                                                                            // P
      HalfBB[WHITE] | HalfBB[BLACK],                                                            // N
      ((Rank0BB | Rank4BB) & (FileCBB | FileGBB)) | (Rank2BB & (FileABB | FileEBB | FileIBB)),  // B
      HalfBB[WHITE] & Palace & ~FileFBB,                                                        // K
      0,                                                                                        // _
      HalfBB[WHITE] | HalfBB[BLACK],                                                            // r
      ((Rank7BB | Rank9BB) & (FileDBB | FileFBB)) | (Rank8BB & FileEBB),                        // a
      HalfBB[WHITE] | HalfBB[BLACK],                                                            // c
      PawnBB[BLACK],                                                                            // p
      HalfBB[WHITE] | HalfBB[BLACK],                                                            // n
      ((Rank5BB | Rank9BB) & (FileCBB | FileGBB)) | (Rank7BB & (FileABB | FileEBB | FileIBB)),  // b
      HalfBB[BLACK] & Palace,                                                                   // k
    };

    static constexpr std::array<Piece, 14> AllPieces{
      W_ROOK, W_ADVISOR, W_CANNON, W_PAWN, W_KNIGHT, W_BISHOP, W_KING,
      B_ROOK, B_ADVISOR, B_CANNON, B_PAWN, B_KNIGHT, B_BISHOP, B_KING};

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
        std::array<std::array<std::array<std::pair<int, bool>, 2>, SQUARE_NB>, SQUARE_NB> v{};
        for (uint8_t ksq = SQ_A0; ksq <= SQ_I9; ++ksq)
            for (uint8_t oksq = SQ_A0; oksq <= SQ_I9; ++oksq)
                for (uint8_t midm = 0; midm <= 1; ++midm)
                {
                    uint8_t king_bucket_ = KingBuckets[ksq];
                    int     king_bucket  = king_bucket_ & 0x7;
                    int     oking_bucket = KingBuckets[oksq] & 0x7;
                    bool    mirror =
                      (king_bucket_ >> 3)
                      || ((king_bucket & 1)
                          && ((KingBuckets[oksq] >> 3) || (bool(oking_bucket & 1) && midm)));
                    v[ksq][oksq][midm].first  = king_bucket;
                    v[ksq][oksq][midm].second = mirror;
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

    /* 64 bit encoding related to mid mirror, which is divided into two parts, piece counts and squares except for king
    *
    *  Encoding representations:
    *    Middle king   : | 1 bit | -> Active when king in FILE_E
    *    Piece types   : |advisor|bishop| pawn |knight|cannon| rook |
    *    Piece counts  : | 3 bits|3 bits|4 bits|3 bits|3 bits|3 bits| -> 19 bits
    *    Guarding bit  : | 1 bit | -> Set to one to prevent overflow from piece squares part into piece counts part
    *    Piece squares : | 7 bits|7 bits|8 bits|7 bits|7 bits|7 bits| -> 43 bits
    *
    *  Piece counts for the left flank (i.e. FILE_A to FILE_D) of the board are 1, for the right flank (i.e. FILE_F to
    *  FILE_I) are -1, mirroring that of the left flank but with a negative sign, and for the center is 0.
    *
    *  Piece squares for the left flank of the board are positive, counting from 0 to 39, for the right flank are
    *  negative, counting from -39 to 0, mirroring that of the left flank but with a negative sign, and for the center
    *  is 0. For black pieces, the encoding is vertically flipped.
    *
    *  Encoding for piece at the left flank of the board is positive, for example, encoding for a single piece 'rook'
    *  at square 'A3' (assuming A3 is 33) is:
    *    Middle king   : | 0|
    *    Piece counts  : | 0| 0| 0| 0| 0| 1|
    *    Guarding bits : | 0|
    *    Piece squares : | 0| 0| 0| 0| 0|33|
    *
    *  Encoding for piece at the right flank of the board is getting negated, which means a negative sign is added to
    *  the final encoding of that piece, for example, the encoding of the same piece on square 'I3' (assuming I3 is -33)
    *  is:
    *    -(Middle king   : | 0|
    *      Piece counts  : | 0| 0| 0| 0| 0| 1|
    *      Guarding bits : | 0|
    *      Piece squares : | 0| 0| 0| 0| 0|33|)
    *
    *  Encoding for piece at FILE_E of the board is all zero, for example, the encoding of some piece on square 'E3'
    *  is:
    *    Middle king   : | 0| (| 1| if the piece is king)
    *    Piece counts  : | 0| 0| 0| 0| 0| 0|
    *    Guarding bits : | 0|
    *    Piece squares : | 0| 0| 0| 0| 0| 0|
    *
    *  The overall encoding of a balanced position is (with the concept of complement numbers being used):
    *    Middle king   : | 1|
    *    Piece counts  : | 2| 2| 5| 2| 2| 2|
    *    Guarding bits : | 1|
    *    Piece squares : |39|39|76|39|39|39|
    *  Where 39 can account for subtraction of 0 - 39 for non-pawn pieces and 76 can account for subtraction of two pawns
    *  (0 + 1) - (38 + 39) at most in a balanced position.
    *
    *  Each piece placed will add its encoding to the overall representation, and removing a piece will do a subtraction.
    *
    *  Now we just need to test if the encoding of the board is smaller than balanced position to see if we need mirroring:
    *    If the piece count is imbalanced for both flanks, the result will be fully decided by the first part of the
    *    encoding, regardless of the second part and overflows.
    *    If the piece count is balanced, the first part of the encoding must be the same, and the result will be decided
    *    by the second part, and all overflows of the second part will be automatically resolved at this time.
    */
    static constexpr auto MidMirrorEncoding = [] {
        std::array<std::array<uint64_t, static_cast<size_t>(SQUARE_NB)>,
                   static_cast<size_t>(PIECE_NB)>
                          encodings{};
        constexpr uint8_t shifts[8][2]{{0, 0},   {44, 0},  {60, 36}, {47, 7},
                                       {53, 21}, {50, 14}, {57, 29}, {0, 0}};
        for (const auto& c : {WHITE, BLACK})
            for (uint8_t pt = ROOK; pt <= KING; ++pt)
                for (uint8_t r = RANK_0; r < RANK_NB; ++r)
                    for (uint8_t f = FILE_A; f < FILE_NB; ++f)
                    {
                        uint64_t encoding = 0;
                        if (f != FILE_E && pt != KING)
                        {
                            uint8_t r_           = c == WHITE ? r : RANK_9 - r;
                            uint8_t f_           = f < FILE_E ? f : FILE_I - f;
                            const auto& [s1, s2] = shifts[pt];
                            encoding             = (1ULL << s1)
                                     | ((uint64_t(File::FILE_D - f_) * 10 + uint64_t(r_)) << s2);
                            encoding = f < FILE_E ? encoding : uint64_t(-int64_t(encoding));
                        }
                        else if (f != FILE_E && pt == KING)
                            encoding = 1ULL << 63;
                        uint8_t p        = static_cast<uint8_t>(make_piece(c, PieceType(pt)));
                        uint8_t sq       = static_cast<uint8_t>(make_square(File(f), Rank(r)));
                        encodings[p][sq] = encoding;
                    }
        return encodings;
    }();

    static constexpr uint64_t BalanceEncoding{0xa4a92a74e989d3a7};

    // Maximum number of simultaneously active features.
    static constexpr IndexType MaxActiveDimensions = 32;
    using IndexList                                = ValueList<IndexType, MaxActiveDimensions>;
    using DiffType                                 = DirtyPiece;

    // Returns whether the middle mirror is required.
    static bool requires_mid_mirror(const Position& pos, Color c);

    // Get attack bucket
    static IndexType make_attack_bucket(const Position& pos, Color c);

    // Get feature bucket
    static std::tuple<int, bool, int> make_feature_bucket(Color perspective, const Position& pos);

    // Get layer stack bucket
    static IndexType make_layer_stack_bucket(const Position& pos);

    // Index of a feature for a given king position and another piece on some square
    static IndexType make_index(Color perspective, Square s, Piece pc, int bucket, bool mirror);

    // Get a list of indices for recently changed features
    static void append_changed_indices(Color           perspective,
                                       int             bucket,
                                       bool            mirror,
                                       const DiffType& diff,
                                       IndexList&      removed,
                                       IndexList&      added);

    // Returns whether the change stored in this DirtyPiece means
    // that a full accumulator refresh is required.
    static bool requires_refresh(const DiffType& diff, Color perspective);
};

}  // namespace Stockfish::Eval::NNUE::Features

#endif  // #ifndef NNUE_FEATURES_HALF_KA_V2_HM_H_INCLUDED
