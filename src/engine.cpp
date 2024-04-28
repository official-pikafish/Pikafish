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

#include "engine.h"

#include <deque>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>
#include <sstream>
#include <cassert>

#include "evaluate.h"
#include "misc.h"
#include "nnue/network.h"
#include "perft.h"
#include "position.h"
#include "search.h"
#include "types.h"
#include "uci.h"
#include "ucioption.h"

namespace Stockfish {

namespace NN = Eval::NNUE;

constexpr auto StartFEN = "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w";

Engine::Engine(std::string path) :
    binaryDirectory(CommandLine::get_binary_directory(path)),
    states(new std::deque<StateInfo>(1)),
    network(NN::Network({EvalFileDefaultName, "None", ""})) {
    pos.set(StartFEN, &states->back());
    capSq = SQ_NONE;
}

std::uint64_t Engine::perft(const std::string& fen, Depth depth) {
    verify_network();

    return Benchmark::perft(fen, depth);
}

void Engine::go(Search::LimitsType& limits) {
    assert(limits.perft == 0);
    verify_network();
    limits.capSq = capSq;

    threads.start_thinking(pos, states, limits);
}
void Engine::stop() { threads.stop = true; }

void Engine::search_clear() {
    wait_for_search_finished();

    tt.clear(options["Threads"]);
    threads.clear();
}

void Engine::set_on_update_no_moves(std::function<void(const Engine::InfoShort&)>&& f) {
    updateContext.onUpdateNoMoves = std::move(f);
}

void Engine::set_on_update_full(std::function<void(const Engine::InfoFull&)>&& f) {
    updateContext.onUpdateFull = std::move(f);
}

void Engine::set_on_iter(std::function<void(const Engine::InfoIter&)>&& f) {
    updateContext.onIter = std::move(f);
}

void Engine::set_on_bestmove(std::function<void(std::string_view, std::string_view)>&& f) {
    updateContext.onBestmove = std::move(f);
}

void Engine::wait_for_search_finished() { threads.main_thread()->wait_for_search_finished(); }

void Engine::set_position(const std::string& fen, const std::vector<std::string>& moves) {
    // Drop the old state and create a new one
    states = StateListPtr(new std::deque<StateInfo>(1));
    pos.set(fen, &states->back());

    capSq = SQ_NONE;
    for (const auto& move : moves)
    {
        auto m = UCIEngine::to_move(pos, move);

        if (m == Move::none())
            break;

        states->emplace_back();
        pos.do_move(m, states->back());

        capSq          = SQ_NONE;
        DirtyPiece& dp = states->back().dirtyPiece;
        if (dp.dirty_num > 1 && dp.to[1] == SQ_NONE)
            capSq = m.to_sq();
    }
}

// modifiers

void Engine::resize_threads() { threads.set({options, threads, tt, network}, updateContext); }

void Engine::set_tt_size(size_t mb) {
    wait_for_search_finished();
    tt.resize(mb, options["Threads"]);
}

void Engine::set_ponderhit(bool b) { threads.main_manager()->ponder = b; }

// network related

void Engine::verify_network() const { network.verify(options["EvalFile"]); }

void Engine::load_network(const std::string& file) { network.load(binaryDirectory, file); }

void Engine::save_network(const std::optional<std::string>& file) { network.save(file); }

// utility functions

void Engine::trace_eval() const {
    StateListPtr trace_states(new std::deque<StateInfo>(1));
    Position     p;
    p.set(pos.fen(), &trace_states->back());

    verify_network();

    sync_cout << "\n" << Eval::trace(p, network) << sync_endl;
}

OptionsMap& Engine::get_options() { return options; }

std::string Engine::fen() const { return pos.fen(); }

void Engine::flip() { pos.flip(); }

std::string Engine::visualize() const {
    std::stringstream ss;
    ss << pos;
    return ss.str();
}

}
