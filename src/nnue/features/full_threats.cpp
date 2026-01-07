/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2026 The Stockfish developers (see AUTHORS file)

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

// Definition of input features FullThreats of NNUE evaluation function

#include "full_threats.h"

#include <array>

#include "../../position.h"
#include "../../types.h"
#include "../nnue_common.h"

namespace Stockfish::Eval::NNUE::Features {

// Lookup array for indexing threats
auto ThreatOffsets = []() {
    MultiArray<uint16_t, PIECE_NB, SQUARE_NB, SQUARE_NB, PIECE_NB> ThreatOffsets{};
    // clang-format off
    constexpr bool ValidPairs[PIECE_NB][PIECE_NB] = {
      //    R   A   C   P   N   B   K   _   r   a   c   p   n   b   k
      { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, // _
      { 0,  1,  1,  1,  1,  1,  1,  1,  0,  1,  1,  1,  1,  1,  1,  0}, // R
      { 0,  1,  1,  1,  0,  1,  0,  1,  0,  1,  0,  1,  1,  1,  0,  0}, // A
      { 0,  1,  1,  1,  1,  1,  1,  1,  0,  1,  1,  1,  1,  1,  1,  0}, // C
      { 0,  0,  0,  1,  1,  1,  1,  0,  0,  0,  1,  1,  1,  1,  1,  0}, // P
      { 0,  1,  1,  1,  1,  1,  1,  1,  0,  1,  1,  1,  1,  1,  1,  0}, // N
      { 0,  1,  0,  1,  1,  1,  1,  1,  0,  1,  0,  1,  1,  1,  0,  0}, // B
      { 0,  0,  1,  1,  0,  1,  1,  0,  0,  0,  0,  1,  1,  1,  0,  0}, // K
      { 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0}, // _
      { 0,  1,  1,  1,  1,  1,  1,  0,  0,  1,  1,  1,  1,  1,  1,  1}, // r
      { 0,  1,  0,  1,  1,  1,  0,  0,  0,  1,  1,  1,  0,  1,  0,  1}, // a
      { 0,  1,  1,  1,  1,  1,  1,  0,  0,  1,  1,  1,  1,  1,  1,  1}, // c
      { 0,  0,  1,  1,  1,  1,  1,  0,  0,  0,  0,  1,  1,  1,  1,  0}, // p
      { 0,  1,  1,  1,  1,  1,  1,  0,  0,  1,  1,  1,  1,  1,  1,  1}, // n
      { 0,  1,  0,  1,  1,  1,  0,  0,  0,  1,  0,  1,  1,  1,  1,  1}, // b
      { 0,  0,  0,  1,  1,  1,  0,  0,  0,  0,  1,  1,  0,  1,  1,  0}, // k
    };
    // clang-format on

    // Initialize threat offsets to be all Dimension
    for (uint8_t i = 0; i < PIECE_NB; ++i)
        for (uint8_t j = 0; j < SQUARE_NB; ++j)
            for (uint8_t k = 0; k < SQUARE_NB; ++k)
                for (uint8_t l = 0; l < PIECE_NB; ++l)
                    ThreatOffsets[i][j][k][l] = FullThreats::Dimensions;

    int cumulativeOffset = 0;
    for (Piece attacker : HalfKAv2_hm::AllPieces)
    {
        PieceType pt = type_of(attacker);

        for (Square from = SQ_A0; from <= SQ_I9; ++from)
            if (HalfKAv2_hm::ValidBB[attacker] & from)
            {
                Bitboard attacks = Bitboard(0);
                if (pt == PAWN)
                    attacks = attacks_bb<PAWN>(from, color_of(attacker));
                else if (pt == CANNON)
                    attacks =
                      Bitboards::sliding_attack<CANNON>(from, unconstrained_attacks_bb<KING>(from));
                else
                    attacks = PseudoAttacks[pt][from];

                for (Piece attacked : HalfKAv2_hm::AllPieces)
                    if (ValidPairs[attacker][attacked])
                    {
                        Bitboard targets = attacks & HalfKAv2_hm::ValidBB[attacked];
                        while (targets)
                        {
                            Square to = pop_lsb(targets);

                            // Some threats imply the existence of the corresponding ones in the opposite
                            // direction. We filter them here to ensure only one such threat is active.
                            bool enemy     = color_of(attacker) != color_of(attacked);
                            bool same_file = file_of(from) == file_of(to);
                            bool same_rank = rank_of(from) == rank_of(to);
                            bool semi_excluded =
                              pt == type_of(attacked)
                              && (pt != PAWN || (enemy && same_file) || (!enemy && same_rank))
                              && pt != KNIGHT;
                            if (!semi_excluded || from > to)
                                ThreatOffsets[attacker][from][to][attacked] = cumulativeOffset++;
                        }
                    }
            }
    }

    return ThreatOffsets;
}();

// Index of a feature for a given king position and another piece on some square
IndexType FullThreats::make_index(
  Color perspective, Piece attacker, Square from, Square to, Piece attacked, bool mirror) {
    from = (Square) HalfKAv2_hm::IndexMap[mirror][perspective == BLACK][from];
    to   = (Square) HalfKAv2_hm::IndexMap[mirror][perspective == BLACK][to];

    if (perspective == BLACK)
    {
        attacker = ~attacker;
        attacked = ~attacked;
    }

    return ThreatOffsets[attacker][from][to][attacked];
}

// Get a list of indices for active features in ascending order
void FullThreats::append_active_indices(Color perspective, const Position& pos, IndexList& active) {
    Square ksq  = pos.king_square(perspective);
    Square oksq = pos.king_square(~perspective);
    auto& [_, mirror] =
      HalfKAv2_hm::KingBuckets[ksq][oksq][HalfKAv2_hm::requires_mid_mirror(pos, perspective)];
    Bitboard occupied = pos.pieces();

    Bitboard bb = occupied;
    while (bb)
    {
        Square    from     = pop_lsb(bb);
        Piece     attacker = pos.piece_on(from);
        PieceType pt       = type_of(attacker);
        Color     c        = color_of(attacker);
        Bitboard  attacks =
          (pt == PAWN ? attacks_bb<PAWN>(from, c) : attacks_bb(pt, from, occupied)) & occupied;

        while (attacks)
        {
            Square    to       = pop_lsb(attacks);
            Piece     attacked = pos.piece_on(to);
            IndexType index    = make_index(perspective, attacker, from, to, attacked, mirror);

            if (index < Dimensions)
                active.push_back(index);
        }
    }
}

// Get a list of indices for recently changed features
void FullThreats::append_changed_indices(
  Color perspective, bool mirror, const DiffType& diff, IndexList& removed, IndexList& added) {
    for (const auto dirty : diff.list)
    {
        auto attacker = dirty.pc();
        auto attacked = dirty.threatened_pc();
        auto from     = dirty.pc_sq();
        auto to       = dirty.threatened_sq();
        auto add      = dirty.add();

        auto&     insert = add ? added : removed;
        IndexType index  = make_index(perspective, attacker, from, to, attacked, mirror);

        if (index < Dimensions)
            insert.push_back(index);
    }
}

bool FullThreats::requires_refresh(const DiffType& diff, Color perspective) {
    return diff.requires_refresh[perspective];
}

}  // namespace Stockfish::Eval::NNUE::Features
