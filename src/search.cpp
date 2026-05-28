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

#include "search.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <tuple>
#include <utility>

#include "evaluate.h"
#include "history.h"
#include "misc.h"
#include "movegen.h"
#include "movepick.h"
#include "nnue/network.h"
#include "nnue/nnue_accumulator.h"
#include "position.h"
#include "thread.h"
#include "timeman.h"
#include "tt.h"
#include "types.h"
#include "uci.h"
#include "ucioption.h"

namespace Stockfish {
int search_80_0 = 8982, search_83_0 = 71856, search_85_0 = 4547, search_85_1 = 3804, search_85_2 = 8213, search_101_0 = 125, search_105_0 = 145, search_113_0 = 131, search_114_0 = 63, search_136_0 = 10, search_138_0 = 20, search_298_0 = 99, search_302_0 = 5, search_302_1 = 768, search_335_0 = 10, search_335_1 = 39605, search_341_0 = 92, search_341_1 = 95, search_398_0 = 44, search_500_0 = 1693, search_500_1 = 273, search_501_0 = 8, search_503_0 = 610, search_503_1 = 1860, search_507_0 = 80, search_507_1 = 170, search_507_2 = 67, search_507_3 = 144, search_510_0 = 210, search_510_1 = 2480, search_512_0 = 960, search_512_1 = 163, search_515_0 = 78000, search_515_1 = 94000, search_515_2 = 960, search_515_3 = 74, search_534_0 = 26, search_590_0 = 5, search_591_0 = 607, search_594_0 = 6, search_595_0 = 1247, search_601_0 = 7, search_607_0 = 436, search_610_0 = 1740, search_773_0 = 193, search_780_0 = 5, search_788_0 = 108, search_788_1 = 60, search_788_2 = 1433, search_791_0 = 3, search_792_0 = 2218, search_799_0 = 7, search_820_0 = 5, search_831_0 = 110, search_831_1 = 187, search_831_2 = 34, search_832_0 = 13, search_834_0 = 12, search_840_0 = 1370, search_840_1 = 244, search_845_0 = 15, search_848_0 = 40, search_848_1 = 129, search_849_0 = 33, search_852_0 = 2512, search_852_1 = 340, search_853_0 = 132109, search_856_0 = 716, search_856_1 = 308, search_860_0 = 8, search_860_1 = 50, search_860_2 = 187, search_866_0 = 8, search_876_0 = 15, search_899_0 = 6, search_905_0 = 251, search_905_1 = 66, search_953_0 = 470, search_1012_0 = 931, search_1023_0 = 1005, search_1031_0 = 19, search_1033_0 = 322, search_1033_1 = 336, search_1034_0 = 229, search_1041_0 = 256, search_1041_1 = 34, search_1053_0 = 2995, search_1056_0 = 73, search_1061_0 = 47, search_1061_1 = 272, search_1061_2 = 129, search_1062_0 = 112, search_1067_0 = 10, search_1078_0 = 35, search_1093_0 = 5, search_1097_0 = 44, search_1097_1 = 72, search_1097_2 = 69, search_1106_0 = 265845, search_1107_0 = 4, search_1107_1 = 234, search_1107_2 = 172, search_1108_0 = 1085, search_1108_1 = 133615, search_1108_2 = 43, search_1109_0 = 106, search_1109_1 = 299, search_1109_2 = 263, search_1109_3 = 93, search_1110_0 = 60, search_1126_0 = 397, search_1126_1 = 103, search_1157_0 = 2363, search_1157_1 = 963, search_1157_2 = 1121, search_1158_0 = 1137, search_1158_1 = 922, search_1160_0 = 855, search_1162_0 = 30558, search_1166_0 = 3251, search_1166_1 = 1048, search_1170_0 = 1571, search_1174_0 = 256, search_1174_1 = 1024, search_1174_2 = 1024, search_1178_0 = 10, search_1178_1 = 2730, search_1178_2 = 150, search_1181_0 = 953, search_1189_0 = 946, search_1193_0 = 256, search_1193_1 = 256, search_1193_2 = 256, search_1215_0 = 60, search_1216_0 = 9, search_1224_0 = 1528, search_1233_0 = 979, search_1237_0 = 3135, search_1237_1 = 4840, search_1345_0 = 11, search_1385_0 = 796, search_1385_1 = 855, search_1391_0 = 231, search_1392_0 = 73, search_1393_0 = 62, search_1394_0 = 152, search_1394_1 = 13, search_1395_0 = 76, search_1395_1 = 166, search_1396_0 = 163, search_1396_1 = 109, search_1401_0 = 148, search_1401_1 = 86, search_1401_2 = 2188, search_1404_0 = 192, search_1406_0 = 216, search_1409_0 = 244, search_1417_0 = 983, search_1444_0 = 12, search_1444_1 = 17, search_1446_0 = 1069, search_1570_0 = 467, search_1570_1 = 557, search_1581_0 = 220, search_1641_0 = 106, search_1688_0 = 481, search_1688_1 = 543, search_1703_0 = 1138, search_1703_1 = 166, search_1703_2 = 1934, search_1772_0 = 162, search_1772_1 = 87, search_1772_2 = 1602, search_1772_3 = 336, search_1773_0 = 870, search_1773_1 = 148, search_1773_2 = 2000, search_1777_0 = 899, search_1779_0 = 1100, search_1783_0 = 950, search_1791_0 = 1455, search_1797_0 = 617, search_1804_0 = 1440, search_1813_0 = 1076, search_1813_1 = 639, search_1813_2 = 293, search_1813_3 = 523, search_1813_4 = 129, search_1813_5 = 445, search_1816_0 = 96, search_1816_1 = 100, search_1816_2 = 100, search_1816_3 = 100, search_1816_4 = 115, search_1816_5 = 118, search_1816_6 = 129, search_1832_0 = 83, search_1846_0 = 693, search_1848_0 = 972, search_1851_0 = 7, search_1851_1 = 913, search_1851_2 = 553;
TUNE(search_80_0, search_83_0, search_85_0, search_85_1, search_85_2, search_101_0, search_105_0, search_113_0, search_114_0, search_136_0, search_138_0, search_298_0, search_302_0, search_302_1, search_335_0, search_335_1, search_341_0, search_341_1, search_398_0, search_500_0, search_500_1, search_501_0, search_503_0, search_503_1, search_507_0, search_507_1, search_507_2, search_507_3, search_510_0, search_510_1, search_512_0, search_512_1, search_515_0, search_515_1, search_515_2, search_515_3, search_534_0, search_590_0, search_591_0, search_594_0, search_595_0, search_601_0, search_607_0, search_610_0, search_773_0, search_780_0, search_788_0, search_788_1, search_788_2, search_791_0, search_792_0, search_799_0, search_820_0, search_831_0, search_831_1, search_831_2, search_832_0, search_834_0, search_840_0, search_840_1, search_845_0, search_848_0, search_848_1, search_849_0, search_852_0, search_852_1, search_853_0, search_856_0, search_856_1, search_860_0, search_860_1, search_860_2, search_866_0, search_876_0, search_899_0, search_905_0, search_905_1, search_953_0, search_1012_0, search_1023_0, search_1031_0, search_1033_0, search_1033_1, search_1034_0, search_1041_0, search_1041_1, search_1053_0, search_1056_0, search_1061_0, search_1061_1, search_1061_2, search_1062_0, search_1067_0, search_1078_0, search_1093_0, search_1097_0, search_1097_1, search_1097_2, search_1106_0, search_1107_0, search_1107_1, search_1107_2, search_1108_0, search_1108_1, search_1108_2, search_1109_0, search_1109_1, search_1109_2, search_1109_3, search_1110_0, search_1126_0, search_1126_1, search_1157_0, search_1157_1, search_1157_2, search_1158_0, search_1158_1, search_1160_0, search_1162_0, search_1166_0, search_1166_1, search_1170_0, search_1174_0, search_1174_1, search_1174_2, search_1178_0, search_1178_1, search_1178_2, search_1181_0, search_1189_0, search_1193_0, search_1193_1, search_1193_2, search_1215_0, search_1216_0, search_1224_0, search_1233_0, search_1237_0, search_1237_1, search_1345_0, search_1385_0, search_1385_1, search_1391_0, search_1392_0, search_1393_0, search_1394_0, search_1394_1, search_1395_0, search_1395_1, search_1396_0, search_1396_1, search_1401_0, search_1401_1, search_1401_2, search_1404_0, search_1406_0, search_1409_0, search_1417_0, search_1444_0, search_1444_1, search_1446_0, search_1570_0, search_1570_1, search_1581_0, search_1641_0, search_1688_0, search_1688_1, search_1703_0, search_1703_1, search_1703_2, search_1772_0, search_1772_1, search_1772_2, search_1772_3, search_1773_0, search_1773_1, search_1773_2, search_1777_0, search_1779_0, search_1783_0, search_1791_0, search_1797_0, search_1804_0, search_1813_0, search_1813_1, search_1813_2, search_1813_3, search_1813_4, search_1813_5, search_1816_0, search_1816_1, search_1816_2, search_1816_3, search_1816_4, search_1816_5, search_1816_6, search_1832_0, search_1846_0, search_1848_0, search_1851_0, search_1851_1, search_1851_2);

static int lmrDivisor[16] = {3307, 2930, 2874, 2818, 3215, 3225, 3224, 2782,
                                                   2858, 2919, 3088, 3275, 3180, 2868, 3006, 3599};
TUNE(lmrDivisor);

using namespace Search;

namespace {

constexpr uint64_t NODES_LIMIT_OUTPUT = 10'000'000;

constexpr int SEARCHEDLIST_CAPACITY = 32;
using SearchedList                  = ValueList<Move, SEARCHEDLIST_CAPACITY>;

// (*Scalers):
// The values with Scaler asterisks have proven non-linear scaling.
// They are optimized to time controls of 180 + 1.8 and longer,
// so changing them or adding conditions that are similar requires
// tests at these types of time controls.

// (*Scaler) All tuned parameters at time controls shorter than
// optimized for require verifications at longer time controls

int correction_value(const Worker& w, const Position& pos, const Stack* const ss) {
    const Color us     = pos.side_to_move();
    const auto  m      = (ss - 1)->currentMove;
    const auto& shared = w.sharedHistory;
    const int   pcv    = shared.pawn_correction_entry(pos)[us].pawn;
    const int   micv   = shared.minor_piece_correction_entry(pos)[us].minor;
    const int   wnpcv  = shared.nonpawn_correction_entry<WHITE>(pos)[us].nonPawnWhite;
    const int   bnpcv  = shared.nonpawn_correction_entry<BLACK>(pos)[us].nonPawnBlack;
    const int   cntcv =
      m.is_ok()
        ? (search_80_0)
            * ((*(ss - 2)->continuationCorrectionHistory)[pos.piece_on(m.to_sq())][m.to_sq()]
               + (*(ss - 4)->continuationCorrectionHistory)[pos.piece_on(m.to_sq())][m.to_sq()])
        : (search_83_0);

    return (search_85_0) * pcv + (search_85_1) * micv + (search_85_2) * (wnpcv + bnpcv) + cntcv;
}

// Add correctionHistory value to raw staticEval and guarantee evaluation
// does not hit the tablebase range.
Value to_corrected_static_eval(const Value v, const int cv) {
    return std::clamp(v + cv / 131072, VALUE_MATED_IN_MAX_PLY + 1, VALUE_MATE_IN_MAX_PLY - 1);
}

void update_correction_history(const Position& pos,
                               Stack* const    ss,
                               Search::Worker& workerThread,
                               const int       bonus) {
    const Move  m  = (ss - 1)->currentMove;
    const Color us = pos.side_to_move();

    int nonPawnWeight = (search_101_0);
    auto&         shared        = workerThread.sharedHistory;

    shared.pawn_correction_entry(pos)[us].pawn << bonus;
    shared.minor_piece_correction_entry(pos)[us].minor << bonus * (search_105_0) / 128;
    shared.nonpawn_correction_entry<WHITE>(pos)[us].nonPawnWhite << bonus * nonPawnWeight / 128;
    shared.nonpawn_correction_entry<BLACK>(pos)[us].nonPawnBlack << bonus * nonPawnWeight / 128;

    if (m.is_ok())
    {
        const Square to = m.to_sq();
        const Piece  pc = pos.piece_on(to);
        (*(ss - 2)->continuationCorrectionHistory)[pc][to] << bonus * (search_113_0) / 128;
        (*(ss - 4)->continuationCorrectionHistory)[pc][to] << bonus * (search_114_0) / 128;
    }
}

// Add a small random component to draw evaluations to avoid 3-fold blindness
Value value_draw(size_t nodes) { return VALUE_DRAW - 1 + Value(nodes & 0x2); }
Value value_to_tt(Value v, int ply);
Value value_from_tt(Value v, int ply, int r60c);
void  update_continuation_histories(Stack* ss, Piece pc, Square to, int bonus);
void  update_quiet_histories(
  const Position& pos, Stack* ss, Search::Worker& workerThread, Move move, int bonus);
void update_all_stats(const Position& pos,
                      Stack*          ss,
                      Search::Worker& workerThread,
                      Move            bestMove,
                      Square          prevSq,
                      SearchedList&   quietsSearched,
                      SearchedList&   capturesSearched,
                      Depth           depth,
                      Move            ttMove);

bool is_shuffling(Move move, Stack* const ss, const Position& pos) {
    if (pos.capture(move) || pos.rule60_count() < (search_136_0))
        return false;
    if (pos.state()->pliesFromNull < 6 || ss->ply < (search_138_0))
        return false;
    return move.from_sq() == (ss - 2)->currentMove.to_sq()
        && (ss - 2)->currentMove.from_sq() == (ss - 4)->currentMove.to_sq();
}

}  // namespace

Search::Worker::Worker(SharedState&                    sharedState,
                       std::unique_ptr<ISearchManager> sm,
                       size_t                          threadId,
                       size_t                          numaThreadId,
                       size_t                          numaTotalThreads,
                       NumaReplicatedAccessToken       token) :
    // Unpack the SharedState struct into member variables
    sharedHistory(sharedState.sharedHistories.at(token.get_numa_index())),
    threadIdx(threadId),
    numaThreadIdx(numaThreadId),
    numaTotal(numaTotalThreads),
    numaAccessToken(token),
    manager(std::move(sm)),
    options(sharedState.options),
    threads(sharedState.threads),
    tt(sharedState.tt),
    network(sharedState.network),
    refreshTable(network[token]) {
    clear();
}

void Search::Worker::ensure_network_replicated() {
    // Access once to force lazy initialization.
    // We do this because we want to avoid initialization during search.
    (void) (network[numaAccessToken]);
}

void Search::Worker::start_searching() {

    accumulatorStack.reset();
    lastIterationPV.clear();

    // Non-main threads go directly to iterative_deepening()
    if (!is_mainthread())
    {
        iterative_deepening();
        return;
    }

    main_manager()->tm.init(limits, rootPos.side_to_move(), rootPos.game_ply(), options,
                            main_manager()->originalTimeAdjust);
    tt.new_search();

    if (rootMoves.empty())
    {
        main_manager()->updates.onUpdateNoMoves({0, {-VALUE_MATE, rootPos}});
        main_manager()->updates.onBestmove(UCIEngine::move(Move::none()), "");
        return;
    }

    // Main thread starts non-main threads, and begins own search.
    threads.start_searching();
    bool uciPvSent = iterative_deepening();

    // When we reach the maximum depth, we can arrive here without a raise of
    // threads.stop. However, if we are pondering or in an infinite search,
    // the UCI protocol states that we shouldn't print the best move before the
    // GUI sends a "stop" or "ponderhit" command. We therefore simply wait here
    // until the GUI sends one of those commands.
    while (!threads.stop && (main_manager()->ponder || limits.infinite))
    {}  // Busy wait for a stop or a ponder reset

    // Stop the threads if not already stopped (also raise the stop if
    // "ponderhit" just reset threads.ponder)
    threads.stop = true;

    // Wait until all threads have finished
    threads.wait_for_search_finished();

    // When playing in 'nodes as time' mode, subtract the searched nodes from
    // the available ones before exiting.
    if (limits.npmsec)
        main_manager()->tm.advance_nodes_time(threads.nodes_searched()
                                              - limits.inc[rootPos.side_to_move()]);

    Worker* bestThread = this;

    if (!limits.depth)
        bestThread = threads.get_best_thread()->worker.get();

    main_manager()->bestPreviousScore        = bestThread->rootMoves[0].score;
    main_manager()->bestPreviousAverageScore = bestThread->rootMoves[0].averageScore;

    if (bestThread->rootMoves[0].pv.size() == 1
        && bestThread->rootMoves[0].extract_ponder_from_tt(tt, rootPos))
        uciPvSent = false;

    // Send PV info if it has changed since last output in iterative_deepening().
    if (!uciPvSent || bestThread != this)
        main_manager()->pv(*bestThread, threads, tt, bestThread->rootDepth);

    // In rare cases, pv() may change the ponder move through syzygy_extend_pv().
    std::string ponder;
    if (bestThread->rootMoves[0].pv.size() > 1)
        ponder = UCIEngine::move(bestThread->rootMoves[0].pv[1]);

    auto bestmove = UCIEngine::move(bestThread->rootMoves[0].pv[0]);
    main_manager()->updates.onBestmove(bestmove, ponder);
}

// Main iterative deepening loop. It calls search()
// repeatedly with increasing depth until the allocated thinking time has been
// consumed, the user stops the search, or the maximum search depth is reached.
bool Search::Worker::iterative_deepening() {

    SearchManager* mainThread = (is_mainthread() ? main_manager() : nullptr);

    PVMoves pv;

    Depth lastBestMoveDepth = 0;

    Value  alpha, beta;
    Value  bestValue          = -VALUE_INFINITE;
    Value  lastIterationScore = -VALUE_INFINITE;
    Color  us                 = rootPos.side_to_move();
    double timeReduction = 1, totBestMoveChanges = 0;
    int    delta, iterIdx                        = 0;

    // Allocate stack with extra size to allow access from (ss - 7) to (ss + 2):
    // (ss - 7) is needed for update_continuation_histories(ss - 1) which accesses (ss - 6),
    // (ss + 2) is needed for initialization of cutOffCnt.
    Stack  stack[MAX_PLY + 10] = {};
    Stack* ss                  = stack + 7;

    for (int i = 7; i > 0; --i)
    {
        (ss - i)->continuationHistory =
          &continuationHistory[0][0][NO_PIECE][0];  // Use as a sentinel
        (ss - i)->continuationCorrectionHistory = &continuationCorrectionHistory[NO_PIECE][0];
        (ss - i)->staticEval                    = VALUE_NONE;
    }

    for (int i = 0; i <= MAX_PLY + 2; ++i)
        (ss + i)->ply = i;

    ss->pv = &pv;

    if (mainThread)
    {
        if (mainThread->bestPreviousScore == VALUE_INFINITE)
            mainThread->iterValue.fill(VALUE_ZERO);
        else
            mainThread->iterValue.fill(mainThread->bestPreviousScore);
    }

    size_t multiPV = size_t(options["MultiPV"]);

    multiPV = std::min(multiPV, rootMoves.size());

    int  searchAgainCounter = 0;
    bool uciPvSent          = false;

    lowPlyHistory.fill((search_298_0));

    for (Color c : {WHITE, BLACK})
        for (int i = 0; i < UINT_16_HISTORY_SIZE; i++)
            mainHistory[c][i] = (mainHistory[c][i] + (search_302_0)) * (search_302_1) / 1024;

    // Iterative deepening loop until requested to stop or the target depth is reached
    while (rootDepth + 1 < MAX_PLY && !threads.stop
           && !(limits.depth && mainThread && rootDepth >= limits.depth))
    {
        rootDepth++;

        // Age out PV variability metric and signal the start of a new iteration.
        if (mainThread)
        {
            totBestMoveChanges /= 2;
            uciPvSent = false;
        }

        // Save the last iteration's scores before the first PV line is searched and
        // all the move scores except the (new) PV are set to -VALUE_INFINITE.
        for (RootMove& rm : rootMoves)
            rm.previousScore = rm.score;

        size_t pvFirst = 0;
        pvLast         = rootMoves.size();

        if (!threads.increaseDepth)
            searchAgainCounter++;

        // MultiPV loop. We perform a full root search for each PV line
        for (pvIdx = 0; pvIdx < multiPV; ++pvIdx)
        {
            // Reset UCI info selDepth for each depth and each PV line
            selDepth = 0;

            // Reset aspiration window starting size
            delta     = (search_335_0) + threadIdx % 8 + std::abs(rootMoves[pvIdx].meanSquaredScore) / (search_335_1);
            Value avg = rootMoves[pvIdx].averageScore;
            alpha     = std::max(avg - delta, -VALUE_INFINITE);
            beta      = std::min(avg + delta, VALUE_INFINITE);

            // Adjust optimism based on root move's averageScore
            optimism[us]  = (search_341_0) * avg / (std::abs(avg) + (search_341_1));
            optimism[~us] = -optimism[us];

            // Start with a small aspiration window and, in the case of a fail
            // high/low, re-search with a bigger window until we don't fail
            // high/low anymore.
            int failedHighCnt = 0;
            while (true)
            {
                // Adjust the effective depth searched, but ensure at least one
                // effective increment for every four searchAgain steps (see issue #2717).
                Depth adjustedDepth =
                  std::max(1, rootDepth - failedHighCnt - 3 * (searchAgainCounter + 1) / 4);
                rootDelta = beta - alpha;
                bestValue = search<Root>(rootPos, ss, alpha, beta, adjustedDepth, false);

                // Bring the best move to the front. It is critical that sorting
                // is done with a stable algorithm because all the values but the
                // first and eventually the new best one is set to -VALUE_INFINITE
                // and we want to keep the same order for all the moves except the
                // new PV that goes to the front. Note that in the case of MultiPV
                // search the already searched PV lines are preserved.
                std::stable_sort(rootMoves.begin() + pvIdx, rootMoves.begin() + pvLast);

                // If search has been stopped, we break immediately. Sorting is
                // safe because RootMoves is still valid, although it refers to
                // the previous iteration.
                if (threads.stop)
                    break;

                // When failing high/low give some update before a re-search. To avoid
                // excessive output that could hang GUIs, only start at nodes > 10M
                // (rather than depth N, which can be reached quickly)
                if (mainThread && multiPV == 1 && (bestValue <= alpha || bestValue >= beta)
                    && nodes > NODES_LIMIT_OUTPUT)
                    main_manager()->pv(*this, threads, tt, rootDepth);

                // In case of failing low/high increase aspiration window and re-search,
                // otherwise exit the loop.
                if (bestValue <= alpha)
                {
                    beta  = alpha;
                    alpha = std::max(bestValue - delta, -VALUE_INFINITE);

                    failedHighCnt = 0;
                    if (mainThread)
                        mainThread->stopOnPonderhit = false;
                }
                else if (bestValue >= beta)
                {
                    alpha = std::max(beta - delta, alpha);
                    beta  = std::min(bestValue + delta, VALUE_INFINITE);
                    ++failedHighCnt;
                }
                else
                    break;

                delta += (search_398_0) * delta / 128;

                assert(alpha >= -VALUE_INFINITE && beta <= VALUE_INFINITE);
            }

            // In multiPV analysis we do not let aborted searches spoil mated-in/
            // TB loss scores from a completed search in an earlier PV line.
            // A mated-in/TB loss from an aborted search for pvIdx > 0 can only become
            // bestmove in the sorting below, if the current bestmove (and hence also
            // the previously searched pvIdx - 1 line) is already a proven loss.
            if (threads.stop && pvIdx && is_loss(rootMoves[pvIdx - 1].score)
                && rootMoves[pvIdx] < rootMoves[pvIdx - 1])
            {
                rootMoves[pvIdx].score = rootMoves[pvIdx].uciScore =
                  (rootMoves[pvIdx].previousScore != -VALUE_INFINITE
                   && rootMoves[pvIdx].previousScore < rootMoves[pvIdx - 1].score)
                    ? rootMoves[pvIdx].previousScore
                    : rootMoves[pvIdx - 1].score;
                rootMoves[pvIdx].previousScore = -VALUE_INFINITE;
                rootMoves[pvIdx].unset_bound_flags();
                rootMoves[pvIdx].pv.resize(1);
            }

            // Sort the PV lines searched so far and update the GUI
            std::stable_sort(rootMoves.begin() + pvFirst, rootMoves.begin() + pvIdx + 1);

            if (mainThread && !threads.stop && (pvIdx + 1 == multiPV || nodes > NODES_LIMIT_OUTPUT))
            {
                main_manager()->pv(*this, threads, tt, rootDepth);
                uciPvSent = (pvIdx + 1 == multiPV);
            }

            if (threads.stop)
                break;
        }

        const bool forgottenMate = lastIterationScore != -VALUE_INFINITE
                                && is_decisive(lastIterationScore)
                                && (std::abs(rootMoves[0].score) < std::abs(lastIterationScore)
                                    || rootMoves[0].score_is_bound());

        if (!threads.stop)
        {
            if (lastIterationPV.empty() || rootMoves[0].pv[0] != lastIterationPV[0])
                lastBestMoveDepth = rootDepth;

            // Do not replace (shorter) mate scores from a previous iteration.
            if (!forgottenMate)
            {
                lastIterationPV    = rootMoves[0].pv;
                lastIterationScore = rootMoves[0].score;
            }
        }

        const bool abortedLossSearch =
          threads.stop && !pvIdx && rootMoves[0].score != -VALUE_INFINITE
          && is_loss(rootMoves[0].score) && !rootMoves[0].score_is_bound();

        // An exact mated-in/TB-loss score from an aborted search cannot be trusted: the
        // loss could be delayed or refuted upon exploring the remaining root-moves.
        // Thus here we roll back to the score from the previous iteration.
        // We do the same if a search has failed to recover a mate score that was found
        // in a previous iteration.
        if (abortedLossSearch || (rootMoves[0].score != -VALUE_INFINITE && forgottenMate))
        {
            if (!lastIterationPV.empty())
            {
                Utility::move_to_front(rootMoves, [&lastPV = std::as_const(lastIterationPV)](
                                                    const auto& rm) { return rm == lastPV[0]; });
                rootMoves[0].pv    = lastIterationPV;
                rootMoves[0].score = rootMoves[0].uciScore = lastIterationScore;
                rootMoves[0].unset_bound_flags();

                if (mainThread)
                    uciPvSent = false;
            }
            // For an aborted d1 search we label the loss score as a lower bound.
            else if (abortedLossSearch)
                rootMoves[0].scoreLowerbound = true;
        }

        // Have we found a "mate in x" after a completed iteration?
        if (limits.mate && !threads.stop && is_decisive(rootMoves[0].score)
            && VALUE_MATE - std::abs(rootMoves[0].score) <= 2 * limits.mate)
            threads.stop = true;

        if (!mainThread)
            continue;

        // Use part of the gained time from a previous stable move for the current move
        for (auto&& th : threads)
        {
            totBestMoveChanges += th->worker->bestMoveChanges;
            th->worker->bestMoveChanges = 0;
        }

        // Do we have time for the next iteration? Can we stop searching now?
        if (limits.use_time_management() && !threads.stop && !mainThread->stopOnPonderhit)
        {
            uint64_t nodesEffort =
              rootMoves[0].effort * 100000 / std::max(uint64_t(1), uint64_t(nodes));

            double fallingEval = ((search_500_0 / 100.0) + (search_500_1 / 100.0) * (mainThread->bestPreviousAverageScore - bestValue)
                                  + (search_501_0 / 10.0) * (mainThread->iterValue[iterIdx] - bestValue))
                               / 100.0;
            fallingEval        = std::clamp(fallingEval, (search_503_0 / 1000.0), (search_503_1 / 1000.0));

            // If the bestMove is stable over several iterations, reduce time accordingly
            timeReduction =
              std::clamp(interpolate(double(rootDepth - lastBestMoveDepth), double(search_507_0 / 10), double(search_507_1 / 10), (search_507_2 / 100.0), (search_507_3 / 100.0)),
                         0.67, 1.44);

            double reduction = ((search_510_0 / 100.0) + mainThread->previousTimeReduction) / ((search_510_1 / 1000.0) * timeReduction);

            double bestMoveInstability = (search_512_0 / 1000.0) + (search_512_1 / 100.0) * totBestMoveChanges / threads.size();

            double highBestMoveEffort = std::clamp(
              interpolate(int64_t(nodesEffort), int64_t((search_515_0)), int64_t((search_515_1)), (search_515_2 / 1000.0), (search_515_3 / 100.0)), (search_515_3 / 100.0),
              (search_515_2 / 1000.0));

            double totalTime = mainThread->tm.optimum() * fallingEval * reduction
                             * bestMoveInstability * highBestMoveEffort;

            auto elapsedTime = elapsed();

            // Stop the search if we have exceeded the totalTime or maximum
            if (elapsedTime > std::min(totalTime, double(mainThread->tm.maximum())))
            {
                // If we are allowed to ponder do not stop the search now but
                // keep pondering until the GUI sends "ponderhit" or "stop".
                if (mainThread->ponder)
                    mainThread->stopOnPonderhit = true;
                else
                    threads.stop = true;
            }
            else
                threads.increaseDepth = mainThread->ponder || elapsedTime <= totalTime * (search_534_0 / 100.0);
        }

        mainThread->iterValue[iterIdx] = bestValue;
        iterIdx                        = (iterIdx + 1) & 3;
    }

    if (!mainThread)
        return false;

    mainThread->previousTimeReduction = timeReduction;

    return uciPvSent;
}


void Search::Worker::do_move(Position& pos, const Move move, StateInfo& st, Stack* const ss) {
    do_move(pos, move, st, pos.gives_check(move), ss);
}

void Search::Worker::do_move(
  Position& pos, const Move move, StateInfo& st, const bool givesCheck, Stack* const ss) {
    bool capture = pos.capture(move);
    // Preferable over fetch_add to avoid locking instructions
    nodes.store(nodes.load(std::memory_order_relaxed) + 1, std::memory_order_relaxed);

    auto [dirtyPiece, dirtyThreats] = accumulatorStack.push();
    pos.do_move(move, st, givesCheck, dirtyPiece, dirtyThreats, &tt, &sharedHistory);

    if (ss != nullptr)
    {
        ss->currentMove = move;
        ss->continuationHistory =
          &continuationHistory[ss->inCheck][capture][dirtyPiece.pc][move.to_sq()];
        ss->continuationCorrectionHistory =
          &continuationCorrectionHistory[dirtyPiece.pc][move.to_sq()];
    }
}

void Search::Worker::do_null_move(Position& pos, StateInfo& st, Stack* const ss) {
    pos.do_null_move(st);
    ss->currentMove                   = Move::null();
    ss->continuationHistory           = &continuationHistory[0][0][NO_PIECE][0];
    ss->continuationCorrectionHistory = &continuationCorrectionHistory[NO_PIECE][0];
}

void Search::Worker::undo_move(Position& pos, const Move move) {
    pos.undo_move(move);
    accumulatorStack.pop();
}

void Search::Worker::undo_null_move(Position& pos) { pos.undo_null_move(); }


// Reset histories, usually before a new game
void Search::Worker::clear() {
    mainHistory.fill(-(search_590_0));
    captureHistory.fill(-(search_591_0));

    // Each thread is responsible for clearing their part of shared history
    sharedHistory.correctionHistory.clear_range(-(search_594_0), numaThreadIdx, numaTotal);
    sharedHistory.pawnHistory.clear_range(-(search_595_0), numaThreadIdx, numaTotal);

    ttMoveHistory = 0;

    for (auto& to : continuationCorrectionHistory)
        for (auto& h : to)
            h.fill((search_601_0));

    for (bool inCheck : {false, true})
        for (StatsType c : {NoCaptures, Captures})
            for (auto& to : continuationHistory[inCheck][c])
                for (auto& h : to)
                    h.fill(-(search_607_0));

    for (size_t i = 1; i < reductions.size(); ++i)
        reductions[i] = int((search_610_0) / 100.0 * std::log(i));

    refreshTable.clear(network[numaAccessToken]);
}


// Main search function for both PV and non-PV nodes
template<NodeType nodeType>
Value Search::Worker::search(
  Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, bool cutNode) {

    constexpr bool PvNode   = nodeType != NonPV;
    constexpr bool rootNode = nodeType == Root;
    const bool     allNode  = !(PvNode || cutNode);

    // Dive into quiescence search when the depth reaches zero
    if (depth <= 0)
        return qsearch<PvNode ? PV : NonPV>(pos, ss, alpha, beta);

    // Limit the depth if extensions made it too large
    depth = std::min(depth, MAX_PLY - 1);

    assert(-VALUE_INFINITE <= alpha && alpha < beta && beta <= VALUE_INFINITE);
    assert(PvNode || (alpha == beta - 1));
    assert(0 < depth && depth < MAX_PLY);
    assert(!(PvNode && cutNode));

    PVMoves   pv;
    StateInfo st;

    Key   posKey;
    Move  move, excludedMove, bestMove;
    Depth extension, newDepth;
    Value bestValue, value, eval, maxValue, probCutBeta;
    bool  givesCheck, improving, priorCapture, opponentWorsening;
    bool  capture, ttCapture;
    int   priorReduction;
    Piece movedPiece;

    SearchedList capturesSearched;
    SearchedList quietsSearched;

    // Step 1. Initialize node
    ss->inCheck   = bool(pos.checkers());
    priorCapture  = pos.captured_piece();
    Color us      = pos.side_to_move();
    ss->moveCount = 0;
    bestValue     = -VALUE_INFINITE;
    maxValue      = VALUE_INFINITE;

    ss->followPV = rootNode
                || ((ss - 1)->followPV && static_cast<size_t>(ss->ply - 1) < lastIterationPV.size()
                    && (ss - 1)->currentMove == lastIterationPV[ss->ply - 1]);

    // Check for the available remaining time
    if (is_mainthread())
        main_manager()->check_time(*this);

    // Used to send selDepth info to GUI (selDepth counts from 1, ply from 0)
    if (PvNode && selDepth < ss->ply + 1)
        selDepth = ss->ply + 1;

    if (!rootNode)
    {
        // Step 2. Check for aborted search and repetition
        Value result = VALUE_NONE;
        if (pos.rule_judge(result, ss->ply))
            return result == VALUE_DRAW ? value_draw(nodes) : result;
        if (result != VALUE_NONE)
        {
            assert(result != VALUE_DRAW);

            // 2 fold result is mate for us, the only chance for the opponent is to get a draw
            // We can guarantee to get at least a draw score during searching for that line
            if (result > VALUE_DRAW)
                alpha = std::max(alpha, VALUE_DRAW - 1);
            // 2 fold result is mated for us, the only chance for us is to get a draw
            // We can guarantee to get no more than a draw score during searching for that line
            else
                beta = std::min(beta, VALUE_DRAW + 1);
        }

        if (threads.stop.load(std::memory_order_relaxed) || ss->ply >= MAX_PLY)
            return (ss->ply >= MAX_PLY && !ss->inCheck) ? evaluate(pos) : value_draw(nodes);

        // Step 3. Mate distance pruning. Even if we mate at the next move our score
        // would be at best mate_in(ss->ply + 1), but if alpha is already bigger because
        // a shorter mate was found upward in the tree then there is no need to search
        // because we will never beat the current alpha. Same logic but with reversed
        // signs apply also in the opposite condition of being mated instead of giving
        // mate. In this case, return a fail-high score.
        alpha = std::max(mated_in(ss->ply), alpha);
        beta  = std::min(mate_in(ss->ply + 1), beta);
        if (alpha >= beta)
            return alpha;
    }

    assert(0 <= ss->ply && ss->ply < MAX_PLY);

    Square prevSq  = ((ss - 1)->currentMove).is_ok() ? ((ss - 1)->currentMove).to_sq() : SQ_NONE;
    bestMove       = Move::none();
    priorReduction = (ss - 1)->reduction;
    (ss - 1)->reduction = 0;
    ss->statScore       = 0;
    (ss + 2)->cutoffCnt = 0;

    const auto correctionValue = correction_value(*this, pos, ss);

    // Step 4. Transposition table lookup
    excludedMove                   = ss->excludedMove;
    posKey                         = pos.key();
    auto [ttHit, ttData, ttWriter] = tt.probe(posKey);
    // Need further processing of the saved data
    ss->ttHit    = ttHit;
    ttData.move  = rootNode ? rootMoves[pvIdx].pv[0] : ttHit ? ttData.move : Move::none();
    ttData.value = ttHit ? value_from_tt(ttData.value, ss->ply, pos.rule60_count()) : VALUE_NONE;
    ss->ttPv     = excludedMove ? ss->ttPv : PvNode || (ttHit && ttData.is_pv);
    ttCapture    = ttData.move && pos.capture(ttData.move);

    // Step 5. Static evaluation of the position
    Value unadjustedStaticEval = VALUE_NONE;

    // Skip early pruning when in check
    if (ss->inCheck)
        ss->staticEval = eval = (ss - 2)->staticEval;
    else if (excludedMove)
        unadjustedStaticEval = eval = ss->staticEval;
    else if (ss->ttHit)
    {
        // Never assume anything about values stored in TT
        unadjustedStaticEval = ttData.eval;
        if (!is_valid(unadjustedStaticEval))
            unadjustedStaticEval = evaluate(pos);

        ss->staticEval = eval = to_corrected_static_eval(unadjustedStaticEval, correctionValue);

        // ttValue can be used as a better position evaluation
        if (is_valid(ttData.value)
            && (ttData.bound & (ttData.value > eval ? BOUND_LOWER : BOUND_UPPER)))
            eval = ttData.value;
    }
    else
    {
        unadjustedStaticEval = evaluate(pos);
        ss->staticEval = eval = to_corrected_static_eval(unadjustedStaticEval, correctionValue);

        // Static evaluation is saved as it was before adjustment by correction history
        ttWriter.write(posKey, VALUE_NONE, ss->ttPv, BOUND_NONE, DEPTH_UNSEARCHED, Move::none(),
                       unadjustedStaticEval, tt.generation());
    }

    // Set up the improving flag, which is true if current static evaluation is
    // bigger than the previous static evaluation at our turn (if we were in
    // check at our previous move we go back until we weren't in check) and is
    // false otherwise. The improving flag is used in various pruning heuristics.
    // Similarly, opponentWorsening is true if our static evaluation is better
    // for us than at the last ply.
    improving         = ss->staticEval > (ss - 2)->staticEval;
    opponentWorsening = ss->staticEval > -(ss - 1)->staticEval;

    // Hindsight adjustment of reductions based on static evaluation difference.
    if (priorReduction >= 3 && !opponentWorsening)
        depth++;
    if (priorReduction >= 2 && depth >= 2 && ss->staticEval + (ss - 1)->staticEval > (search_773_0))
        depth--;

    // At non-PV nodes we check for an early TT cutoff
    if (!PvNode && !excludedMove && ttData.depth > depth - (ttData.value <= beta)
        && is_valid(ttData.value)  // Can happen when !ttHit or when access race in probe()
        && (ttData.bound & (ttData.value >= beta ? BOUND_LOWER : BOUND_UPPER))
        && (cutNode == (ttData.value >= beta) || depth > (search_780_0)))
    {
        // If ttMove is quiet, update move sorting heuristics on TT hit
        if (ttData.move && ttData.value >= beta)
        {
            // Bonus for a quiet ttMove that fails high
            if (!ttCapture)
                update_quiet_histories(pos, ss, *this, ttData.move,
                                       std::min((search_788_0) * depth - (search_788_1), (search_788_2)));

            // Extra penalty for early quiet moves of the previous ply
            if (prevSq != SQ_NONE && (ss - 1)->moveCount < (search_791_0) && !priorCapture)
                update_continuation_histories(ss - 1, pos.piece_on(prevSq), prevSq, -(search_792_0));
        }

        // Partial workaround for the graph history interaction problem
        // For high rule60 counts don't produce transposition table cutoffs.
        if (pos.rule60_count() < 116)
        {
            if (depth >= (search_799_0) && ttData.move && pos.pseudo_legal(ttData.move) && pos.legal(ttData.move)
                && !is_decisive(ttData.value))
            {
                pos.do_move(ttData.move, st);
                Key nextPosKey                             = pos.key();
                auto [ttHitNext, ttDataNext, ttWriterNext] = tt.probe(nextPosKey);
                pos.undo_move(ttData.move);

                // Check that the ttValue after the tt move would also trigger a cutoff
                if (!is_valid(ttDataNext.value))
                    return ttData.value;

                if ((ttData.value >= beta) == (-ttDataNext.value >= beta))
                    return ttData.value;
            }
            else
                return ttData.value;
        }
    }  // No cutoff, but why? Does the stored inexact value mismatch our aspiration window?
    else if (!PvNode && !excludedMove && ttData.depth > depth - (ttData.value <= beta)
             && is_valid(ttData.value) && ttData.bound != BOUND_EXACT
             && ttData.bound & (ttData.value >= beta ? BOUND_UPPER : BOUND_LOWER) && depth > (search_820_0))
    {  // If a window-bound mismatch is the only reason cutoff failed, penalize the now-useless tte
        ttWriter.penalize(1);
    }

    if (ss->inCheck)
        goto moves_loop;

    // Use static evaluation difference to improve quiet move ordering
    if (((ss - 1)->currentMove).is_ok() && !(ss - 1)->inCheck && !priorCapture)
    {
        int evalDiff = std::clamp(-int((ss - 1)->staticEval + ss->staticEval), -(search_831_0), (search_831_1)) + (search_831_2);
        mainHistory[~us][((ss - 1)->currentMove).raw()] << evalDiff * (search_832_0);
        if (!ttHit && type_of(pos.piece_on(prevSq)) != PAWN)
            sharedHistory.pawn_entry(pos)[pos.piece_on(prevSq)][prevSq] << evalDiff * (search_834_0);
    }

    // Step 6. Razoring
    // If eval is really low, skip search entirely and return the qsearch value.
    // For PvNodes, we must have a guard against mates being returned.
    if (!PvNode && eval < alpha - (search_840_0) - (search_840_1) * depth * depth)
        return qsearch<NonPV>(pos, ss, alpha, beta);

    // Step 7. Futility pruning: child node
    // The depth condition is important for mate finding.
    if (!ss->ttPv && depth < (search_845_0) && eval >= beta && (!ttData.move || ttCapture) && !is_loss(beta)
        && !is_win(eval))
    {
        Value futilityMult = interpolate(std::min(int(depth), 10), 1, 10, (search_848_0), (search_848_1));
        futilityMult -= (search_849_0) * !ss->ttHit;

        Value futilityMargin = futilityMult * depth
                             - ((search_852_0) * improving + (search_852_1) * opponentWorsening) * futilityMult / 1024
                             + std::abs(correctionValue) / (search_853_0);

        if (eval - futilityMargin >= beta)
            return ((search_856_0) * beta + (search_856_1) * eval) / 1024;
    }

    // Step 8. Null move search with verification search
    if (cutNode && ss->staticEval >= beta - (search_860_0) * depth - (search_860_1) * improving + (search_860_2) && !excludedMove
        && pos.major_material(us) && ss->ply >= nmpMinPly && !is_loss(beta))
    {
        assert((ss - 1)->currentMove != Move::null());

        // Null move dynamic reduction based on depth
        Depth R = (search_866_0) + depth / 3;
        do_null_move(pos, st, ss);

        Value nullValue = -search<NonPV>(pos, ss + 1, -beta, -beta + 1, depth - R, false);

        undo_null_move(pos);

        // Do not return unproven mate
        if (nullValue >= beta && !is_win(nullValue))
        {
            if (nmpMinPly || depth < (search_876_0))
                return nullValue;

            assert(!nmpMinPly);  // Recursive verification is not allowed

            // Do verification search at high depths, with null move pruning disabled
            // until ply exceeds nmpMinPly.
            nmpMinPly = ss->ply + 3 * (depth - R) / 4;

            Value v = search<NonPV>(pos, ss, beta - 1, beta, depth - R, false);

            nmpMinPly = 0;

            if (v >= beta)
                return nullValue;
        }
    }

    improving |= ss->staticEval >= beta;

    // Step 9. Internal iterative reductions
    // At sufficient depth, reduce depth for PV/Cut nodes without a TTMove.
    // (*Scaler) Making IIR more aggressive scales poorly.
    if (!ss->followPV && !allNode && depth >= (search_899_0) && !ttData.move && priorReduction <= 3)
        depth--;

    // Step 10. ProbCut
    // If we have a good enough capture and a reduced search
    // returns a value much above beta, we can (almost) safely prune the previous move.
    probCutBeta = beta + (search_905_0) - (search_905_1) * improving;
    if (depth >= 3
        && !is_decisive(beta)
        // If value from transposition table is lower than probCutBeta, don't attempt
        // probCut there
        && !(is_valid(ttData.value) && ttData.value < probCutBeta))
    {
        assert(probCutBeta < VALUE_INFINITE && probCutBeta > beta);

        MovePicker mp(pos, ttData.move, probCutBeta - ss->staticEval, &captureHistory);
        Depth      probCutDepth = depth - 4;

        while ((move = mp.next_move()) != Move::none())
        {
            assert(move.is_ok());

            if (move == excludedMove || !pos.legal(move))
                continue;

            assert(pos.capture(move));

            do_move(pos, move, st, ss);

            // Perform a preliminary qsearch to verify that the move holds
            value = -qsearch<NonPV>(pos, ss + 1, -probCutBeta, -probCutBeta + 1);

            // If the qsearch held, perform the regular search
            if (value >= probCutBeta && probCutDepth > 0)
                value = -search<NonPV>(pos, ss + 1, -probCutBeta, -probCutBeta + 1, probCutDepth,
                                       !cutNode);

            undo_move(pos, move);

            if (value >= probCutBeta)
            {
                // Save ProbCut data into transposition table
                ttWriter.write(posKey, value_to_tt(value, ss->ply), ss->ttPv, BOUND_LOWER,
                               probCutDepth + 1, move, unadjustedStaticEval, tt.generation());

                if (!is_decisive(value))
                    return value - (probCutBeta - beta);
            }
        }
    }

moves_loop:  // When in check, search starts here

    // Step 11. A small Probcut idea
    probCutBeta = beta + (search_953_0);
    if ((ttData.bound & BOUND_LOWER) && ttData.depth >= depth - 4 && ttData.value >= probCutBeta
        && !is_decisive(beta) && is_valid(ttData.value) && !is_decisive(ttData.value))
        return probCutBeta;

    const PieceToHistory* contHist[] = {
      (ss - 1)->continuationHistory, (ss - 2)->continuationHistory, (ss - 3)->continuationHistory,
      (ss - 4)->continuationHistory, (ss - 5)->continuationHistory, (ss - 6)->continuationHistory};


    MovePicker mp(pos, ttData.move, depth, &mainHistory, &lowPlyHistory, &captureHistory, contHist,
                  &sharedHistory, ss->ply);

    value = bestValue;

    int moveCount = 0;

    // Step 12. Loop through all pseudo-legal moves until no moves remain
    // or a beta cutoff occurs.
    while ((move = mp.next_move()) != Move::none())
    {
        assert(move.is_ok());

        if (move == excludedMove)
            continue;

        // Check for legality
        if (!pos.legal(move))
            continue;

        // At root obey the "searchmoves" option and skip moves not listed in Root Move List.
        // In MultiPV mode we also skip PV moves that have been already searched.
        if (rootNode && !std::count(rootMoves.begin() + pvIdx, rootMoves.begin() + pvLast, move))
            continue;

        ss->moveCount = ++moveCount;

        if (rootNode && is_mainthread() && nodes > NODES_LIMIT_OUTPUT)
        {
            main_manager()->updates.onIter({depth, UCIEngine::move(move), moveCount + pvIdx});
        }
        if (PvNode)
            (ss + 1)->pv = nullptr;

        extension  = 0;
        capture    = pos.capture(move);
        movedPiece = pos.moved_piece(move);
        givesCheck = pos.gives_check(move);

        // Calculate new depth for this move
        newDepth = depth - 1;

        int delta = beta - alpha;

        int r = reduction(improving, depth, moveCount, delta);

        // Increase reduction for ttPv nodes (*Scaler)
        // Larger values scale well
        if (ss->ttPv)
            r += (search_1012_0);

        // Step 13. Pruning at shallow depths.
        // Depth conditions are important for mate finding.
        if (!rootNode && pos.major_material(us) && !is_loss(bestValue))
        {
            // Skip quiet moves if movecount exceeds our threshold
            if (moveCount >= (3 + depth * depth) / (2 - improving))
                mp.skip_quiet_moves();

            // Reduced depth of the next LMR search
            int lmrDepth = newDepth - r / (search_1023_0);

            if (capture || givesCheck)
            {
                Piece capturedPiece = pos.piece_on(move.to_sq());
                int   captHist = captureHistory[movedPiece][move.to_sq()][type_of(capturedPiece)];

                // Futility pruning for captures
                if (!givesCheck && lmrDepth < (search_1031_0))
                {
                    Value futilityValue = ss->staticEval + (search_1033_0) + (search_1033_1) * lmrDepth
                                        + PieceValue[capturedPiece] + (search_1034_0) * captHist / 1024;

                    if (futilityValue <= alpha)
                        continue;
                }

                // SEE based pruning for captures and checks
                int margin = std::max((search_1041_0) * depth + captHist * (search_1041_1) / 1024, 0);
                if (!pos.see_ge(move, -margin))
                    continue;
            }
            else if (!ss->followPV || !PvNode)
            {
                int dIndex  = std::min(int(depth), int(16)) - 1;
                int history = (*contHist[0])[movedPiece][move.to_sq()]
                            + (*contHist[1])[movedPiece][move.to_sq()]
                            + sharedHistory.pawn_entry(pos)[movedPiece][move.to_sq()];

                // Continuation history based pruning
                if (history < -(search_1053_0) * depth)
                    continue;

                history += (search_1056_0) * mainHistory[us][move.raw()] / 32;

                // (*Scaler): Generally, lower divisors scale well
                lmrDepth += history / lmrDivisor[dIndex];

                Value futilityValue = ss->staticEval + (search_1061_0) + (search_1061_1) * !bestMove + (search_1061_2) * lmrDepth
                                    + (search_1062_0) * (ss->staticEval > alpha);

                // Futility pruning: parent node
                // (*Scaler): Generally, more frequent futility pruning
                // scales well
                if (!ss->inCheck && lmrDepth < (search_1067_0) && futilityValue <= alpha)
                {
                    if (bestValue <= futilityValue && !is_decisive(bestValue)
                        && !is_win(futilityValue))
                        bestValue = futilityValue;
                    continue;
                }

                lmrDepth = std::max(lmrDepth, 0);

                // Prune moves with negative SEE
                if (!pos.see_ge(move, -(search_1078_0) * lmrDepth * lmrDepth))
                    continue;
            }
        }

        // Step 14. Extensions
        // Singular extension search. If all moves but one
        // fail low on a search of (alpha-s, beta-s), and just one fails high on
        // (alpha, beta), then that move is singular and should be extended. To
        // verify this we do a reduced search on the position excluding the ttMove
        // and if the result is lower than ttValue minus a margin, then we will
        // extend the ttMove. Recursive singular search is avoided.

        // (*Scaler) Generally, higher singularBeta (i.e closer to ttValue)
        // and lower extension margins scale well.
        if (!rootNode && move == ttData.move && !excludedMove && depth >= (search_1093_0) + ss->ttPv
            && is_valid(ttData.value) && !is_decisive(ttData.value) && (ttData.bound & BOUND_LOWER)
            && ttData.depth >= depth - 3 && !is_shuffling(move, ss, pos))
        {
            Value singularBeta  = ttData.value - ((search_1097_0) + (search_1097_1) * (ss->ttPv && !PvNode)) * depth / (search_1097_2);
            Depth singularDepth = newDepth / 2;

            ss->excludedMove = move;
            value = search<NonPV>(pos, ss, singularBeta - 1, singularBeta, singularDepth, cutNode);
            ss->excludedMove = Move::none();

            if (value < singularBeta)
            {
                int corrValAdj   = std::abs(correctionValue) / (search_1106_0);
                int doubleMargin = -(search_1107_0) + (search_1107_1) * PvNode - (search_1107_2) * !ttCapture - corrValAdj
                                 - (search_1108_0) * ttMoveHistory / (search_1108_1) - (ss->ply > rootDepth) * (search_1108_2);
                int tripleMargin = (search_1109_0) + (search_1109_1) * PvNode - (search_1109_2) * !ttCapture + (search_1109_3) * ss->ttPv
                                 - corrValAdj - (ss->ply > rootDepth) * (search_1110_0);

                extension =
                  1 + (value < singularBeta - doubleMargin) + (value < singularBeta - tripleMargin);

                depth++;
            }

            // Multi-cut pruning
            // Our ttMove is assumed to fail high based on the bound of the TT entry,
            // and if after excluding the ttMove with a reduced search we fail high
            // over the original beta, we assume this expected cut-node is not
            // singular (multiple moves fail high), and we can prune the whole
            // subtree by returning a softbound.
            else if (value >= beta && !is_decisive(value))
            {
                ttMoveHistory << -(search_1126_0) - (search_1126_1) * depth;
                return value;
            }

            // Negative extensions
            // If other moves failed high over (ttValue - margin) without the
            // ttMove on a reduced search, but we cannot do multi-cut because
            // (ttValue - margin) is lower than the original beta, we do not know
            // if the ttMove is singular or can do a multi-cut, so we reduce the
            // ttMove in favor of other moves based on some conditions:

            // If the ttMove is assumed to fail high over current beta
            else if (ttData.value >= beta)
                extension = -3;

            // If we are on a cutNode but the ttMove is not assumed to fail high
            // over current beta
            else if (cutNode)
                extension = -2;
        }

        uint64_t nodeCount = rootNode ? uint64_t(nodes) : 0;

        // Step 15. Make the move
        do_move(pos, move, st, givesCheck, ss);

        // Add extension to new depth
        newDepth += extension;

        // Decrease reduction for PvNodes (*Scaler)
        if (ss->ttPv)
            r -= (search_1157_0) + PvNode * (search_1157_1) + (ttData.value > alpha) * (search_1157_2)
               + (ttData.depth >= depth) * ((search_1158_0) + cutNode * (search_1158_1));

        r += (search_1160_0);  // Base reduction offset to compensate for other tweaks
        r -= moveCount * 64;
        r -= std::abs(correctionValue) / (search_1162_0);

        // Increase reduction for cut nodes
        if (cutNode)
            r += (search_1166_0) + (search_1166_1) * !ttData.move;

        // Increase reduction if ttMove is a capture
        if (ttCapture)
            r += (search_1170_0);

        // Increase reduction if next ply has a lot of fail high
        if ((ss + 1)->cutoffCnt > 1)
            r += (search_1174_0) + (search_1174_1) * ((ss + 1)->cutoffCnt > 2) + (search_1174_2) * allNode;

        // For first picked move (ttMove) reduce reduction
        else if (move == ttData.move)
            r = std::max(-(search_1178_0), r - (search_1178_1) + (search_1178_2) * cutNode);

        if (capture)
            ss->statScore = (search_1181_0) * int(PieceValue[pos.captured_piece()]) / 128
                          + captureHistory[movedPiece][move.to_sq()][type_of(pos.captured_piece())];
        else
            ss->statScore = 2 * mainHistory[us][move.raw()]
                          + (*contHist[0])[movedPiece][move.to_sq()]
                          + (*contHist[1])[movedPiece][move.to_sq()];

        // Decrease/increase reduction for moves with a good/bad history
        r -= ss->statScore * (search_1189_0) / 8192;

        // Scale up reductions for expected ALL nodes
        if (allNode)
            r += r * (search_1193_0) / ((search_1193_1) * depth + (search_1193_2));

        // Step 16. Late moves reduction / extension (LMR)
        if (depth >= 2 && moveCount > 1)
        {
            // In general we want to cap the LMR depth search at newDepth, but when
            // reduction is negative, we allow this move a limited search extension
            // beyond the first move depth.
            // To prevent problems when the max value is less than the min value,
            // std::clamp has been replaced by a more robust implementation.
            Depth d = std::max(1, std::min(newDepth - r / 1024, newDepth + 2)) + PvNode;

            ss->reduction = newDepth - d;
            value         = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha, d, true);
            ss->reduction = 0;

            // Do a full-depth search when reduced LMR search fails high
            // (*Scaler) Shallower searches here don't scale well
            if (value > alpha)
            {
                // Adjust full-depth search based on LMR results - if the result was
                // good enough search deeper, if it was bad enough search shallower.
                const bool doDeeperSearch    = d < newDepth && value > bestValue + (search_1215_0);
                const bool doShallowerSearch = value < bestValue + (search_1216_0);

                newDepth += doDeeperSearch - doShallowerSearch;

                if (newDepth > d)
                    value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha, newDepth, !cutNode);

                // Post LMR continuation history updates
                update_continuation_histories(ss, movedPiece, move.to_sq(), (search_1224_0));
            }
        }

        // Step 17. Full-depth search when LMR is skipped
        else if (!PvNode || moveCount > 1)
        {
            // Increase reduction if ttMove is not present
            if (!ttData.move)
                r += (search_1233_0);

            // Note that if expected reduction is high, we reduce search depth here
            value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha,
                                   newDepth - (r > (search_1237_0)) - (r > (search_1237_1) && newDepth > 2), !cutNode);
        }

        // For PV nodes only, do a full PV search on the first move or after a fail high,
        // otherwise let the parent node fail low with value <= alpha and try another move.
        if (PvNode && (moveCount == 1 || value > alpha))
        {
            (ss + 1)->pv = &pv;
            (ss + 1)->pv->clear();

            // Extend move from transposition table if we are about to dive into qsearch.
            // decisive score handling improves mate finding and retrograde analysis.
            if (move == ttData.move
                && ((is_valid(ttData.value) && is_decisive(ttData.value) && ttData.depth > 0)
                    || ttData.depth > 1))
                newDepth = std::max(newDepth, 1);

            value = -search<PV>(pos, ss + 1, -beta, -alpha, newDepth, false);
        }

        // Step 18. Undo move
        undo_move(pos, move);

        assert(value > -VALUE_INFINITE && value < VALUE_INFINITE);

        // Step 19. Check for a new best move
        // Finished searching the move. If a stop occurred, the return value of
        // the search cannot be trusted, and we return immediately without updating
        // best move, principal variation nor transposition table.
        if (threads.stop.load(std::memory_order_relaxed))
            return VALUE_ZERO;

        if (rootNode)
        {
            RootMove& rm = *std::find(rootMoves.begin(), rootMoves.end(), move);

            rm.effort += nodes - nodeCount;

            rm.averageScore =
              rm.averageScore != -VALUE_INFINITE ? (value + rm.averageScore) / 2 : value;

            rm.meanSquaredScore = rm.meanSquaredScore != -VALUE_INFINITE * VALUE_INFINITE
                                  ? (value * std::abs(value) + rm.meanSquaredScore) / 2
                                  : value * std::abs(value);

            // PV move or new best move?
            if (moveCount == 1 || value > alpha)
            {
                rm.score = rm.uciScore = value;
                rm.selDepth            = selDepth;
                rm.unset_bound_flags();

                if (value >= beta)
                {
                    rm.scoreLowerbound = true;
                    rm.uciScore        = beta;
                }
                else if (value <= alpha)
                {
                    rm.scoreUpperbound = true;
                    rm.uciScore        = alpha;
                }

                rm.pv.resize(1);

                assert((ss + 1)->pv);

                for (Move pvMove : *(ss + 1)->pv)
                    rm.pv.push_back(pvMove);

                // We record how often the best move has been changed in each iteration.
                // This information is used for time management. In MultiPV mode,
                // we must take care to only do this for the first PV line.
                if (moveCount > 1 && !pvIdx)
                    ++bestMoveChanges;
            }
            else
                // All other moves but the PV, are set to the lowest value: this
                // is not a problem when sorting because the sort is stable and the
                // move position in the list is preserved - just the PV is pushed up.
                rm.score = -VALUE_INFINITE;
        }

        // In case we have an alternative move equal in eval to the current bestmove,
        // promote it to bestmove by pretending it just exceeds alpha (but not beta).
        int inc = (value == bestValue && ss->ply + 2 >= rootDepth && (int(nodes) & 14) == 0
                   && !is_win(std::abs(value) + 1));

        if (value + inc > bestValue)
        {
            bestValue = value;

            if (value + inc > alpha)
            {
                bestMove = move;

                if (PvNode && !rootNode)  // Update pv even in fail-high case
                    ss->pv->update(move, (ss + 1)->pv);

                if (value >= beta)
                {
                    // (*Scaler) Infrequent and small updates scale well
                    ss->cutoffCnt += (extension < 2) || PvNode;
                    assert(value >= beta);  // Fail high
                    break;
                }

                // Reduce other moves if we have found at least one score improvement
                if (depth > 2 && depth < (search_1345_0) && !is_decisive(value))
                    depth -= 2;

                assert(depth > 0);
                alpha = value;  // Update alpha! Always alpha < beta
            }
        }

        // If the move is worse than some previously searched move,
        // remember it, to update its stats later.
        if (move != bestMove && moveCount <= SEARCHEDLIST_CAPACITY)
        {
            if (capture)
                capturesSearched.push_back(move);
            else
                quietsSearched.push_back(move);
        }
    }

    // Step 20. Check for mate
    // All legal moves have been searched and if there are no legal moves,
    // it must be a mate. If we are in a singular extension search then
    // return a fail low score.

    assert(moveCount || !ss->inCheck || excludedMove || !MoveList<LEGAL>(pos).size());

    // Adjust best value for fail high cases
    if (bestValue >= beta && !is_decisive(bestValue) && !is_decisive(alpha))
        bestValue = (bestValue * depth + beta) / (depth + 1);

    if (!moveCount)
        bestValue = excludedMove ? alpha : mated_in(ss->ply);

    // If there is a move that produces search value greater than alpha,
    // we update the stats of searched moves.
    else if (bestMove)
    {
        update_all_stats(pos, ss, *this, bestMove, prevSq, quietsSearched, capturesSearched, depth,
                         ttData.move);
        if (!PvNode)
            ttMoveHistory << (bestMove == ttData.move ? (search_1385_0) : -(search_1385_1));
    }

    // Bonus for prior quiet countermove that caused the fail low
    else if (!priorCapture && prevSq != SQ_NONE)
    {
        int bonusScale = -(search_1391_0);
        bonusScale -= (ss - 1)->statScore / (search_1392_0);
        bonusScale += std::min((search_1393_0) * depth, 512);
        bonusScale += (search_1394_0) * ((ss - 1)->moveCount > (search_1394_1));
        bonusScale += (search_1395_0) * (!ss->inCheck && bestValue <= ss->staticEval - (search_1395_1));
        bonusScale += (search_1396_0) * (!(ss - 1)->inCheck && bestValue <= -(ss - 1)->staticEval - (search_1396_1));

        bonusScale = std::max(bonusScale, 0);

        // scaledBonus ranges from 0 to roughly 2.3M, overflows happen for multipliers larger than 900
        const int scaledBonus = std::min((search_1401_0) * depth - (search_1401_1), (search_1401_2)) * bonusScale;

        update_continuation_histories(ss - 1, pos.piece_on(prevSq), prevSq,
                                      scaledBonus * (search_1404_0) / 16384);

        mainHistory[~us][((ss - 1)->currentMove).raw()] << scaledBonus * (search_1406_0) / 32768;

        if (type_of(pos.piece_on(prevSq)) != PAWN)
            sharedHistory.pawn_entry(pos)[pos.piece_on(prevSq)][prevSq] << scaledBonus * (search_1409_0) / 8192;
    }

    // Bonus for prior capture countermove that caused the fail low
    else if (priorCapture && prevSq != SQ_NONE)
    {
        Piece capturedPiece = pos.captured_piece();
        assert(capturedPiece != NO_PIECE);
        captureHistory[pos.piece_on(prevSq)][prevSq][type_of(capturedPiece)] << (search_1417_0);
    }

    if (PvNode)
        bestValue = std::min(bestValue, maxValue);

    // If no good move is found and the previous position was ttPv, then the previous
    // opponent move is probably good and the new position is added to the search tree.
    if (bestValue <= alpha)
        ss->ttPv = ss->ttPv || (ss - 1)->ttPv;

    // Write gathered information in transposition table. Note that the
    // static evaluation is saved as it was before correction history.
    if (!excludedMove && !(rootNode && pvIdx))
        ttWriter.write(posKey, value_to_tt(bestValue, ss->ply), ss->ttPv,
                       bestValue >= beta    ? BOUND_LOWER
                       : PvNode && bestMove ? BOUND_EXACT
                                            : BOUND_UPPER,
                       moveCount != 0 ? depth : std::min(MAX_PLY - 1, depth + 6), bestMove,
                       unadjustedStaticEval, tt.generation());

    // Adjust correction history if the best move is not a capture
    // and the error direction matches whether we are above/below bounds.
    if (!ss->inCheck && !(bestMove && pos.capture(bestMove))
        && (bestValue > ss->staticEval) == bool(bestMove))
    {
        auto bonus =
          std::clamp(int(bestValue - ss->staticEval) * depth * (bestMove ? (search_1444_0) : (search_1444_1)) / 128,
                     -CORRECTION_HISTORY_LIMIT / 4, CORRECTION_HISTORY_LIMIT / 4);
        update_correction_history(pos, ss, *this, (search_1446_0) * bonus / 1024);
    }

    assert(bestValue > -VALUE_INFINITE && bestValue < VALUE_INFINITE);

    return bestValue;
}


// Quiescence search function, which is called by the main search function with
// depth zero, or recursively with further decreasing depth. With depth <= 0, we
// "should" be using static eval only, but tactical moves may confuse the static eval.
// To fight this horizon effect, we implement this qsearch of tactical moves.
// See https://www.chessprogramming.org/Horizon_Effect
// and https://www.chessprogramming.org/Quiescence_Search
template<NodeType nodeType>
Value Search::Worker::qsearch(Position& pos, Stack* ss, Value alpha, Value beta) {

    static_assert(nodeType != Root);
    constexpr bool PvNode = nodeType == PV;

    assert(alpha >= -VALUE_INFINITE && alpha < beta && beta <= VALUE_INFINITE);
    assert(PvNode || (alpha == beta - 1));

    PVMoves   pv;
    StateInfo st;

    Key   posKey;
    Move  move, bestMove;
    Value bestValue, value, futilityBase;
    bool  pvHit, givesCheck, capture;
    int   moveCount;

    // Step 1. Initialize node
    if (PvNode)
    {
        (ss + 1)->pv = &pv;
        ss->pv->clear();
    }

    bestMove    = Move::none();
    ss->inCheck = bool(pos.checkers());
    moveCount   = 0;

    // Used to send selDepth info to GUI (selDepth counts from 1, ply from 0)
    if (PvNode && selDepth < ss->ply + 1)
        selDepth = ss->ply + 1;

    // Step 2. Check for repetition or maximum ply reached
    Value result = VALUE_NONE;
    if (pos.rule_judge(result, ss->ply))
        return result;
    if (result != VALUE_NONE)
    {
        assert(result != VALUE_DRAW);

        // 2 fold result is mate for us, the only chance for the opponent is to get a draw
        // We can guarantee to get at least a draw score during searching for that line
        if (result > VALUE_DRAW)
            alpha = std::max(alpha, VALUE_DRAW);
        // 2 fold result is mated for us, the only chance for us is to get a draw
        // We can guarantee to get no more than a draw score during searching for that line
        else
            beta = std::min(beta, VALUE_DRAW);

        if (alpha >= beta)
            return alpha;
    }

    if (ss->ply >= MAX_PLY)
        return !ss->inCheck ? evaluate(pos) : VALUE_DRAW;

    assert(0 <= ss->ply && ss->ply < MAX_PLY);

    // Step 3. Transposition table lookup
    posKey                         = pos.key();
    auto [ttHit, ttData, ttWriter] = tt.probe(posKey);
    // Need further processing of the saved data
    ss->ttHit    = ttHit;
    ttData.move  = ttHit ? ttData.move : Move::none();
    ttData.value = ttHit ? value_from_tt(ttData.value, ss->ply, pos.rule60_count()) : VALUE_NONE;
    pvHit        = ttHit && ttData.is_pv;

    // At non-PV nodes we check for an early TT cutoff
    if (!PvNode && ttData.depth >= DEPTH_QS
        && is_valid(ttData.value)  // Can happen when !ttHit or when access race in probe()
        && (ttData.bound & (ttData.value >= beta ? BOUND_LOWER : BOUND_UPPER)))
        return ttData.value;

    // Step 4. Static evaluation of the position
    Value unadjustedStaticEval = VALUE_NONE;
    if (ss->inCheck)
        bestValue = futilityBase = -VALUE_INFINITE;
    else
    {
        const auto correctionValue = correction_value(*this, pos, ss);

        if (ss->ttHit)
        {
            // Never assume anything about values stored in TT
            unadjustedStaticEval = ttData.eval;

            if (!is_valid(unadjustedStaticEval))
                unadjustedStaticEval = evaluate(pos);

            ss->staticEval = bestValue =
              to_corrected_static_eval(unadjustedStaticEval, correctionValue);

            // ttValue can be used as a better position evaluation
            if (is_valid(ttData.value) && !is_decisive(ttData.value)
                && (ttData.bound & (ttData.value > bestValue ? BOUND_LOWER : BOUND_UPPER)))
                bestValue = ttData.value;
        }
        else
        {
            unadjustedStaticEval = evaluate(pos);
            ss->staticEval       = bestValue =
              to_corrected_static_eval(unadjustedStaticEval, correctionValue);
        }

        // Stand pat. Return immediately if static value is at least beta
        if (bestValue >= beta)
        {
            if (!is_decisive(bestValue))
                bestValue = ((search_1570_0) * bestValue + (search_1570_1) * beta) / 1024;

            if (!ss->ttHit)
                ttWriter.write(posKey, VALUE_NONE, false, BOUND_LOWER, DEPTH_UNSEARCHED,
                               Move::none(), unadjustedStaticEval, tt.generation());
            return bestValue;
        }

        if (bestValue > alpha)
            alpha = bestValue;

        futilityBase = ss->staticEval + (search_1581_0);
    }

    const PieceToHistory* contHist[] = {(ss - 1)->continuationHistory};

    Square prevSq = ((ss - 1)->currentMove).is_ok() ? ((ss - 1)->currentMove).to_sq() : SQ_NONE;

    // Initialize a MovePicker object for the current position, and prepare to search
    // the moves. We presently use two stages of move generator in quiescence search:
    // captures, or evasions only when in check.
    MovePicker mp(pos, ttData.move, DEPTH_QS, &mainHistory, &lowPlyHistory, &captureHistory,
                  contHist, &sharedHistory, ss->ply);

    // Step 5. Loop through all pseudo-legal moves until no moves remain or a beta
    // cutoff occurs.
    while ((move = mp.next_move()) != Move::none())
    {
        assert(move.is_ok());

        if (!pos.legal(move))
            continue;

        givesCheck = pos.gives_check(move);
        capture    = pos.capture(move);

        moveCount++;

        // Step 6. Pruning
        if (!is_loss(bestValue))
        {
            // Futility pruning and moveCount pruning
            if (!givesCheck && move.to_sq() != prevSq && !is_loss(futilityBase))
            {
                if (moveCount > 2)
                    continue;

                Value futilityValue = futilityBase + PieceValue[pos.piece_on(move.to_sq())];

                // If static eval + value of piece we are going to capture is
                // much lower than alpha, we can prune this move.
                if (futilityValue <= alpha)
                {
                    bestValue = std::max(bestValue, futilityValue);
                    continue;
                }

                // If static exchange evaluation is low enough
                // we can prune this move.
                if (!pos.see_ge(move, alpha - futilityBase))
                {
                    bestValue = std::max(bestValue, std::min(alpha, futilityBase));
                    continue;
                }
            }

            // Skip non-captures
            if (!capture)
                continue;

            // Do not search moves with bad enough SEE values
            if (!pos.see_ge(move, -(search_1641_0)))
                continue;
        }

        // Step 7. Make and search the move
        do_move(pos, move, st, givesCheck, ss);

        value = -qsearch<nodeType>(pos, ss + 1, -beta, -alpha);
        undo_move(pos, move);

        assert(value > -VALUE_INFINITE && value < VALUE_INFINITE);

        // Step 8. Check for a new best move
        if (value > bestValue)
        {
            bestValue = value;

            if (value > alpha)
            {
                bestMove = move;

                if (PvNode)  // Update pv even in fail-high case
                    ss->pv->update(move, (ss + 1)->pv);

                if (value < beta)  // Update alpha here!
                    alpha = value;
                else
                    break;  // Fail high
            }
        }
    }

    // Step 9. Check for mate and stalemate
    // All legal moves have been searched. A special case: if no legal
    // moves were found, it is checkmate.
    if (!moveCount && (ss->inCheck || [&] {
            for (const auto& m : MoveList<QUIETS>(pos))
                if (pos.legal(m))
                    return false;
            return true;
        }()))
    {
        assert(!MoveList<LEGAL>(pos).size());
        return mated_in(ss->ply);  // Plies to mate from the root
    }

    if (!is_decisive(bestValue) && bestValue > beta)
        bestValue = ((search_1688_0) * bestValue + (search_1688_1) * beta) / 1024;

    // Save gathered info in transposition table. The static evaluation
    // is saved as it was before adjustment by correction history.
    ttWriter.write(posKey, value_to_tt(bestValue, ss->ply), pvHit,
                   bestValue >= beta ? BOUND_LOWER : BOUND_UPPER, DEPTH_QS, bestMove,
                   unadjustedStaticEval, tt.generation());

    assert(bestValue > -VALUE_INFINITE && bestValue < VALUE_INFINITE);

    return bestValue;
}

int Search::Worker::reduction(bool i, Depth d, int mn, int delta) const {
    int reductionScale = reductions[d] * reductions[mn];
    return reductionScale - delta * (search_1703_0) / rootDelta + !i * reductionScale * (search_1703_1) / 512 + (search_1703_2);
}

// elapsed() returns the time elapsed since the search started. If the
// 'nodestime' option is enabled, it will return the count of nodes searched
// instead. This function is called to check whether the search should be
// stopped based on predefined thresholds like time limits or nodes searched.
//
// elapsed_time() returns the actual time elapsed since the start of the search.
// This function is intended for use only when printing PV outputs, and not used
// for making decisions within the search algorithm itself.
TimePoint Search::Worker::elapsed() const {
    return main_manager()->tm.elapsed([this]() { return threads.nodes_searched(); });
}

TimePoint Search::Worker::elapsed_time() const { return main_manager()->tm.elapsed_time(); }

Value Search::Worker::evaluate(const Position& pos) {
    return Eval::evaluate(network[numaAccessToken], pos, accumulatorStack, refreshTable,
                          optimism[pos.side_to_move()]);
}

namespace {
// Adjusts a mate from "plies to mate from the root" to
// "plies to mate from the current position". Standard scores are unchanged.
// The function is called before storing a value in the transposition table.
Value value_to_tt(Value v, int ply) { return is_win(v) ? v + ply : is_loss(v) ? v - ply : v; }


// Inverse of value_to_tt(): it adjusts a mate score from the transposition
// table (which refers to the plies to mate/be mated from current position) to
// "plies to mate/be mated from the root". However, to avoid potentially false
// mate scores related to the 60 moves rule and the graph history interaction,
// we return the highest non-mate score instead.
Value value_from_tt(Value v, int ply, int r60c) {

    if (!is_valid(v))
        return VALUE_NONE;

    // Handle win
    if (is_win(v))
        // Downgrade a potentially false mate score
        return VALUE_MATE - v > 120 - r60c ? VALUE_MATE_IN_MAX_PLY - 1 : v - ply;

    // Handle loss
    if (is_loss(v))
        // Downgrade a potentially false mate score.
        return VALUE_MATE + v > 120 - r60c ? VALUE_MATED_IN_MAX_PLY + 1 : v + ply;

    return v;
}


// Updates stats at the end of search() when a bestMove is found
void update_all_stats(const Position& pos,
                      Stack*          ss,
                      Search::Worker& workerThread,
                      Move            bestMove,
                      Square          prevSq,
                      SearchedList&   quietsSearched,
                      SearchedList&   capturesSearched,
                      Depth           depth,
                      Move            ttMove) {

    CapturePieceToHistory& captureHistory = workerThread.captureHistory;
    Piece                  movedPiece     = pos.moved_piece(bestMove);
    PieceType              capturedPiece;

    int bonus =
      std::min((search_1772_0) * depth - (search_1772_1), (search_1772_2)) + (search_1772_3) * (bestMove == ttMove) + (ss - 1)->statScore / 32;
    int malus = std::min((search_1773_0) * depth - (search_1773_1), (search_1773_2));

    if (!pos.capture(bestMove))
    {
        update_quiet_histories(pos, ss, workerThread, bestMove, bonus * (search_1777_0) / 1024);

        int actualMalus = malus * (search_1779_0) / 1024;
        // Decrease stats for all non-best quiet moves
        for (Move move : quietsSearched)
        {
            actualMalus = actualMalus * (search_1783_0) / 1024;
            update_quiet_histories(pos, ss, workerThread, move, -actualMalus);
        }
    }
    else
    {
        // Increase stats for the best move in case it was a capture move
        capturedPiece = type_of(pos.piece_on(bestMove.to_sq()));
        captureHistory[movedPiece][bestMove.to_sq()][capturedPiece] << bonus * (search_1791_0) / 1024;
    }

    // Extra penalty for a quiet early move that was not a TT move in
    // previous ply when it gets refuted.
    if (prevSq != SQ_NONE && ((ss - 1)->moveCount == 1 + (ss - 1)->ttHit) && !pos.captured_piece())
        update_continuation_histories(ss - 1, pos.piece_on(prevSq), prevSq, -malus * (search_1797_0) / 1024);

    // Decrease stats for all non-best capture moves
    for (Move move : capturesSearched)
    {
        movedPiece    = pos.moved_piece(move);
        capturedPiece = type_of(pos.piece_on(move.to_sq()));
        captureHistory[movedPiece][move.to_sq()][capturedPiece] << -malus * (search_1804_0) / 1024;
    }
}


// Updates the continuation histories for the move pairs formed by
// the current move and the moves played in previous plies.
void update_continuation_histories(Stack* ss, Piece pc, Square to, int bonus) {
    std::array<ConthistBonus, 6> conthist_bonuses = {
      {{1, (search_1813_0)}, {2, (search_1813_1)}, {3, (search_1813_2)}, {4, (search_1813_3)}, {5, (search_1813_4)}, {6, (search_1813_5)}}};

    // Multipliers for positive history consistency
    int CMHCMultipliers[] = {(search_1816_0), (search_1816_1), (search_1816_2), (search_1816_3), (search_1816_4), (search_1816_5), (search_1816_6)};
    int           positiveCount     = 0;

    for (const auto [i, weight] : conthist_bonuses)
    {
        // Only update the first 2 continuation histories if we are in check
        if (ss->inCheck && i > 2)
            break;

        if (((ss - i)->currentMove).is_ok())
        {
            auto& historyEntry = (*(ss - i)->continuationHistory)[pc][to];
            if (historyEntry > 0)
                positiveCount++;

            int multiplier = CMHCMultipliers[positiveCount];
            historyEntry << (bonus * weight * multiplier / 131072) + (search_1832_0) * (i < 2);
        }
    }
}

// Updates move sorting heuristics

void update_quiet_histories(
  const Position& pos, Stack* ss, Search::Worker& workerThread, Move move, int bonus) {

    Color us = pos.side_to_move();
    workerThread.mainHistory[us][move.raw()] << bonus;  // Untuned to prevent duplicate effort

    if (ss->ply < LOW_PLY_HISTORY_SIZE)
        workerThread.lowPlyHistory[ss->ply][move.raw()] << bonus * (search_1846_0) / 1024;

    update_continuation_histories(ss, pos.moved_piece(move), move.to_sq(), bonus * (search_1848_0) / 1024);

    workerThread.sharedHistory.pawn_entry(pos)[pos.moved_piece(move)][move.to_sq()]
      << bonus * (bonus > -(search_1851_0) ? (search_1851_1) : (search_1851_2)) / 1024;
}
}

// Used to print debug info and, more importantly, to detect
// when we are out of available time and thus stop the search.
void SearchManager::check_time(Search::Worker& worker) {
    if (--callsCnt > 0)
        return;

    // When using nodes, ensure checking rate is not lower than 0.1% of nodes
    callsCnt = worker.limits.nodes ? std::min(512, int(worker.limits.nodes / 1024)) : 512;

    static TimePoint lastInfoTime = now();

    TimePoint elapsed = tm.elapsed([&worker]() { return worker.threads.nodes_searched(); });
    TimePoint tick    = worker.limits.startTime + elapsed;

    if (tick - lastInfoTime >= 1000)
    {
        lastInfoTime = tick;
        dbg_print();
    }

    // We should not stop pondering until told so by the GUI
    if (ponder)
        return;

    if ((worker.limits.use_time_management() && (elapsed > tm.maximum() || stopOnPonderhit))
        || (worker.limits.movetime && elapsed >= worker.limits.movetime)
        || (worker.limits.nodes && worker.threads.nodes_searched() >= worker.limits.nodes))
        worker.threads.stop = true;
}

void SearchManager::pv(Search::Worker&           worker,
                       const ThreadPool&         threads,
                       const TranspositionTable& tt,
                       Depth                     depth) const {

    const auto  nodes     = threads.nodes_searched();
    const auto& rootMoves = worker.rootMoves;
    const auto& pos       = worker.rootPos;
    size_t      multiPV   = std::min(size_t(worker.options["MultiPV"]), rootMoves.size());

    for (size_t i = 0; i < multiPV; ++i)
    {
        bool usePreviousScore = rootMoves[i].score == -VALUE_INFINITE;

        if (depth == 1 && usePreviousScore && i > 0)
            continue;

        Depth d = usePreviousScore ? std::max(1, depth - 1) : depth;
        Value v = usePreviousScore ? rootMoves[i].previousScore : rootMoves[i].uciScore;

        if (v == -VALUE_INFINITE)
            v = VALUE_ZERO;

        std::string pv;
        for (Move m : rootMoves[i].pv)
            pv += UCIEngine::move(m) + " ";

        // Remove last whitespace
        if (!pv.empty())
            pv.pop_back();

        auto wdl   = worker.options["UCI_ShowWDL"] ? UCIEngine::wdl(v, pos) : "";
        auto bound = rootMoves[i].scoreLowerbound
                     ? "lowerbound"
                     : (rootMoves[i].scoreUpperbound ? "upperbound" : "");

        InfoFull info;

        info.depth    = d;
        info.selDepth = rootMoves[i].selDepth;
        info.multiPV  = i + 1;
        info.score    = {v, pos};
        info.wdl      = wdl;

        // TB and previous scores are exact, even though their bound flags may say otherwise.
        if (!usePreviousScore)
            info.bound = bound;

        TimePoint time = std::max(TimePoint(1), tm.elapsed_time());
        info.timeMs    = time;
        info.nodes     = nodes;
        info.nps       = nodes * 1000 / time;
        info.tbHits    = 0;
        info.pv        = pv;
        info.hashfull  = tt.hashfull();

        updates.onUpdateFull(info);
    }
}

// Called in case we have no ponder move before exiting the search,
// for instance, in case we stop the search during a fail high at root.
// We try hard to have a ponder move to return to the GUI,
// otherwise in case of 'ponder on' we have nothing to think about.
bool RootMove::extract_ponder_from_tt(const TranspositionTable& tt, Position& pos) {

    assert(pv.size() == 1 && pv[0] != Move::none());

    StateInfo st;
    pos.do_move(pv[0], st, &tt);

    Value _;
    if (!pos.rule_judge(_, 1))
    {
        auto [ttHit, ttData, ttWriter] = tt.probe(pos.key());
        if (ttHit && MoveList<LEGAL>(pos).contains(ttData.move))
            pv.push_back(ttData.move);
    }

    pos.undo_move(pv[0]);
    return pv.size() > 1;
}


}  // namespace Stockfish
