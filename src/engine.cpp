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

#include <cassert>
#include <deque>
#include <memory>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

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

constexpr auto StartFEN  = "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w";
constexpr int  MaxHashMB = Is64Bit ? 33554432 : 2048;

Engine::Engine(std::optional<std::string> path) :
    binaryDirectory(path ? CommandLine::get_binary_directory(*path) : ""),
    numaContext(NumaConfig::from_system()),
    states(new std::deque<StateInfo>(1)),
    threads(),
    network(numaContext, NN::Network({EvalFileDefaultName, "None", ""})) {
    pos.set(StartFEN, &states->back());
    capSq = SQ_NONE;

    options["Debug Log File"] << Option("", [](const Option& o) {
        start_logger(o);
        return std::nullopt;
    });

    options["NumaPolicy"] << Option("auto", [this](const Option& o) {
        set_numa_config_from_option(o);
        return numa_config_information_as_string() + "\n" + thread_binding_information_as_string();
    });

    options["Threads"] << Option(1, 1, 1024, [this](const Option&) {
        resize_threads();
        return thread_binding_information_as_string();
    });

    options["Hash"] << Option(16, 1, MaxHashMB, [this](const Option& o) {
        set_tt_size(o);
        return std::nullopt;
    });

    options["Clear Hash"] << Option([this](const Option&) {
        search_clear();
        return std::nullopt;
    });
    options["Ponder"] << Option(false);
    options["MultiPV"] << Option(1, 1, MAX_MOVES);
    options["Move Overhead"] << Option(10, 0, 5000);
    options["nodestime"] << Option(0, 0, 10000);
    options["UCI_ShowWDL"] << Option(false);
    options["EvalFile"] << Option(EvalFileDefaultName, [this](const Option& o) {
        load_network(o);
        return std::nullopt;
    });

    load_network(options["EvalFile"]);
    resize_threads();
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

    tt.clear(threads);
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

void Engine::set_numa_config_from_option(const std::string& o) {
    if (o == "auto" || o == "system")
    {
        numaContext.set_numa_config(NumaConfig::from_system());
    }
    else if (o == "hardware")
    {
        // Don't respect affinity set in the system.
        numaContext.set_numa_config(NumaConfig::from_system(false));
    }
    else if (o == "none")
    {
        numaContext.set_numa_config(NumaConfig{});
    }
    else
    {
        numaContext.set_numa_config(NumaConfig::from_string(o));
    }

    // Force reallocation of threads in case affinities need to change.
    resize_threads();
    threads.ensure_network_replicated();
}

void Engine::resize_threads() {
    threads.wait_for_search_finished();
    threads.set(numaContext.get_numa_config(), {options, threads, tt, network}, updateContext);

    // Reallocate the hash with the new threadpool size
    set_tt_size(options["Hash"]);
    threads.ensure_network_replicated();
}

void Engine::set_tt_size(size_t mb) {
    wait_for_search_finished();
    tt.resize(mb, threads);
}

void Engine::set_ponderhit(bool b) { threads.main_manager()->ponder = b; }

// network related

void Engine::verify_network() const { network->verify(options["EvalFile"]); }

void Engine::load_network(const std::string& file) {
    network.modify_and_replicate(
      [this, &file](NN::Network& network_) { network_.load(binaryDirectory, file); });
    threads.clear();
    threads.ensure_network_replicated();
}

void Engine::save_network(const std::optional<std::string>& file) {
    network.modify_and_replicate([&file](NN::Network& network_) { network_.save(file); });
}

// utility functions

void Engine::trace_eval() const {
    StateListPtr trace_states(new std::deque<StateInfo>(1));
    Position     p;
    p.set(pos.fen(), &trace_states->back());

    verify_network();

    sync_cout << "\n" << Eval::trace(p, *network) << sync_endl;
}

const OptionsMap& Engine::get_options() const { return options; }
OptionsMap&       Engine::get_options() { return options; }

std::string Engine::fen() const { return pos.fen(); }

void Engine::flip() { pos.flip(); }

std::string Engine::visualize() const {
    std::stringstream ss;
    ss << pos;
    return ss.str();
}

std::vector<std::pair<size_t, size_t>> Engine::get_bound_thread_count_by_numa_node() const {
    auto                                   counts = threads.get_bound_thread_count_by_numa_node();
    const NumaConfig&                      cfg    = numaContext.get_numa_config();
    std::vector<std::pair<size_t, size_t>> ratios;
    NumaIndex                              n = 0;
    for (; n < counts.size(); ++n)
        ratios.emplace_back(counts[n], cfg.num_cpus_in_numa_node(n));
    if (!counts.empty())
        for (; n < cfg.num_numa_nodes(); ++n)
            ratios.emplace_back(0, cfg.num_cpus_in_numa_node(n));
    return ratios;
}

std::string Engine::get_numa_config_as_string() const {
    return numaContext.get_numa_config().to_string();
}

std::string Engine::numa_config_information_as_string() const {
    auto cfgStr = get_numa_config_as_string();
    return "Available processors: " + cfgStr;
}

std::string Engine::thread_binding_information_as_string() const {
    auto              boundThreadsByNode = get_bound_thread_count_by_numa_node();
    std::stringstream ss;

    size_t threadsSize = threads.size();
    ss << "Using " << threadsSize << (threadsSize > 1 ? " threads" : " thread");

    if (boundThreadsByNode.empty())
        return ss.str();

    ss << " with NUMA node thread binding: ";

    bool isFirst = true;

    for (auto&& [current, total] : boundThreadsByNode)
    {
        if (!isFirst)
            ss << ":";
        ss << current << "/" << total;
        isFirst = false;
    }

    return ss.str();
}

}
