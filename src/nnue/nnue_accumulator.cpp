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

#include "nnue_accumulator.h"

#include <algorithm>
#include <cassert>
#include <new>
#include <utility>

#include "../bitboard.h"
#include "../misc.h"
#include "../position.h"
#include "../types.h"
#include "nnue_architecture.h"
#include "nnue_common.h"
#include "nnue_feature_transformer.h"  // IWYU pragma: keep
#include "simd.h"

namespace Stockfish::Eval::NNUE {

using namespace SIMD;

namespace {

template<bool Forward>
void update_accumulator_incremental(Color                     perspective,
                                    const FeatureTransformer& featureTransformer,
                                    const int                 bucket,
                                    const bool                mirror,
                                    AccumulatorState&         target_state,
                                    const AccumulatorState&   computed);

void update_accumulator_incremental_both(const FeatureTransformer& featureTransformer,
                                         int                       white_bucket,
                                         bool                      white_mirror,
                                         int                       black_bucket,
                                         bool                      black_mirror,
                                         AccumulatorState&         target_state,
                                         const AccumulatorState&   computed);

void update_accumulator_refresh_cache(Color                     perspective,
                                      const FeatureTransformer& featureTransformer,
                                      const Position&           pos,
                                      AccumulatorState&         accumulatorState,
                                      AccumulatorCaches&        cache);
}

const AccumulatorState& AccumulatorStack::latest() const noexcept { return accumulators[size - 1]; }

AccumulatorState& AccumulatorStack::mut_latest() noexcept { return accumulators[size - 1]; }

void AccumulatorStack::reset() noexcept {
    accumulators[0].dirtyPiece = {};
    new (&accumulators[0].dirtyThreats) DirtyThreats;
    accumulators[0].computed.fill(false);
    size = 1;
}

Dirties& AccumulatorStack::push() noexcept {
    assert(size < MaxSize);
    auto& st = accumulators[size];
    st.computed.fill(false);
    new (&st.dirtyThreats) DirtyThreats;
    size++;
    return st;
}

void AccumulatorStack::pop() noexcept {
    assert(size > 1);
    size--;
}

void AccumulatorStack::evaluate(const Position&           pos,
                                const FeatureTransformer& featureTransformer,
                                // Silence spurious warning on GCC 10
                                [[maybe_unused]] AccumulatorCaches& cache) noexcept {
    const usize last_white = find_last_usable_accumulator(WHITE);
    const usize last_black = find_last_usable_accumulator(BLACK);

    if (accumulators[last_white].computed[WHITE] && accumulators[last_black].computed[BLACK])
        forward_update_incremental_both(pos, featureTransformer, last_white, last_black);
    else
    {
        evaluate_side(WHITE, pos, featureTransformer, cache, last_white);
        evaluate_side(BLACK, pos, featureTransformer, cache, last_black);
    }
}

void AccumulatorStack::evaluate_side(Color                     perspective,
                                     const Position&           pos,
                                     const FeatureTransformer& featureTransformer,
                                     AccumulatorCaches&        cache,
                                     usize                     last_usable_accum) noexcept {

    if (accumulators[last_usable_accum].computed[perspective])
        forward_update_incremental(perspective, pos, featureTransformer, last_usable_accum);

    else
    {
        update_accumulator_refresh_cache(perspective, featureTransformer, pos, mut_latest(), cache);
        backward_update_incremental(perspective, pos, featureTransformer, last_usable_accum);
    }
}

// Find the earliest usable accumulator, this can either be a computed accumulator or the accumulator
// state just before a change that requires full refresh.
usize AccumulatorStack::find_last_usable_accumulator(Color perspective) const noexcept {

    for (usize curr_idx = size - 1; curr_idx > 0; curr_idx--)
    {
        if (accumulators[curr_idx].computed[perspective])
            return curr_idx;

        // Threat feature set refreshes require a king move across the center, i.e.,
        // a subset of halfka refreshes
        if (PSQFeatureSet::requires_refresh(accumulators[curr_idx].dirtyPiece, perspective))
            return curr_idx;
    }

    return 0;
}

void AccumulatorStack::forward_update_incremental(Color                     perspective,
                                                  const Position&           pos,
                                                  const FeatureTransformer& featureTransformer,
                                                  const usize               begin) noexcept {

    assert(begin < accumulators.size());
    assert(accumulators[begin].computed[perspective]);

    auto [bucket, mirror, _] = PSQFeatureSet::make_feature_bucket(perspective, pos);

    for (usize next = begin + 1; next < size; next++)
        update_accumulator_incremental<true>(perspective, featureTransformer, bucket, mirror,
                                             accumulators[next], accumulators[next - 1]);

    assert(latest().computed[perspective]);
}

void AccumulatorStack::backward_update_incremental(Color                     perspective,
                                                   const Position&           pos,
                                                   const FeatureTransformer& featureTransformer,
                                                   const usize               end) noexcept {

    assert(end < accumulators.size());
    assert(end < size);
    assert(latest().computed[perspective]);

    auto [bucket, mirror, _] = PSQFeatureSet::make_feature_bucket(perspective, pos);

    for (i64 next = i64(size) - 2; next >= i64(end); next--)
        update_accumulator_incremental<false>(perspective, featureTransformer, bucket, mirror,
                                              accumulators[next], accumulators[next + 1]);

    assert(accumulators[end].computed[perspective]);
}

void AccumulatorStack::forward_update_incremental_both(const Position&           pos,
                                                       const FeatureTransformer& featureTransformer,
                                                       usize                     white_begin,
                                                       usize black_begin) noexcept {

    assert(white_begin < size);
    assert(black_begin < size);
    assert(accumulators[white_begin].computed[WHITE]);
    assert(accumulators[black_begin].computed[BLACK]);

    auto [white_bucket, white_mirror, _w] = PSQFeatureSet::make_feature_bucket(WHITE, pos);
    auto [black_bucket, black_mirror, _b] = PSQFeatureSet::make_feature_bucket(BLACK, pos);
    const usize shared_begin              = std::max(white_begin, black_begin);

    // Catch up the lagging perspective, then traverse the common suffix once.
    for (usize next = white_begin + 1; next <= shared_begin; ++next)
        update_accumulator_incremental<true>(WHITE, featureTransformer, white_bucket, white_mirror,
                                             accumulators[next], accumulators[next - 1]);
    for (usize next = black_begin + 1; next <= shared_begin; ++next)
        update_accumulator_incremental<true>(BLACK, featureTransformer, black_bucket, black_mirror,
                                             accumulators[next], accumulators[next - 1]);

    for (usize next = shared_begin + 1; next < size; ++next)
        update_accumulator_incremental_both(featureTransformer, white_bucket, white_mirror,
                                            black_bucket, black_mirror, accumulators[next],
                                            accumulators[next - 1]);

    assert(latest().computed[WHITE]);
    assert(latest().computed[BLACK]);
}

namespace {

constexpr IndexType Dimensions = FeatureTransformer::OutputDimensions;

#ifdef USE_RVV

struct Tiling {
    static constexpr int NumRegs     = 1;
    static constexpr int NumPsqtRegs = 1;
};

using Tile     = vint16m8_t;
using PsqtTile = vint32m1_t;

sf_always_inline Tile load_tile(IndexType j, const i16* data) {
    usize vl = __riscv_vsetvl_e16m8(Dimensions - j);
    return __riscv_vle16_v_i16m8(data + j, vl);
}

sf_always_inline void store_tile(IndexType j, i16* dest, Tile acc) {
    usize vl = __riscv_vsetvl_e16m8(Dimensions - j);
    __riscv_vse16_v_i16m8(dest + j, acc, vl);
}

sf_always_inline PsqtTile load_psqt(IndexType j, const i32* data) {
    usize vl = __riscv_vsetvl_e32m1(PSQTBuckets - j);
    return __riscv_vle32_v_i32m1(data + j, vl);
}

sf_always_inline void store_psqt(IndexType j, i32* dest, PsqtTile psqt) {
    usize vl = __riscv_vsetvl_e32m1(PSQTBuckets - j);
    __riscv_vse32_v_i32m1(dest + j, psqt, vl);
}

sf_always_inline void increment_index(IndexType& j) { j += __riscv_vsetvl_e16m8(Dimensions - j); }

sf_always_inline void increment_psqt_index(IndexType& j) {
    j += __riscv_vsetvl_e32m1(PSQTBuckets - j);
}

template<int sign>
sf_always_inline Tile apply(IndexType j, Tile acc, const i8* data) {
    static_assert(sign == 1 || sign == -1);
    usize     vl       = __riscv_vsetvl_e16m8(Dimensions - j);
    vint8m4_t data_vec = __riscv_vle8_v_i8m4(data + j, vl);
    if constexpr (sign == +1)
        acc = __riscv_vwadd_wv_i16m8(acc, data_vec, vl);
    else
        acc = __riscv_vwsub_wv_i16m8(acc, data_vec, vl);
    return acc;
}

template<int sign>
sf_always_inline PsqtTile apply(IndexType j, PsqtTile acc, const i32* data) {
    static_assert(sign == 1 || sign == -1);
    usize      vl       = __riscv_vsetvl_e32m1(PSQTBuckets - j);
    vint32m1_t data_vec = __riscv_vle32_v_i32m1(data + j, vl);
    if constexpr (sign == +1)
        acc = __riscv_vadd_vv_i32m1(acc, data_vec, vl);
    else
        acc = __riscv_vsub_vv_i32m1(acc, data_vec, vl);
    return acc;
}

#else  // VECTOR or non VECTOR

    #ifdef VECTOR

using Tiling = SIMDTiling<Dimensions, Dimensions, PSQTBuckets>;

    #else

// Treat scalar impl as degenerate size-1 vector
struct Tiling {
    static constexpr int NumRegs        = 1;
    static constexpr int NumPsqtRegs    = 1;
    static constexpr int TileHeight     = 1;
    static constexpr int PsqtTileHeight = 1;
};

using vec_t      = i16;
using vec_i8_t   = i8;
using psqt_vec_t = i32;

        #define vec_add_16(a, b) ((a) + (b))
        #define vec_sub_16(a, b) ((a) - (b))
        #define vec_add_psqt_32(a, b) ((a) + (b))
        #define vec_sub_psqt_32(a, b) ((a) - (b))
        #define vec_convert_8_16(a) (i16(a))

    #endif

struct PsqtTile {
    psqt_vec_t inner[Tiling::NumPsqtRegs];
    auto&      operator[](int i) { return inner[i]; }
};

struct Tile {
    vec_t inner[Tiling::NumRegs];
    auto& operator[](int i) { return inner[i]; }
};

sf_always_inline Tile load_tile(IndexType j, const i16* data) {
    Tile  acc;
    auto* column = reinterpret_cast<const vec_t*>(&data[j]);
    for (IndexType k = 0; k < Tiling::NumRegs; ++k)
        acc[k] = column[k];
    return acc;
}

sf_always_inline void store_tile(IndexType j, i16* dest, Tile acc) {
    auto* column = reinterpret_cast<vec_t*>(&dest[j]);
    for (IndexType k = 0; k < Tiling::NumRegs; ++k)
        column[k] = acc[k];
}

sf_always_inline PsqtTile load_psqt(IndexType j, const i32* data) {
    PsqtTile psqt;
    auto*    column = reinterpret_cast<const psqt_vec_t*>(&data[j]);
    for (IndexType k = 0; k < Tiling::NumPsqtRegs; ++k)
        psqt[k] = column[k];
    return psqt;
}

sf_always_inline void store_psqt(IndexType j, i32* dest, PsqtTile psqt) {
    auto* column = reinterpret_cast<psqt_vec_t*>(&dest[j]);
    for (IndexType k = 0; k < Tiling::NumPsqtRegs; ++k)
        column[k] = psqt[k];
}

sf_always_inline void increment_index(IndexType& j) { j += Tiling::TileHeight; }

sf_always_inline void increment_psqt_index(IndexType& j) { j += Tiling::PsqtTileHeight; }

template<int sign>
sf_always_inline Tile apply(IndexType j, Tile acc, const i8* data) {
    static_assert(sign == 1 || sign == -1);
    const auto* column = reinterpret_cast<const vec_i8_t*>(data + j);
    #ifdef USE_NEON
    for (IndexType k = 0; k < Tiling::NumRegs; k += 2)
    {
        if constexpr (sign == +1)
        {
            acc[k]     = vaddw_s8(acc[k], vget_low_s8(column[k / 2]));
            acc[k + 1] = vaddw_high_s8(acc[k + 1], column[k / 2]);
        }
        else
        {
            acc[k]     = vsubw_s8(acc[k], vget_low_s8(column[k / 2]));
            acc[k + 1] = vsubw_high_s8(acc[k + 1], column[k / 2]);
        }
    }
    #elif defined(USE_LSX) && !defined(USE_LASX)
    for (IndexType k = 0; k < Tiling::NumRegs; k += 2)
    {
        const __m128i weight = __lsx_vld(reinterpret_cast<const void*>(&column[k]), 0);
        if constexpr (sign == +1)
        {
            acc[k]     = vec_add_16(acc[k], __lsx_vsllwil_h_b(weight, 0));
            acc[k + 1] = vec_add_16(acc[k + 1], __lsx_vexth_h_b(weight));
        }
        else
        {
            acc[k]     = vec_sub_16(acc[k], __lsx_vsllwil_h_b(weight, 0));
            acc[k + 1] = vec_sub_16(acc[k + 1], __lsx_vexth_h_b(weight));
        }
    }
    #else
    for (IndexType k = 0; k < Tiling::NumRegs; ++k)
        if constexpr (sign == +1)
            acc[k] = vec_add_16(acc[k], vec_convert_8_16(column[k]));
        else
            acc[k] = vec_sub_16(acc[k], vec_convert_8_16(column[k]));
    #endif
    return acc;
}

template<int sign>
sf_always_inline PsqtTile apply(IndexType j, PsqtTile acc, const i32* data) {
    static_assert(sign == 1 || sign == -1);
    const auto* column = reinterpret_cast<const psqt_vec_t*>(data + j);
    for (IndexType k = 0; k < Tiling::NumPsqtRegs; ++k)
        if constexpr (sign == +1)
            acc[k] = vec_add_psqt_32(acc[k], column[k]);
        else
            acc[k] = vec_sub_psqt_32(acc[k], column[k]);
    return acc;
}

#endif

template<int sign, bool Incremental = false>
sf_always_inline Tile apply_psq_features(IndexType                       j,
                                         Tile                            acc,
                                         const PSQFeatureSet::IndexList& list,
                                         const FeatureTransformer&       ft) {
    static_assert(sign == 1 || sign == -1);
    if constexpr (Incremental)
    {
        assert(list.size() == 1 || list.size() == 2);
        acc = apply<sign>(j, acc, &ft.weights[list[0] * Dimensions]);
        if (list.size() > 1)
            acc = apply<sign>(j, acc, &ft.weights[list[1] * Dimensions]);
        return acc;
    }
    for (int i = 0; i < list.ssize(); ++i)
        acc = apply<sign>(j, acc, &ft.weights[list[i] * Dimensions]);
    return acc;
}

template<int sign>
sf_always_inline Tile apply_threat_features(IndexType                          j,
                                            Tile                               acc,
                                            const ThreatFeatureSet::IndexList& list,
                                            const FeatureTransformer&          ft) {
    static_assert(sign == 1 || sign == -1);
    for (int i = 0; i < list.ssize(); ++i)
        acc = apply<sign>(j, acc, &ft.threatWeights[list[i] * Dimensions]);
    return acc;
}

template<int sign, typename IdxType, usize MaxLen>
sf_always_inline PsqtTile apply_psqt(IndexType                         j,
                                     PsqtTile                          acc,
                                     const ValueList<IdxType, MaxLen>& list,
                                     const PSQTWeightType*             weights) {
    static_assert(sign == 1 || sign == -1);
    for (int i = 0; i < list.ssize(); ++i)
        acc = apply<sign>(j, acc, &weights[list[i] * PSQTBuckets]);
    return acc;
}

void apply_combined(Color                              perspective,
                    const FeatureTransformer&          featureTransformer,
                    const AccumulatorState&            from,
                    AccumulatorState&                  to,
                    const PSQFeatureSet::IndexList&    psqAdded,
                    const PSQFeatureSet::IndexList&    psqRemoved,
                    const ThreatFeatureSet::IndexList& thrAdded,
                    const ThreatFeatureSet::IndexList& thrRemoved) {

    const auto& fromAcc = from.accumulation[perspective];
    auto&       toAcc   = to.accumulation[perspective];

    const auto& fromPsqtAcc = from.psqtAccumulation[perspective];
    auto&       toPsqtAcc   = to.psqtAccumulation[perspective];

    Tile     acc;
    PsqtTile psqt;

    for (IndexType j = 0; j < Dimensions; increment_index(j))
    {
        acc = load_tile(j, fromAcc.data());

        acc = apply_psq_features<-1, true>(j, acc, psqRemoved, featureTransformer);
        acc = apply_psq_features<+1, true>(j, acc, psqAdded, featureTransformer);

        acc = apply_threat_features<-1>(j, acc, thrRemoved, featureTransformer);
        acc = apply_threat_features<+1>(j, acc, thrAdded, featureTransformer);

        store_tile(j, toAcc.data(), acc);
    }

    for (IndexType j = 0; j < PSQTBuckets; increment_psqt_index(j))
    {
        psqt = load_psqt(j, fromPsqtAcc.data());

        psqt = apply_psqt<-1>(j, psqt, psqRemoved, featureTransformer.psqtWeights.data());
        psqt = apply_psqt<+1>(j, psqt, psqAdded, featureTransformer.psqtWeights.data());

        psqt = apply_psqt<-1>(j, psqt, thrRemoved, featureTransformer.threatPsqtWeights.data());
        psqt = apply_psqt<+1>(j, psqt, thrAdded, featureTransformer.threatPsqtWeights.data());

        store_psqt(j, toPsqtAcc.data(), psqt);
    }
}

template<bool Forward>
void update_accumulator_incremental(Color                     perspective,
                                    const FeatureTransformer& featureTransformer,
                                    const int                 bucket,
                                    const bool                mirror,
                                    AccumulatorState&         target_state,
                                    const AccumulatorState&   computed) {

    assert(computed.computed[perspective]);
    assert(!target_state.computed[perspective]);

    // The size must be enough to contain the largest possible update.
    // That might depend on the feature set and generally relies on the
    // feature set's update cost calculation to be correct and never allow
    // updates with more added/removed features than MaxActiveDimensions.
    PSQFeatureSet::IndexList    psqRemoved, psqAdded;
    ThreatFeatureSet::IndexList thrRemoved, thrAdded;

    const auto& dirtyPiece   = Forward ? target_state.dirtyPiece : computed.dirtyPiece;
    const auto& dirtyThreats = Forward ? target_state.dirtyThreats : computed.dirtyThreats;

    // Used solely for prefetching
    const auto* threatBase = &featureTransformer.threatWeights[0];
    IndexType   pfStride   = FeatureTransformer::OutputDimensions;

    if constexpr (Forward)
    {
        ThreatFeatureSet::append_changed_indices(perspective, mirror, dirtyThreats, thrRemoved,
                                                 thrAdded, threatBase, pfStride);
        PSQFeatureSet::append_changed_indices(perspective, bucket, mirror, dirtyPiece, psqRemoved,
                                              psqAdded);
    }
    else
    {
        ThreatFeatureSet::append_changed_indices(perspective, mirror, dirtyThreats, thrAdded,
                                                 thrRemoved, threatBase, pfStride);
        PSQFeatureSet::append_changed_indices(perspective, bucket, mirror, dirtyPiece, psqAdded,
                                              psqRemoved);
    }

    apply_combined(perspective, featureTransformer, computed, target_state, psqAdded, psqRemoved,
                   thrAdded, thrRemoved);

    target_state.computed[perspective] = true;
}

void update_accumulator_incremental_both(const FeatureTransformer& featureTransformer,
                                         int                       white_bucket,
                                         bool                      white_mirror,
                                         int                       black_bucket,
                                         bool                      black_mirror,
                                         AccumulatorState&         target_state,
                                         const AccumulatorState&   computed) {

    assert(computed.computed[WHITE]);
    assert(computed.computed[BLACK]);
    assert(!target_state.computed[WHITE]);
    assert(!target_state.computed[BLACK]);

    PSQFeatureSet::IndexList    psq_removed[COLOR_NB], psq_added[COLOR_NB];
    ThreatFeatureSet::IndexList thr_removed[COLOR_NB], thr_added[COLOR_NB];

    const auto* threat_base = &featureTransformer.threatWeights[0];
    const auto  pf_stride   = FeatureTransformer::OutputDimensions;

    ThreatFeatureSet::append_changed_indices_both(
      white_mirror, black_mirror, target_state.dirtyThreats, thr_removed[WHITE], thr_added[WHITE],
      thr_removed[BLACK], thr_added[BLACK], threat_base, pf_stride);
    PSQFeatureSet::append_changed_indices(WHITE, white_bucket, white_mirror,
                                          target_state.dirtyPiece, psq_removed[WHITE],
                                          psq_added[WHITE]);
    PSQFeatureSet::append_changed_indices(BLACK, black_bucket, black_mirror,
                                          target_state.dirtyPiece, psq_removed[BLACK],
                                          psq_added[BLACK]);

    apply_combined(WHITE, featureTransformer, computed, target_state, psq_added[WHITE],
                   psq_removed[WHITE], thr_added[WHITE], thr_removed[WHITE]);
    apply_combined(BLACK, featureTransformer, computed, target_state, psq_added[BLACK],
                   psq_removed[BLACK], thr_added[BLACK], thr_removed[BLACK]);

    target_state.computed[WHITE] = true;
    target_state.computed[BLACK] = true;
}

Bitboard get_changed_pieces(const std::array<Piece, SQUARE_NB>& oldPieces,
                            const std::array<Piece, SQUARE_NB>& newPieces) {
#if defined(USE_AVX2)
    static_assert(sizeof(Piece) == 1);
    Bitboard sameBB = 0;

    for (int i : {0, 32, 58})
    {
        const __m256i old_v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&oldPieces[i]));
        const __m256i new_v = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(&newPieces[i]));
        const __m256i cmpEqual  = _mm256_cmpeq_epi8(old_v, new_v);
        const u32     equalMask = _mm256_movemask_epi8(cmpEqual);
        sameBB |= static_cast<Bitboard>(equalMask) << i;
    }
    return ~sameBB;
#elif defined(USE_LASX)
    static_assert(sizeof(Piece) == 1);

    Bitboard changed = 0;

    for (int i : {0, 32, 58})
    {
        const __m256i old_v = __lasx_xvld(reinterpret_cast<const void*>(&oldPieces[i]), 0);
        const __m256i new_v = __lasx_xvld(reinterpret_cast<const void*>(&newPieces[i]), 0);
        const __m256i diff  = __lasx_xvxor_v(old_v, new_v);
        const __m256i mask  = __lasx_xvmsknz_b(diff);
        const auto    lo    = __lasx_xvpickve2gr_d(mask, 0);
        const auto    hi    = __lasx_xvpickve2gr_d(mask, 2);

        changed |= (static_cast<Bitboard>(lo) | (static_cast<Bitboard>(hi) << 16)) << i;
    }

    return changed;
#elif defined(USE_LSX)
    static_assert(sizeof(Piece) == 1);

    Bitboard changed = 0;

    for (int i : {0, 16, 32, 48, 64, 74})
    {
        const __m128i old_v = __lsx_vld(reinterpret_cast<const void*>(&oldPieces[i]), 0);
        const __m128i new_v = __lsx_vld(reinterpret_cast<const void*>(&newPieces[i]), 0);
        const __m128i diff  = __lsx_vxor_v(old_v, new_v);
        const __m128i mask  = __lsx_vmsknz_b(diff);

        changed |= static_cast<Bitboard>(__lsx_vpickve2gr_d(mask, 0)) << i;
    }

    return changed;
#elif defined(USE_NEON)
    Bitboard sameBB = 0;

    constexpr uint8x16_t mask_weights = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};

    for (int i : {0, 16, 32, 48, 64, 74})
    {
        uint8x16_t old_v = vld1q_u8(reinterpret_cast<const u8*>(&oldPieces[i]));
        uint8x16_t new_v = vld1q_u8(reinterpret_cast<const u8*>(&newPieces[i]));

        uint8x16_t eq     = vceqq_u8(old_v, new_v);
        uint8x16_t masked = vandq_u8(eq, mask_weights);
        u16        mask16 = vaddv_u8(vget_low_u8(masked)) | (vaddv_u8(vget_high_u8(masked)) << 8);

        sameBB |= static_cast<Bitboard>(mask16) << i;
    }

    return ~sameBB;
#elif defined(USE_SSE2)
    Bitboard sameBB = 0;

    for (int i : {0, 16, 32, 48, 64, 74})
    {
        const __m128i old_v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&oldPieces[i]));
        const __m128i new_v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&newPieces[i]));
        const __m128i same  = _mm_cmpeq_epi8(old_v, new_v);

        sameBB |= static_cast<Bitboard>(_mm_movemask_epi8(same)) << i;
    }

    return ~sameBB;
#elif defined(USE_RVV)

    #define RVV_MASK(mx, bx, offset, n) \
        __riscv_vmv_x_s_u64m1_u64(__riscv_vreinterpret_v_u8m1_u64m1( \
          __riscv_vreinterpret_v_b##bx##_u8m1(__riscv_vmsne_vv_i8m##mx##_b##bx( \
            __riscv_vle8_v_i8m##mx(reinterpret_cast<const i8*>(oldPieces.data() + offset), n), \
            __riscv_vle8_v_i8m##mx(reinterpret_cast<const i8*>(newPieces.data() + offset), n), \
            n))))
    #define IMPL(mx, bx) \
        { \
            u64 lo   = RVV_MASK(mx, bx, 0, 64); \
            u64 tail = RVV_MASK(mx, bx, 58, 32); \
            return Bitboard(lo) | (Bitboard(tail) << 58); \
        }


    usize vl = __riscv_vsetvlmax_e8m1();
    if (vl >= 64)
        IMPL(1, 8);
    else if (vl == 32)
        IMPL(2, 4);
    else
        IMPL(4, 2);

    #undef IMPL
    #undef RVV_MASK

#else
    Bitboard changed = 0;

    for (Square sq = SQUARE_ZERO; sq < SQUARE_NB; ++sq)
        changed |= static_cast<Bitboard>(oldPieces[sq] != newPieces[sq]) << sq;

    return changed;
#endif
}

// HalfKA data comes from the Finny table entry, while the threats are built
// from the active threat features
void update_accumulator_refresh_cache(Color                     perspective,
                                      const FeatureTransformer& featureTransformer,
                                      const Position&           pos,
                                      AccumulatorState&         accumulator,
                                      AccumulatorCaches&        cache) {

    auto [bucket, mirror, attack_bucket] = PSQFeatureSet::make_feature_bucket(perspective, pos);

    auto cache_index = AccumulatorCaches::KingCacheMaps[pos.king_square(perspective)];
    if (cache_index < 3 && mirror)
    {
        cache_index += 9;
        if (PSQFeatureSet::requires_mid_mirror(pos, perspective))
            cache_index += 3;
    }

    auto& entry = cache[cache_index * PSQFeatureSet::AttackBucketNB + attack_bucket][perspective];
    PSQFeatureSet::IndexList removed, added;

    const Bitboard changedBB = get_changed_pieces(entry.pieces, pos.piece_array());
    Bitboard       removedBB = changedBB & entry.pieceBB;
    Bitboard       addedBB   = changedBB & pos.pieces();

    while (removedBB)
    {
        Square sq = pop_lsb(removedBB);
        removed.push_back(
          PSQFeatureSet::make_index(perspective, sq, entry.pieces[sq], bucket, mirror));
    }
    while (addedBB)
    {
        Square sq = pop_lsb(addedBB);
        added.push_back(
          PSQFeatureSet::make_index(perspective, sq, pos.piece_on(sq), bucket, mirror));
    }

    entry.pieceBB = pos.pieces();
    entry.pieces  = pos.piece_array();

    ThreatFeatureSet::IndexList active;
    ThreatFeatureSet::append_active_indices(perspective, pos, active);

    accumulator.computed[perspective] = true;

    Tile     acc;
    PsqtTile psqt;

    for (IndexType j = 0; j < Dimensions; increment_index(j))
    {
        acc = load_tile(j, entry.accumulation.data());

        acc = apply_psq_features<-1>(j, acc, removed, featureTransformer);
        acc = apply_psq_features<+1>(j, acc, added, featureTransformer);

        store_tile(j, entry.accumulation.data(), acc);

        acc = apply_threat_features<+1>(j, acc, active, featureTransformer);

        store_tile(j, accumulator.accumulation[perspective].data(), acc);
    }

    for (IndexType j = 0; j < PSQTBuckets; increment_psqt_index(j))
    {
        psqt = load_psqt(j, entry.psqtAccumulation.data());

        psqt = apply_psqt<-1>(j, psqt, removed, featureTransformer.psqtWeights.data());
        psqt = apply_psqt<+1>(j, psqt, added, featureTransformer.psqtWeights.data());

        store_psqt(j, entry.psqtAccumulation.data(), psqt);

        psqt = apply_psqt<+1>(j, psqt, active, featureTransformer.threatPsqtWeights.data());

        store_psqt(j, accumulator.psqtAccumulation[perspective].data(), psqt);
    }
}

}

}
