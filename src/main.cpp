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

#include <iostream>
#include <string>

#include "bitboard.h"
#include "misc.h"
#include "position.h"
#include "uci.h"
#include "tune.h"

using namespace Stockfish;

int eval(char* argv0, const char* fen);

int main(int argc, char* argv[]) {
    if (argc >= 2)
    {
        if (strcmp(argv[1], "eval") == 0)
        {
            if (argc < 3)
            {
                std::cerr << "usage: eval FEN\n";
                return 1;
            }

            return eval(argv[0], argv[2]);
        }
    }
    std::cout << engine_info() << std::endl;

    Bitboards::init();
    Position::init();

    UCIEngine uci(argc, argv);

    Tune::init(uci.engine_options());

    uci.loop();

    return 0;
}


int eval(char* argv0, const char* fen) {
    Bitboards::init();
    Position::init();

    UCIEngine uci(1, &argv0);

    Tune::init(uci.engine_options());

    std::istringstream is(fen);

    // uci.engine.search_clear();
    uci.position(is);


    std::vector<std::string> moves;
    uci.engine.set_position(fen, moves);
    uci.engine.trace_eval();

    return 0;
}