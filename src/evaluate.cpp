/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2024 The Stockfish developers (see AUTHORS file)

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
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "misc.h"
#include "nnue/evaluate_nnue.h"
#include "position.h"
#include "thread.h"
#include "types.h"
#include "uci.h"


namespace Stockfish {

namespace Eval {

std::string currentEvalFileName = "None";

// Tries to load a NNUE network at startup time, or when the engine
// receives a UCI command "setoption name EvalFile value .*.nnue"
// The name of the NNUE network is always retrieved from the EvalFile option.
// We search the given network in two locations: in the active working directory and
// in the engine directory.
void NNUE::init() {

    std::string eval_file = std::string(Options["EvalFile"]);
    if (eval_file.empty())
        eval_file = EvalFileDefaultName;

    std::vector<std::string> dirs = {"", CommandLine::binaryDirectory};

    for (const std::string& directory : dirs)
        if (currentEvalFileName != eval_file)
        {
            std::ifstream     stream(directory + eval_file, std::ios::binary);
            std::stringstream ss = read_zipped_nnue(directory + eval_file);
            if (NNUE::load_eval(eval_file, stream) || NNUE::load_eval(eval_file, ss))
                currentEvalFileName = eval_file;
        }
}

// Verifies that the last net used was loaded successfully
void NNUE::verify() {

    std::string eval_file = std::string(Options["EvalFile"]);
    if (eval_file.empty())
        eval_file = EvalFileDefaultName;

    if (currentEvalFileName != eval_file)
    {

        std::string msg1 =
          "Network evaluation parameters compatible with the engine must be available.";
        std::string msg2 = "The network file " + eval_file + " was not loaded successfully.";
        std::string msg3 = "The UCI option EvalFile might need to specify the full path, "
                           "including the directory name, to the network file.";
        std::string msg4 =
          "The default net can be downloaded from: "
          "https://github.com/official-pikafish/Networks/releases/download/master-net/"
          + std::string(EvalFileDefaultName);
        std::string msg5 = "The engine will be terminated now.";

        sync_cout << "info string ERROR: " << msg1 << sync_endl;
        sync_cout << "info string ERROR: " << msg2 << sync_endl;
        sync_cout << "info string ERROR: " << msg3 << sync_endl;
        sync_cout << "info string ERROR: " << msg4 << sync_endl;
        sync_cout << "info string ERROR: " << msg5 << sync_endl;

        exit(EXIT_FAILURE);
    }

    sync_cout << "info string NNUE evaluation using " << eval_file << " enabled" << sync_endl;
}
}


// Returns a static, purely materialistic evaluation of the position from
// the point of view of the given color. It can be divided by PawnValue to get
// an approximation of the material advantage on the board in terms of pawns.
int Eval::simple_eval(const Position& pos, Color c) {
    return PawnValue * (pos.count<PAWN>(c) - pos.count<PAWN>(~c))
         + AdvisorValue * (pos.count<ADVISOR>(c) - pos.count<ADVISOR>(~c))
         + BishopValue * (pos.count<BISHOP>(c) - pos.count<BISHOP>(~c))
         + (pos.major_material(c) - pos.major_material(~c));
}

// Evaluate is the evaluator for the outer world. It returns a static
// evaluation of the position from the point of view of the side to move.
Value Eval::evaluate(const Position& pos) {

    assert(!pos.checkers());

    int   v;
    Color stm        = pos.side_to_move();
    int   shuffling  = pos.rule60_count();
    int   simpleEval = simple_eval(pos, stm);

    int   nnueComplexity;
    Value nnue = NNUE::evaluate(pos, true, &nnueComplexity);

    int optimism = pos.this_thread()->optimism[stm];

    // Blend optimism and eval with nnue complexity and material imbalance
    optimism += optimism * (nnueComplexity + std::abs(simpleEval - nnue)) / 708;
    nnue -= nnue * (nnueComplexity + std::abs(simpleEval - nnue)) / 32858;

    int mm = pos.major_material() / 42;
    v      = (nnue * (545 + mm) + optimism * (128 + mm)) / 1312;

    // Damp down the evaluation linearly when shuffling
    v = v * (263 - shuffling) / 192;

    // Guarantee evaluation does not hit the mate range
    v = std::clamp(int(v), VALUE_MATED_IN_MAX_PLY + 1, VALUE_MATE_IN_MAX_PLY - 1);

    return v;
}

// Like evaluate(), but instead of returning a value, it returns
// a string (suitable for outputting to stdout) that contains the detailed
// descriptions and values of each evaluation term. Useful for debugging.
// Trace scores are from white's point of view
std::string Eval::trace(Position& pos) {

    if (pos.checkers())
        return "Final evaluation: none (in check)";

    std::stringstream ss;
    ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2);

    Value v;

    // Reset any global variable used in eval
    pos.this_thread()->bestValue       = VALUE_ZERO;
    pos.this_thread()->optimism[WHITE] = VALUE_ZERO;
    pos.this_thread()->optimism[BLACK] = VALUE_ZERO;

    ss << '\n' << NNUE::trace(pos) << '\n';

    ss << std::showpoint << std::showpos << std::fixed << std::setprecision(2) << std::setw(15);

    v = NNUE::evaluate(pos);
    v = pos.side_to_move() == WHITE ? v : -v;
    ss << "NNUE evaluation        " << 0.01 * UCI::to_cp(v) << " (white side)\n";

    v = evaluate(pos);
    v = pos.side_to_move() == WHITE ? v : -v;
    ss << "Final evaluation       " << 0.01 * UCI::to_cp(v) << " (white side)";
    ss << " [with scaled NNUE, ...]";
    ss << "\n";

    return ss.str();
}

}  // namespace Stockfish
