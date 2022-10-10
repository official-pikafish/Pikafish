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

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>   // For std::memset
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <streambuf>
#include <vector>

#include "bitboard.h"
#include "evaluate.h"
#include "misc.h"
#include "thread.h"
#include "uci.h"

using namespace std;

namespace Stockfish {

namespace Eval {

  string currentEvalFileName = "None";

  /// NNUE::init() tries to load a NNUE network at startup time, or when the engine
  /// receives a UCI command "setoption name EvalFile value .*.nnue"
  /// The name of the NNUE network is always retrieved from the EvalFile option.
  /// We search the given network in two locations: in the active working directory and
  /// in the engine directory.

  void NNUE::init() {

    string eval_file = string(Options["EvalFile"]);
    if (eval_file.empty())
        eval_file = EvalFileDefaultName;

    vector<string> dirs = { "" , CommandLine::binaryDirectory };

    for (string directory : dirs)
        if (currentEvalFileName != eval_file)
        {
            ifstream stream(directory + eval_file, ios::binary);
            stringstream ss = read_zipped_nnue(directory + eval_file);
            if (load_eval(eval_file, stream) || load_eval(eval_file, ss))
                currentEvalFileName = eval_file;
        }
  }

  /// NNUE::verify() verifies that the last net used was loaded successfully
  void NNUE::verify() {

    string eval_file = string(Options["EvalFile"]);
    if (eval_file.empty())
        eval_file = EvalFileDefaultName;

    if (currentEvalFileName != eval_file)
    {

        string msg1 = "Network evaluation parameters compatible with the engine must be available.";
        string msg2 = "The network file " + eval_file + " was not loaded successfully.";
        string msg3 = "The UCI option EvalFile might need to specify the full path, including the directory name, to the network file.";
        string msg4 = "The engine will be terminated now.";

        sync_cout << "info string ERROR: " << msg1 << sync_endl;
        sync_cout << "info string ERROR: " << msg2 << sync_endl;
        sync_cout << "info string ERROR: " << msg3 << sync_endl;
        sync_cout << "info string ERROR: " << msg4 << sync_endl;

        exit(EXIT_FAILURE);
    }

    sync_cout << "info string NNUE evaluation using " << eval_file << " enabled" << sync_endl;
  }
}

namespace Trace {

  enum Tracing { NO_TRACE, TRACE };

  double to_cp(Value v) { return double(v) / PawnValueEg; }
}

using namespace Trace;

/// evaluate() is the evaluator for the outer world. It returns a static
/// evaluation of the position from the point of view of the side to move.

Value Eval::evaluate(const Position& pos, int* complexity) {

  int nnueComplexity;
  Value v = NNUE::evaluate(pos, &nnueComplexity);
  // Blend nnue complexity with material complexity
  nnueComplexity = (90 * nnueComplexity + 121 * abs(v - pos.material_diff())) / 256;
  if (complexity) // Return hybrid NNUE complexity to caller
      *complexity = nnueComplexity;

  int scale = 1035 + 126 * pos.material_sum() / 4214;
  Value optimism = pos.this_thread()->optimism[pos.side_to_move()];
  optimism = optimism * (281 + nnueComplexity) / 256;
  v = (v * scale + optimism * (scale - 780)) / 1024;

  // Guarantee evaluation does not hit the mate range
  v = std::clamp(v, VALUE_MATED_IN_MAX_PLY + 1, VALUE_MATE_IN_MAX_PLY - 1);

  return v;
}

/// trace() is like evaluate(), but instead of returning a value, it returns
/// a string (suitable for outputting to stdout) that contains the detailed
/// descriptions and values of each evaluation term. Useful for debugging.
/// Trace scores are from white's point of view

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
  ss << "NNUE evaluation        " << to_cp(v) << " (white side)\n";

  v = evaluate(pos);
  v = pos.side_to_move() == WHITE ? v : -v;
  ss << "Final evaluation       " << to_cp(v) << " (white side) [with scaled NNUE, optimism, ...]\n";

  return ss.str();
}

} // namespace Stockfish
