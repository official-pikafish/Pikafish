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
    "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w",

    // Middle game
    "r1ba1a3/4kn3/2n1b4/pNp1p1p1p/4c4/6P2/P1P2R2P/1CcC5/9/2BAKAB2 w",
    "1cbak4/9/n2a5/2p1p3p/5cp2/2n2N3/6PCP/3AB4/2C6/3A1K1N1 w",
    "5a3/3k5/3aR4/9/5r3/5n3/9/3A1A3/5K3/2BC2B2 w",
    "2bak4/9/3a5/p2Np3p/3n1P3/3pc3P/P4r1c1/B2CC2R1/4A4/3AK1B2 b",
    "1r1akabr1/1c7/2n1b1n2/p1p1p3p/6p2/PN3R3/1cP1P1P1P/2C1C1N2/1R7/2BAKAB2 b",
    "2b1ka2r/3na2c1/4b3n/8R/8C/4C1P2/P1P1P3P/4B1N2/1r2A4/2BAK4 w",
    "2bckab2/4a4/5n3/CR3N2p/5r3/P3P1B2/9/2n1B4/4A4/3AK1C2 w",
    "2b1kab1C/1N2a4/n3ccn2/p5r1p/4p4/P1P2RN2/2r1P3P/C3B4/4A4/2BAK2R1 w",
    "2bakab2/9/2n1c1R1c/3r4p/4N4/r8/6P1P/6C1C/4A4/1RBAK1B2 w",
    "2bak1b1r/4a4/2n4cn/p6C1/4pN3/P2N4R/4P1P1P/3CB4/4A2r1/c1BAKR3 w",
    "1r2kabr1/4a4/2C1b2c1/p3p3p/1c3n3/2p3R2/P3P3P/N3C1N2/7R1/2BAKAB2 b",
    "3ak1b2/4a4/2n1b1R2/p1N1pc2p/7r1/2PN1r3/P3P3P/3RB4/4A4/1C2KAB1c w",
    "2baka1r1/9/c5n1c/p3p1CCp/2p3p2/4P4/P6RP/2r1B1N2/4A4/1RB1KA3 w",
    "3akabr1/9/4c4/p1pRn2Cp/4rcp2/2P1p4/P3P1P1P/3CB1N2/9/3AKABR1 w",
    "3akab2/3r5/8n/8p/2P1C1b2/8P/cR2N2r1/2n1B1N2/4A4/2B1KR3 w",
    "2bak4/4a1R2/2n1ccn1b/p3p1C1p/9/2p3P2/P1r1P3P/2N1BCN2/4A4/2BAK4 w",
    "C3kab2/4a4/2Rnb3n/8p/6p2/1p2c3r/P5P2/4B3N/3CA4/2BAK4 w",
    "4kabr1/4a4/2n1b3n/p1C1p3p/6p2/PNP6/4P1P2/1C2B4/4A4/1R2KAB1c w",

    // Positions with complicated checks and evasions
    "CRN1k1b2/3ca4/4ba3/9/2nr5/9/9/4B4/4A4/4KA3 w",
    "R1N1k1b2/9/3aba3/9/2nr5/2B6/9/4B4/4A4/4KA3 w",
    "C1nNk4/9/9/9/9/9/n1pp5/B3C4/9/3A1K3 w",
    "4ka3/4a4/9/9/4N4/p8/9/4C3c/7n1/2BK5 w",
    "2b1ka3/9/b3N4/4n4/9/9/9/4C4/2p6/2BK5 w",
    "1C2ka3/9/C1Nab1n2/p3p3p/6p2/9/P3P3P/3AB4/3p2c2/c1BAK4 w",
    "CnN1k1b2/c3a4/4ba3/9/2nr5/9/9/4C4/4A4/4KA3 w"
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
