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

#include "evaluate.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>

#include "misc.h"
#include "nnue/network.h"
#include "nnue/nnue_misc.h"
#include "position.h"
#include "types.h"
#include "uci.h"
#include "nnue/nnue_accumulator.h"

namespace Stockfish {

static int simple_eval(const Position& pos) {
    const Color c = pos.side_to_move();
    return PawnValue * (pos.count<PAWN>(c) - pos.count<PAWN>(~c))
         + AdvisorValue * (pos.count<ADVISOR>(c) - pos.count<ADVISOR>(~c))
         + BishopValue * (pos.count<BISHOP>(c) - pos.count<BISHOP>(~c)) + pos.major_material(c)
         - pos.major_material(~c);
}

Value scale_evaluation(Value nnue, int optimism, const Position& pos);

Value Eval::evaluate(const Eval::NNUE::Network&     network,
                     const Position&                pos,
                     Eval::NNUE::AccumulatorStack&  accumulators,
                     Eval::NNUE::AccumulatorCaches& caches,
                     int                            optimism) {

    assert(!pos.checkers());
    Value nnue = network.evaluate(pos, accumulators, caches);
    return scale_evaluation(nnue, optimism, pos);
}

// Applies search-dependent scaling (optimism and rule60) to the raw NNUE eval
Value scale_evaluation(Value nnue, int optimism, const Position& pos) {
    Value se = simple_eval(pos);

    // Normalize the raw evaluations to [-1024, 1024] to measure their correlation.
    int se_norm   = (se * 1024) / (std::abs(se) + 1024);
    int nnue_norm = (nnue * 1024) / (std::abs(nnue) + 1024);
    // When NNUE and material agree (positive alignment), the position is straightforward;
    // otherwise (negative alignment) it involves complex compensation. In a representative
    // sample, alignment averages -1 or so, i.e. it is well-centered in [-2048, 2048].
    int alignment = (se_norm * nnue_norm) / 512;

    // When winning, we favor easy positions, and vice versa
    int base_eval = nnue + (nnue * alignment) / 65536 + (optimism * alignment) / 16384;

    // Scale the combined evaluation by total material
    int material = PawnValue * pos.count<PAWN>() + AdvisorValue * pos.count<ADVISOR>()
                 + BishopValue * pos.count<BISHOP>() + pos.major_material();
    int v        = base_eval * i64(80030 + material) / 80030;

    // Damp down the evaluation linearly when shuffling
    v -= v * pos.rule60_count() / 244;

    // Guarantee that the evaluation does not hit the mate range
    v = std::clamp(v, VALUE_MATED_IN_MAX_PLY + 1, VALUE_MATE_IN_MAX_PLY - 1);

    return v;
}

// Like evaluate(), but instead of returning a value, it returns
// a string (suitable for outputting to stdout) that contains the detailed
// descriptions and values of each evaluation term. Useful for debugging.
// Trace scores are from white's point of view
std::string Eval::trace(Position& pos, const Eval::NNUE::Network& network) {

    if (pos.checkers())
        return "Final evaluation: none (in check)";

    auto accumulators = std::make_unique<Eval::NNUE::AccumulatorStack>();
    auto caches       = std::make_unique<Eval::NNUE::AccumulatorCaches>(network);

    std::stringstream ss;
    ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2);
    ss << '\n' << NNUE::trace(pos, network, *caches) << '\n';

    ss << std::showpoint << std::showpos << std::fixed << std::setprecision(2) << std::setw(15);

    Value nnue = network.evaluate(pos, *accumulators, *caches);
    Value s_v  = scale_evaluation(nnue, VALUE_ZERO, pos);  // requires stm perspective

    ss << "NNUE evaluation          " << nnue << " (side to move, internal units)\n";

    nnue = pos.side_to_move() == WHITE ? nnue : -nnue;
    s_v  = pos.side_to_move() == WHITE ? s_v : -s_v;

    ss << "NNUE evaluation        " << 0.01 * UCIEngine::to_cp(nnue, pos) << " (white side)\n";
    ss << "Final evaluation      ";
    ss << 0.01 * UCIEngine::to_cp(s_v, pos) << " (white side)";
    ss << " [with scaled NNUE, ...]\n";

    return ss.str();
}

}  // namespace Stockfish
