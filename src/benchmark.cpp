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

#include <fstream>
#include <iostream>
#include <istream>
#include <vector>

#include "position.h"

using namespace std;

namespace {

// Positions from https://www.chessprogramming.org/Chinese_Chess_Perft_Results
const vector<string> Defaults = {

    // Initial Position
    "xxxxkxxxx/9/1x5x1/x1x1x1x1x/9/9/X1X1X1X1X/1X5X1/9/XXXXKXXXX A2B2N2R2C2P5a2b2n2r2c2p5 w",

    // Middle game
    "C2k1xx1x/9/p1n3p2/x1x1x1x2/8r/4P4/2X3X1X/7X1/1c7/XXXXKXXXX A2B2N2R2C1P3a2b1n1c1p2 w",
    "xxxx1Rxxx/4k2C1/3ap4/xAx1p4/6p1P/9/2X1X1X2/9/5K3/XXX2XXXX A1B2N2C1P4a1b1n2r2c1p2 w",
    "1xxxkxxxx/1R7/8r/2x3xBx/4p4/2P6/c3X1c1X/N5N2/9/rXXXKXX1X A2R1C2P3a2b2n2p4 w",
    "xxxx1kx1x/1p7/7n1/x1x6/9/P1C1p1n2/1A6X/1X7/4P2c1/X1XXK1XXX B2N1R2C1P2a2r2c1p3 b",
    "x2xk2x1/4prN2/2c1p3a/x1x1x1x1B/1P2n4/8P/X1X6/2C6/9/XXXXKXXC1 A2B1N1R2P2a1b2n1r1c1p1 w",
    "2x1kx3/4pb3/3bc3p/4C4/4p1p1p/4Pr2A/Xa2c1X1X/6P2/9/1B1XKXX2 A1N1R1P3n2 b",
    "3k5/5r3/C8/9/9/1B7/9/7A1/7N1/4K4 w - - 0 1",
    "5x3/4k3p/4c1rc1/4R4/P3P4/9/6B2/9/6r2/X1X1K1B1X C1P2a1n1p1 w - - 0 1",
    "x3kxxr1/3n5/3Cb4/xR5a1/2p6/P2N2P1P/2c3B2/9/A4Ab2/4K2XX B1P1r1p3 b - - 0 1",
    "4k1x2/1n2p2a1/4b2P1/9/2p3b2/P3B1r2/6B2/c3N4/A3CA3/4K3X P1p1 b - - 0 1",
};

} // namespace

namespace Stockfish {

/// setup_bench() builds a list of UCI commands to be run by bench. There
/// are five parameters: TT size in MB, number of search threads that
/// should be used, the limit value spent for each position, a file name
/// where to look for positions in FEN format, the type of the limit:
/// depth, perft, nodes and movetime (in millisecs), and evaluation type
/// mixed (default), classical, NNUE.
///
/// bench -> search default positions up to depth 13
/// bench 64 1 15 -> search default positions up to depth 15 (TT = 64MB)
/// bench 64 4 5000 current movetime -> search current position with 4 threads for 5 sec
/// bench 64 1 100000 default nodes -> search default positions for 100K nodes each
/// bench 16 1 5 default perft -> run a perft 5 on default positions

vector<string> setup_bench(const Position& current, istream& is) {

  vector<string> fens, list;
  string go, token;

  // Assign default values to missing arguments
  string ttSize    = (is >> token) ? token : "16";
  string threads   = (is >> token) ? token : "1";
  string limit     = (is >> token) ? token : "13";
  string fenFile   = (is >> token) ? token : "default";
  string limitType = (is >> token) ? token : "depth";

  go = limitType == "eval" ? "eval" : "go " + limitType + " " + limit;

  if (fenFile == "default")
      fens = Defaults;

  else if (fenFile == "current")
      fens.push_back(current.fen());

  else
  {
      string fen;
      ifstream file(fenFile);

      if (!file.is_open())
      {
          cerr << "Unable to open file " << fenFile << endl;
          exit(EXIT_FAILURE);
      }

      while (getline(file, fen))
          if (!fen.empty())
              fens.push_back(fen);

      file.close();
  }

  list.emplace_back("setoption name Threads value " + threads);
  list.emplace_back("setoption name Hash value " + ttSize);
  list.emplace_back("ucinewgame");

  size_t posCounter = 0;

  for (const string& fen : fens)
      if (fen.find("setoption") != string::npos)
          list.emplace_back(fen);
      else
      {
          list.emplace_back("position fen " + fen);
          list.emplace_back(go);
          ++posCounter;
      }

  return list;
}

} // namespace Stockfish
