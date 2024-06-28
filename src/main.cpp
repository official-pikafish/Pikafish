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

static int            old_psqt[311040];
static bool           first = true;
static int            psqt[720];
std::function<void()> update_hook;
static void           update() { update_hook(); }
TUNE(SetRange(-100, 100), psqt, update);

int main(int argc, char* argv[]) {

    std::cout << engine_info() << std::endl;

    Bitboards::init();
    Position::init();

    UCIEngine uci(argc, argv);

    update_hook = [&] {
        auto&       evalFile     = uci.engine.network->evalFile;
        std::string evalfilePath = uci.engine.options["EvalFile"];
        if (evalfilePath.empty())
            evalfilePath = evalFile.defaultName;
        if (evalFile.current != evalfilePath)
            return;
        auto& ft = uci.engine.network->featureTransformer;
        if (first)
        {
            memcpy(old_psqt, ft->psqtWeights, sizeof(old_psqt));
            first = false;
        }
        for (int i = 0; i < 1 * 3 * 3; ++i)
            for (int j = 0; j < 720; ++j)
                for (int k = 0; k < 8; ++k)
                    ft->psqtWeights[i * 720 * 8 + j * 8 + k] =
                      old_psqt[i * 720 * 8 + j * 8 + k] + psqt[j] * 16;
    };

    Tune::init(uci.engine_options());

    uci.loop();

    return 0;
}
