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
#include "tune.h"
#include "types.h"
#include "uci.h"
#include "ucioption.h"

namespace Stockfish {

static std::array<int, 16> lmrDivisor = {3307, 2930, 2874, 2818, 3215, 3225, 3224, 2782,
                                         2858, 2919, 3088, 3275, 3180, 2868, 3006, 3599};

using namespace Search;

namespace {

int LmrEvalWeight      = 3;
int LmrEvalLower       = -64;
int LmrEvalUpper       = 96;
int QuietFutilityDepth = 129;
int QuietFutilityEval  = 112;
int QuietFutilityBase  = 319;
int NmpEvalDivisor     = 256;
int RazorDepth         = 709;

TUNE(SetRange(1, 6), LmrEvalWeight);
TUNE(SetRange(-192, -16), LmrEvalLower);
TUNE(SetRange(24, 288), LmrEvalUpper);
TUNE(SetRange(80, 180), QuietFutilityDepth);
TUNE(SetRange(48, 192), QuietFutilityEval);
TUNE(SetRange(192, 448), QuietFutilityBase);
TUNE(SetRange(128, 512), NmpEvalDivisor);
TUNE(SetRange(480, 960), RazorDepth);

// Search tuning candidates from Stockfish ebcea3ef and subsequent search changes.
// These initialization parameters take effect on ucinewgame.
int CaptureHistoryInit = -607;
TUNE(SetRange(-760, -450), CaptureHistoryInit);
int CorrectionHistoryInit = -6;
TUNE(SetRange(-10, -2), CorrectionHistoryInit);
int PawnHistoryInit = -1247;
TUNE(SetRange(-1560, -930), PawnHistoryInit);
int ContinuationHistoryInit = -436;
TUNE(SetRange(-550, -325), ContinuationHistoryInit);
int ReductionTableScale = 1740;
TUNE(SetRange(1500, 2000), ReductionTableScale);

int CorrectionContinuation = 8982;
TUNE(SetRange(6736, 11228), CorrectionContinuation);

int CorrectionFallback = 71856;
TUNE(SetRange(53892, 89820), CorrectionFallback);

int CorrectionPawn = 4547;
TUNE(SetRange(3410, 5684), CorrectionPawn);

int CorrectionMinor = 3804;
TUNE(SetRange(2853, 4755), CorrectionMinor);

int CorrectionNonPawn = 8213;
TUNE(SetRange(6159, 10267), CorrectionNonPawn);

int CorrectionMinorUpdate = 145;
TUNE(SetRange(108, 182), CorrectionMinorUpdate);

int CorrectionContinuation2Update = 131;
TUNE(SetRange(98, 164), CorrectionContinuation2Update);

int CorrectionContinuation4Update = 63;
TUNE(SetRange(47, 79), CorrectionContinuation4Update);

int LowPlyHistoryInit = 99;
TUNE(SetRange(74, 124), LowPlyHistoryInit);

int MainHistoryDecay = 768;
TUNE(SetRange(576, 960), MainHistoryDecay);

int AspirationDivisor = 39605;
TUNE(SetRange(29703, 49507), AspirationDivisor);

int OptimismWeight = 92;
TUNE(SetRange(69, 115), OptimismWeight);

int OptimismOffset = 95;
TUNE(SetRange(71, 119), OptimismOffset);

int AspirationGrowth = 44;
TUNE(SetRange(32, 64), AspirationGrowth);

int HindsightMargin = 193;
TUNE(SetRange(144, 242), HindsightMargin);

int TtQuietDepth = 108;
TUNE(SetRange(81, 135), TtQuietDepth);

int TtQuietLimit = 1773;
TUNE(SetRange(1329, 2217), TtQuietLimit);

int TtQuietPenalty = 2218;
TUNE(SetRange(1663, 2773), TtQuietPenalty);

int EvalDiffLower = -110;
TUNE(SetRange(-138, -82), EvalDiffLower);

int EvalDiffUpper = 187;
TUNE(SetRange(140, 234), EvalDiffUpper);

int EvalDiffOffset = 34;
TUNE(SetRange(25, 43), EvalDiffOffset);

int EvalDiffMainWeight = 13;
TUNE(SetRange(8, 18), EvalDiffMainWeight);

int FutilityBase = 40;
TUNE(SetRange(36, 60), FutilityBase);

int FutilityLimit = 129;
TUNE(SetRange(96, 162), FutilityLimit);

int FutilityImproving = 2512;
TUNE(SetRange(1884, 3140), FutilityImproving);

int FutilityWorsening = 340;
TUNE(SetRange(255, 425), FutilityWorsening);

int FutilityCorrectionDivisor = 132109;
TUNE(SetRange(99081, 165137), FutilityCorrectionDivisor);

int FutilityEvalWeight = 308;
TUNE(SetRange(231, 385), FutilityEvalWeight);

int NmpDepthMargin = 8;
TUNE(SetRange(4, 12), NmpDepthMargin);

int NmpImprovingMargin = 50;
TUNE(SetRange(37, 63), NmpImprovingMargin);

int NmpBaseMargin = 187;
TUNE(SetRange(140, 234), NmpBaseMargin);

int ProbCutMargin = 251;
TUNE(SetRange(188, 314), ProbCutMargin);

int ProbCutImproving = 66;
TUNE(SetRange(49, 83), ProbCutImproving);

int LmrTtPvInitial = 931;
TUNE(SetRange(698, 1164), LmrTtPvInitial);

int CaptureFutilityBase = 322;
TUNE(SetRange(241, 403), CaptureFutilityBase);

int CaptureFutilityDepth = 336;
TUNE(SetRange(252, 420), CaptureFutilityDepth);

int CaptureFutilityHistory = 229;
TUNE(SetRange(171, 287), CaptureFutilityHistory);

int CaptureSeeDepth = 256;
TUNE(SetRange(192, 320), CaptureSeeDepth);

int QuietHistoryPruning = 2995;
TUNE(SetRange(2246, 3744), QuietHistoryPruning);

int QuietHistoryWeight = 73;
TUNE(SetRange(54, 92), QuietHistoryWeight);

int QuietSeeDepth = 35;
TUNE(SetRange(26, 44), QuietSeeDepth);

int SingularMargin = 44;
TUNE(SetRange(33, 55), SingularMargin);

int SingularTtPvMargin = 72;
TUNE(SetRange(54, 90), SingularTtPvMargin);

int SingularCorrectionDivisor = 265845;
TUNE(SetRange(199383, 332307), SingularCorrectionDivisor);

int DoubleExtensionBase = -4;
TUNE(SetRange(-32, 24), DoubleExtensionBase);

int DoubleExtensionPv = 234;
TUNE(SetRange(175, 293), DoubleExtensionPv);

int DoubleExtensionQuiet = 172;
TUNE(SetRange(129, 215), DoubleExtensionQuiet);

int DoubleExtensionHistory = 1085;
TUNE(SetRange(813, 1357), DoubleExtensionHistory);

int DoubleExtensionPly = 43;
TUNE(SetRange(32, 54), DoubleExtensionPly);

int TripleExtensionBase = 106;
TUNE(SetRange(79, 133), TripleExtensionBase);

int TripleExtensionPv = 299;
TUNE(SetRange(224, 374), TripleExtensionPv);

int TripleExtensionTtPv = 93;
TUNE(SetRange(69, 117), TripleExtensionTtPv);

int TripleExtensionPly = 60;
TUNE(SetRange(45, 75), TripleExtensionPly);

int MultiCutHistoryBase = 397;
TUNE(SetRange(297, 497), MultiCutHistoryBase);

int MultiCutHistoryDepth = 103;
TUNE(SetRange(77, 129), MultiCutHistoryDepth);

int MultiCutCorrectionWeight = 177;
TUNE(SetRange(132, 222), MultiCutCorrectionWeight);

int LmrTtPvBase = 2363;
TUNE(SetRange(1772, 2954), LmrTtPvBase);

int LmrPvWeight = 963;
TUNE(SetRange(722, 1204), LmrPvWeight);

int LmrTtValueWeight = 1121;
TUNE(SetRange(840, 1402), LmrTtValueWeight);

int LmrTtDepthWeight = 1137;
TUNE(SetRange(852, 1422), LmrTtDepthWeight);

int LmrTtCutWeight = 922;
TUNE(SetRange(691, 1153), LmrTtCutWeight);

int LmrBase = 855;
TUNE(SetRange(641, 1069), LmrBase);

int LmrMoveCount = 64;
TUNE(SetRange(48, 80), LmrMoveCount);

int LmrCorrectionDivisor = 30558;
TUNE(SetRange(22918, 38198), LmrCorrectionDivisor);

int LmrCutBase = 3251;
TUNE(SetRange(2438, 4064), LmrCutBase);

int LmrCutNoTt = 1048;
TUNE(SetRange(786, 1310), LmrCutNoTt);

int LmrTtCapture = 1571;
TUNE(SetRange(1178, 1964), LmrTtCapture);

int LmrFirstMove = 2730;
TUNE(SetRange(2047, 3413), LmrFirstMove);

int LmrCaptureValue = 953;
TUNE(SetRange(714, 1192), LmrCaptureValue);

int LmrMainHistory = 2048;
TUNE(SetRange(1536, 2560), LmrMainHistory);

int LmrContinuationHistory = 1126;
TUNE(SetRange(844, 1408), LmrContinuationHistory);

int LmrHistoryWeight = 946;
TUNE(SetRange(709, 1183), LmrHistoryWeight);

int LmrAllWeight = 256;
TUNE(SetRange(192, 320), LmrAllWeight);

int LmrAllOffset = 256;
TUNE(SetRange(192, 320), LmrAllOffset);

int LmrDeeperMargin = 60;
TUNE(SetRange(45, 75), LmrDeeperMargin);

int LmrShallowerMargin = 9;
TUNE(SetRange(4, 16), LmrShallowerMargin);

int LmrContinuationBonus = 1528;
TUNE(SetRange(1146, 1910), LmrContinuationBonus);

int LmrNoTt = 979;
TUNE(SetRange(734, 1224), LmrNoTt);

int LmrSkipThreshold1 = 3135;
TUNE(SetRange(2600, 3600), LmrSkipThreshold1);

int LmrSkipThreshold2 = 4840;
TUNE(SetRange(4000, 5800), LmrSkipThreshold2);

int LmrWindowWeight = 1138;
TUNE(SetRange(853, 1423), LmrWindowWeight);

int LmrNotImproving = 166;
TUNE(SetRange(124, 208), LmrNotImproving);

int LmrOffset = 1934;
TUNE(SetRange(1450, 2418), LmrOffset);

int TtMoveBonus = 796;
TUNE(SetRange(597, 995), TtMoveBonus);

int TtMoveMalus = 855;
TUNE(SetRange(641, 1069), TtMoveMalus);

int CaptureCountermoveBonus = 983;
TUNE(SetRange(737, 1229), CaptureCountermoveBonus);

int CorrectionUpdateWeight = 1069;
TUNE(SetRange(801, 1337), CorrectionUpdateWeight);

int QsearchStandPatWeight = 467;
TUNE(SetRange(350, 584), QsearchStandPatWeight);

int QsearchFutility = 220;
TUNE(SetRange(165, 275), QsearchFutility);

int QsearchFailHighWeight = 481;
TUNE(SetRange(360, 602), QsearchFailHighWeight);

int HistoryBonusDepth = 162;
TUNE(SetRange(121, 203), HistoryBonusDepth);

int HistoryBonusOffset = 87;
TUNE(SetRange(65, 109), HistoryBonusOffset);

int HistoryBonusLimit = 1602;
TUNE(SetRange(1201, 2003), HistoryBonusLimit);

int HistoryTtBonus = 336;
TUNE(SetRange(252, 420), HistoryTtBonus);

int HistoryMalusDepth = 870;
TUNE(SetRange(652, 1088), HistoryMalusDepth);

int HistoryMalusOffset = 148;
TUNE(SetRange(111, 185), HistoryMalusOffset);

int HistoryMalusLimit = 2000;
TUNE(SetRange(1500, 2500), HistoryMalusLimit);

int QuietBonusWeight = 899;
TUNE(SetRange(674, 1124), QuietBonusWeight);

int QuietMalusWeight = 1100;
TUNE(SetRange(825, 1375), QuietMalusWeight);

int QuietMalusDecay = 950;
TUNE(SetRange(800, 1024), QuietMalusDecay);

int CaptureBonusWeight = 1455;
TUNE(SetRange(1091, 1819), CaptureBonusWeight);

int ContinuationMalusWeight = 617;
TUNE(SetRange(462, 772), ContinuationMalusWeight);

int CaptureMalusWeight = 1440;
TUNE(SetRange(1080, 1800), CaptureMalusWeight);

int ContinuationFirstBonus = 83;
TUNE(SetRange(62, 104), ContinuationFirstBonus);

int LowPlyBonusWeight = 693;
TUNE(SetRange(519, 867), LowPlyBonusWeight);

int ContinuationBonusWeight = 972;
TUNE(SetRange(800, 1050), ContinuationBonusWeight);

int PawnBonusThreshold = -7;
TUNE(SetRange(-16, -1), PawnBonusThreshold);

int PawnBonusWeight = 913;
TUNE(SetRange(684, 1142), PawnBonusWeight);

int PawnMalusWeight = 553;
TUNE(SetRange(414, 692), PawnMalusWeight);

int LmrCutoffBase = 256;
TUNE(SetRange(192, 320), LmrCutoffBase);

int LmrCutoffExtra = 1024;
TUNE(SetRange(768, 1280), LmrCutoffExtra);

int LmrCutoffAll = 1024;
TUNE(SetRange(768, 1280), LmrCutoffAll);

int LmrContinuation2History = 1024;
TUNE(SetRange(768, 1280), LmrContinuation2History);

int SingularDivisor = 69;
TUNE(SetRange(51, 87), SingularDivisor);

int DoubleExtensionHistoryDivisor = 133615;
TUNE(SetRange(100211, 167019), DoubleExtensionHistoryDivisor);

int HistoryStatDivisor = 32;
TUNE(SetRange(24, 40), HistoryStatDivisor);

int ContinuationWeight3 = 146;
TUNE(SetRange(132, 160), ContinuationWeight3);

int ContinuationWeight4 = 261;
TUNE(SetRange(235, 287), ContinuationWeight4);

int ContinuationWeight5 = 64;
TUNE(SetRange(58, 70), ContinuationWeight5);

int ContinuationWeight6 = 222;
TUNE(SetRange(200, 244), ContinuationWeight6);

TUNE(SetRange(2000, 4500), lmrDivisor[0]);
TUNE(SetRange(2000, 4500), lmrDivisor[1]);
TUNE(SetRange(2000, 4500), lmrDivisor[2]);
TUNE(SetRange(2000, 4500), lmrDivisor[3]);
TUNE(SetRange(2000, 4500), lmrDivisor[4]);
TUNE(SetRange(2000, 4500), lmrDivisor[5]);
TUNE(SetRange(2000, 4500), lmrDivisor[6]);
TUNE(SetRange(2000, 4500), lmrDivisor[7]);
TUNE(SetRange(2000, 4500), lmrDivisor[8]);
TUNE(SetRange(2000, 4500), lmrDivisor[9]);
TUNE(SetRange(2000, 4500), lmrDivisor[10]);
TUNE(SetRange(2000, 4500), lmrDivisor[11]);
TUNE(SetRange(2000, 4500), lmrDivisor[12]);
TUNE(SetRange(2000, 4500), lmrDivisor[13]);
TUNE(SetRange(2000, 4500), lmrDivisor[14]);
TUNE(SetRange(2000, 4500), lmrDivisor[15]);

int CMHCMultipliers[] = {96, 100, 100, 100, 115, 118, 129};
TUNE(SetRange(90, 140), CMHCMultipliers);

constexpr u64 NODES_LIMIT_OUTPUT = 10'000'000;

constexpr int SEARCHEDLIST_CAPACITY = 32;
using SearchedList                  = ValueList<Move, SEARCHEDLIST_CAPACITY>;

// (*Scalers):
// The values with Scaler asterisks have proven non-linear scaling.
// They are optimized to time controls of 180 + 1.8 and longer,
// so changing them or adding conditions that are similar requires
// tests at these types of time controls.

// (*Scaler) All tuned parameters at time controls shorter than
// optimized for require verifications at longer time controls.

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
        ? CorrectionContinuation
            * ((*(ss - 2)->continuationCorrectionHistory)[pos.piece_on(m.to_sq())][m.to_sq()]
               + (*(ss - 4)->continuationCorrectionHistory)[pos.piece_on(m.to_sq())][m.to_sq()])
        : CorrectionFallback;

    return CorrectionPawn * pcv + CorrectionMinor * micv + CorrectionNonPawn * (wnpcv + bnpcv)
         + cntcv;
}

// Add correctionHistory value to raw staticEval and guarantee evaluation
// does not hit the mate range.
Value to_corrected_static_eval(const Value v, const int cv) {
    return std::clamp(v + cv / 131072, VALUE_MATED_IN_MAX_PLY + 1, VALUE_MATE_IN_MAX_PLY - 1);
}

void update_correction_history(const Position& pos,
                               Stack* const    ss,
                               Search::Worker& workerThread,
                               const int       bonus) {
    const Move  m  = (ss - 1)->currentMove;
    const Color us = pos.side_to_move();

    constexpr int nonPawnWeight = 125;
    auto&         shared        = workerThread.sharedHistory;

    shared.pawn_correction_entry(pos)[us].pawn << bonus;
    shared.minor_piece_correction_entry(pos)[us].minor << bonus * CorrectionMinorUpdate / 128;
    shared.nonpawn_correction_entry<WHITE>(pos)[us].nonPawnWhite << bonus * nonPawnWeight / 128;
    shared.nonpawn_correction_entry<BLACK>(pos)[us].nonPawnBlack << bonus * nonPawnWeight / 128;

    if (m.is_ok())
    {
        const Square to = m.to_sq();
        const Piece  pc = pos.piece_on(to);
        (*(ss - 2)->continuationCorrectionHistory)[pc][to]
          << bonus * CorrectionContinuation2Update / 128;
        (*(ss - 4)->continuationCorrectionHistory)[pc][to]
          << bonus * CorrectionContinuation4Update / 128;
    }
}

// Add a small random component to draw evaluations to avoid 3-fold blindness
Value value_draw(usize nodes) { return VALUE_DRAW - 1 + Value(nodes & 0x2); }
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
                      Move            ttMove,
                      bool            PvNode);

// Detect shuffling moves in order to limit search explosions. This code was
// added in #6447 as non-regression, and so its parameters should not be tuned.
bool is_shuffling(Move move, Stack* const ss, const Position& pos) {
    if (pos.capture(move) || pos.rule60_count() < 10)
        return false;
    if (pos.state()->pliesFromNull < 6 || ss->ply < 20)
        return false;
    return move.from_sq() == (ss - 2)->currentMove.to_sq()
        && (ss - 2)->currentMove.from_sq() == (ss - 4)->currentMove.to_sq();
}

}  // namespace

Search::Worker::Worker(SharedState&                    sharedState,
                       std::unique_ptr<ISearchManager> sm,
                       usize                           threadId,
                       usize                           numaThreadId,
                       usize                           numaTotalThreads,
                       NumaReplicatedAccessToken       token) :
    // Unpack the SharedState struct into member variables
    sharedHistory(sharedState.sharedHistories.at(token.get_numa_index())),
    continuationHistory(sharedHistory.continuationHistory()),
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
    // Access once to force lazy initialization, avoiding initialization during search
    (void) (network[numaAccessToken]);
}

void Search::Worker::start_searching() {

    accumulatorStack.reset();

    // Non-main threads go directly to iterative_deepening()
    if (!is_mainthread())
    {
        iterative_deepening();
        return;
    }

    main_manager()->tm.init(limits, rootPos.side_to_move(), rootPos.game_ply(), options,
                            main_manager()->originalTimeAdjust);
    tt.new_search();
    main_manager()->updates.onStart();

    if (rootMoves.empty())
    {
        main_manager()->updates.onUpdateNoMoves({0, {-VALUE_MATE, rootPos}});
        main_manager()->updates.onBestmove(UCIEngine::move(Move::none()), "");
        return;
    }

    // Main thread starts non-main threads, and begins own search
    threads.start_searching();
    bool uciPvSent = iterative_deepening();

    // When we reach the maximum depth, we can arrive here without a raise of
    // threads.stop. However, if we are pondering or in an infinite search,
    // the UCI protocol states that we shouldn't print the best move before the
    // GUI sends a "stop" or "ponderhit" command. We therefore simply wait here
    // until the GUI sends one of those commands.
    while (!threads.stop && (main_manager()->ponder || limits.infinite))
    {}

    // Stop the threads if not already stopped (also raise the stop if "ponderhit"
    // just reset threads.ponder).
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

    // Send PV info if it has changed since last output in iterative_deepening()
    if (!uciPvSent || bestThread != this)
        main_manager()->output_pv(*bestThread, threads, tt, bestThread->rootDepth);

    std::string ponder;
    if (bestThread->rootMoves[0].pv.size() > 1)
        ponder = UCIEngine::move(bestThread->rootMoves[0].pv[1]);

    auto bestmove = UCIEngine::move(bestThread->rootMoves[0].pv[0]);
    main_manager()->updates.onBestmove(bestmove, ponder);
}

// Main iterative deepening loop. It calls search() repeatedly with increasing
// depth until the allocated thinking time has been consumed, the user stops
// the search, or the maximum search depth is reached.
bool Search::Worker::iterative_deepening() {

    SearchManager* mainThread = (is_mainthread() ? main_manager() : nullptr);

    PVMoves pv;

    RootPVMoves lastBestMovePV;
    Depth       lastBestMoveDepth = 0;
    Value       lastBestMoveScore = -VALUE_INFINITE;

    Value  alpha, beta;
    Value  bestValue     = -VALUE_INFINITE;
    Color  us            = rootPos.side_to_move();
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

    usize multiPV = usize(options["MultiPV"]);

    multiPV = std::min(multiPV, rootMoves.size());

    int  searchAgainCounter = 0;
    bool uciPvSent          = false;

    lowPlyHistory.fill(LowPlyHistoryInit);

    for (Color c : {WHITE, BLACK})
        for (int i = 0; i < UINT_16_HISTORY_SIZE; i++)
            mainHistory[c][i] = mainHistory[c][i] * MainHistoryDecay / 1024;

    // Iterative deepening loop until requested to stop or the target depth is reached
    while (rootDepth + 1 < MAX_PLY && !threads.stop
           && !(limits.depth && mainThread && rootDepth >= limits.depth))
    {
        rootDepth++;

        // Age out PV variability metric and signal the start of a new iteration
        if (mainThread)
        {
            totBestMoveChanges /= 2;
            uciPvSent = false;
        }

        // Save the last iteration's scores before the first PV line is searched and
        // all the move scores except the (new) PV are set to -VALUE_INFINITE.
        for (usize i = 0; i < rootMoves.size(); ++i)
        {
            rootMoves[i].previousScore      = rootMoves[i].score;
            rootMoves[i].previousPV         = rootMoves[i].pv;
            rootMoves[i].previousScoreExact = i < multiPV;
        }

        usize pvFirst = 0;
        pvLast        = rootMoves.size();

        if (!threads.increaseDepth)
            searchAgainCounter++;

        // MultiPV loop: we perform a full root search for each PV line
        for (pvIdx = 0; pvIdx < multiPV; ++pvIdx)
        {
            lastIterationIdxPV = rootMoves[pvIdx].previousPV;

            // Reset UCI info selDepth for each depth and each PV line
            selDepth = 0;

            // Reset aspiration window starting size
            delta =
              10 + threadIdx % 8 + std::abs(rootMoves[pvIdx].meanSquaredScore) / AspirationDivisor;
            Value avg = rootMoves[pvIdx].averageScore;
            alpha     = std::max(avg - delta, -VALUE_INFINITE);
            beta      = std::min(avg + delta, VALUE_INFINITE);

            // Adjust optimism based on root move's averageScore
            optimism[us]  = OptimismWeight * avg / (std::abs(avg) + OptimismOffset);
            optimism[~us] = -optimism[us];

            // Start with a small aspiration window and, in the case of a fail
            // high/low, enlarge the window progressively.
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
                // excessive output that could hang GUIs like Fritz 19, only start
                // at nodes > 10M (rather than depth N, which can be reached quickly).
                if (mainThread && multiPV == 1 && (bestValue <= alpha || bestValue >= beta)
                    && nodes > NODES_LIMIT_OUTPUT)
                    main_manager()->output_pv(*this, threads, tt, rootDepth);

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

                delta += AspirationGrowth * delta / 128;

                assert(alpha >= -VALUE_INFINITE && beta <= VALUE_INFINITE);
            }

            if (threads.stop && pvIdx)
            {
                // In multiPV analysis we do not let aborted searches spoil
                // mated-in/TB loss scores from a completed search in an earlier
                // PV line. Hence we guard against an aborted pvIdx line overtaking
                // pvIdx - 1 when pvIdx - 1 is a proven loss. Moreover, we do not
                // trust an exact loss score from an aborted search.
                if ((is_loss(rootMoves[pvIdx - 1].score) && rootMoves[pvIdx] < rootMoves[pvIdx - 1])
                    || rootMoves[pvIdx].is_exact_loss())
                {
                    // If previousScore is exact and worse than pvIdx - 1, we
                    // can safely use it. If it is equal, we make sure it cannot
                    // overtake pvIdx - 1.
                    if (rootMoves[pvIdx].previousScore != -VALUE_INFINITE
                        && rootMoves[pvIdx].previousScoreExact
                        && rootMoves[pvIdx].previousScore <= rootMoves[pvIdx - 1].score)
                    {
                        rootMoves[pvIdx].score = rootMoves[pvIdx].uciScore =
                          rootMoves[pvIdx].previousScore;
                        rootMoves[pvIdx].previousScore = -VALUE_INFINITE;
                        rootMoves[pvIdx].pv            = rootMoves[pvIdx].previousPV;
                        rootMoves[pvIdx].unset_inexact();
                    }

                    // Otherwise, if we can, we cap the score to the best possible, and mark
                    // the score as inexact (also a valid excuse for the incomplete PV).
                    else
                    {
                        if (is_loss(rootMoves[pvIdx - 1].score))
                        {
                            rootMoves[pvIdx].score = rootMoves[pvIdx].uciScore =
                              rootMoves[pvIdx - 1].score;
                            rootMoves[pvIdx].previousScore = -VALUE_INFINITE;
                            rootMoves[pvIdx].pv.resize(1);
                            rootMoves[pvIdx].inexactUpper = true;
                        }
                        else
                            rootMoves[pvIdx].inexactUpper = false;

                        rootMoves[pvIdx].inexactLower = !rootMoves[pvIdx].inexactUpper;
                    }
                }

                // Finally, we mark all loss scores from partially searched moves as inexact.
                for (usize i = pvIdx + 1; i < multiPV; ++i)
                    if (rootMoves[i].is_exact_loss())
                        rootMoves[i].inexactLower = true;
            }

            // Sort the PV lines searched so far and update the GUI
            std::stable_sort(rootMoves.begin() + pvFirst, rootMoves.begin() + pvIdx + 1);

            if (mainThread && !threads.stop && (pvIdx + 1 == multiPV || nodes > NODES_LIMIT_OUTPUT))
            {
                main_manager()->output_pv(*this, threads, tt, rootDepth);
                uciPvSent = (pvIdx + 1 == multiPV);
            }

            if (threads.stop)
                break;
        }

        const bool forgottenMate = lastBestMoveScore != -VALUE_INFINITE
                                && is_decisive(lastBestMoveScore)
                                && (std::abs(rootMoves[0].score) < std::abs(lastBestMoveScore)
                                    || rootMoves[0].is_inexact());

        if (!threads.stop)
        {
            if (lastBestMovePV.empty() || lastBestMovePV[0] != rootMoves[0].pv[0])
                lastBestMoveDepth = rootDepth;

            // Do not replace (shorter) mate scores from a previous iteration
            if (!forgottenMate)
            {
                lastBestMovePV    = rootMoves[0].pv;
                lastBestMoveScore = rootMoves[0].score;
            }
        }

        const bool abortedLossSearch = threads.stop && !pvIdx && rootMoves[0].is_exact_loss();

        // An exact mated-in/TB-loss score from an aborted search cannot be
        // trusted: the loss could be delayed or refuted upon exploring the
        // remaining root-moves. Thus here we roll back to the score from the
        // previous iteration. We do the same if a search has failed to recover
        // a mate score that was found in a previous iteration.
        if (abortedLossSearch || (rootMoves[0].score != -VALUE_INFINITE && forgottenMate))
        {
            // Bring the last best move to the front for best thread selection
            if (!lastBestMovePV.empty())
            {
                Utility::move_to_front(rootMoves, [&lastPV = std::as_const(lastBestMovePV)](
                                                    const auto& rm) { return rm == lastPV[0]; });
                rootMoves[0].score = rootMoves[0].uciScore = lastBestMoveScore;
                rootMoves[0].pv                            = lastBestMovePV;
                rootMoves[0].unset_inexact();

                if (mainThread)
                    uciPvSent = false;
            }
            // For an aborted d1 search we label the loss score as inexact
            else if (abortedLossSearch)
                rootMoves[0].inexactLower = true;
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
            u64 nodesEffort = rootMoves[0].effort * 100000 / std::max(u64(1), u64(nodes));

            double fallingEval = (16.93 + 2.73 * (mainThread->bestPreviousAverageScore - bestValue)
                                  + 0.8 * (mainThread->iterValue[iterIdx] - bestValue))
                               / 100.0;
            fallingEval        = std::clamp(fallingEval, 0.610, 1.860);

            // If the bestMove is stable over several iterations, reduce time accordingly
            timeReduction = std::clamp(
              interpolate(double(rootDepth - lastBestMoveDepth), 8.00, 17.00, 0.670, 1.440), 0.670,
              1.440);

            double reduction =
              (2.100 + mainThread->previousTimeReduction) / (2.480 * timeReduction);

            double bestMoveInstability = 0.960 + 1.630 * totBestMoveChanges / threads.size();

            double highBestMoveEffort = std::clamp(
              interpolate(i64(nodesEffort), i64(78000), i64(94000), 0.960, 0.740), 0.740, 0.960);

            double totalTime = mainThread->tm.optimum() * fallingEval * reduction
                             * bestMoveInstability * highBestMoveEffort;

            if (rootMoves.size() == 1)
                threads.stop = true;

            auto elapsedTime = elapsed();

            // Stop the search if we have exceeded totalTime or maximum time,
            // or if we know that there are no better moves in the analysed line(s).
            if (elapsedTime > std::min(totalTime, double(mainThread->tm.maximum()))
                || rootMoves[multiPV - 1].score >= mate_in(3) || rootMoves[0].score == mated_in(2))
            {
                // If we are allowed to ponder do not stop the search now but
                // keep pondering until the GUI sends "ponderhit" or "stop".
                if (mainThread->ponder)
                    mainThread->stopOnPonderhit = true;
                else
                    threads.stop = true;
            }
            else
                threads.increaseDepth = mainThread->ponder || elapsedTime <= totalTime * 0.26;
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

    // prefetch_key() does not model castling, en passant or promotion exactly.
    // The correction-history prefetches also approximate castling and promotion.
    // For these rare moves the prefetches land on unused lines.
    prefetch(tt.first_entry(pos.prefetch_key(move)));

    bool capture = pos.capture(move);

    if (ss != nullptr)
    {
        const Piece  pc = pos.moved_piece(move);
        const Square to = move.to_sq();

        prefetch(&(*(ss - 1)->continuationCorrectionHistory)[pc][to]);
        prefetch(&(*(ss - 3)->continuationCorrectionHistory)[pc][to]);
    }

    ++nodes;

    Dirties& dirties = accumulatorStack.push();
    pos.do_move(move, st, givesCheck, dirties, &tt, &sharedHistory);

    if (ss != nullptr)
    {
        auto& dirtyPiece = dirties.dirtyPiece;
        ss->currentMove  = move;
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
    mainHistory.fill(-5);
    captureHistory.fill(CaptureHistoryInit);

    // Each thread clears its part of the dynamically-sized shared histories.
    // The constant-size continuation history is initialized by thread 0 of each NUMA node.
    sharedHistory.correctionHistory.clear_range(CorrectionHistoryInit, numaThreadIdx, numaTotal);
    sharedHistory.pawnHistory.clear_range(PawnHistoryInit, numaThreadIdx, numaTotal);

    if (numaThreadIdx == 0)
        for (bool inCheck : {false, true})
            for (StatsType c : {NoCaptures, Captures})
                for (auto& to : continuationHistory[inCheck][c])
                    for (auto& h : to)
                        h.fill(ContinuationHistoryInit);

    ttMoveHistory = 0;

    for (auto& to : continuationCorrectionHistory)
        for (auto& h : to)
            h.fill(7);

    for (usize i = 1; i < reductions.size(); ++i)
        reductions[i] = int(ReductionTableScale / 100.0 * std::log(i));

    refreshTable.clear(network[numaAccessToken]);
}


// Main search function for both PV and non-PV nodes
template<NodeType nodeType>
Value Search::Worker::search(
  Position& pos, Stack* ss, Value alpha, Value beta, Depth depth, const bool cutNode) {

    constexpr bool PvNode   = nodeType != NonPV;
    constexpr bool rootNode = nodeType == Root;
    const bool     allNode  = !(PvNode || cutNode);
    const bool     seekMate = rootDepth >= 16 && std::abs(rootMoves[pvIdx].score) >= 2000;

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
                || ((ss - 1)->followPV
                    && (static_cast<usize>(ss->ply - 1) < lastIterationIdxPV.size()
                        && (ss - 1)->currentMove == lastIterationIdxPV[ss->ply - 1]));

    // Check for the available remaining time
    if (is_mainthread())
        main_manager()->check_time(*this);

    // Used to send selDepth info to GUI (selDepth counts from 1, ply from 0)
    if (PvNode && selDepth < ss->ply + 1)
        selDepth = ss->ply + 1;

    if (!rootNode)
    {
        // Step 2. Check for aborted search or repetition
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
        // because we will never beat the current alpha. Equal and opposite logic applies
        // when being mated. In either case, return a fail-high score.
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

    // Hindsight adjustment of reductions based on static evaluation difference
    if (priorReduction >= 3 && !opponentWorsening)
        depth++;
    if (priorReduction >= 2 && depth >= 2
        && ss->staticEval + (ss - 1)->staticEval > HindsightMargin)
        depth--;

    // Step 6. At non-PV nodes we check for an early TT cutoff. Note that we
    //         always check the validity of the TT value because of access races.
    if (!PvNode && !excludedMove && ttData.depth > depth - (ttData.value <= beta)
        && is_valid(ttData.value)
        && (ttData.bound & (ttData.value >= beta ? BOUND_LOWER : BOUND_UPPER))
        && (cutNode == (ttData.value >= beta) || depth > 5))
    {
        // If the ttMove is quiet, update move sorting heuristics on TT hit
        if (ttData.move && ttData.value >= beta)
        {
            // Bonus for a quiet ttMove that fails high
            if (!ttCapture)
                update_quiet_histories(pos, ss, *this, ttData.move,
                                       std::min(TtQuietDepth * depth, TtQuietLimit));

            // Extra penalty for early quiet moves of the previous ply
            if (prevSq != SQ_NONE && (ss - 1)->moveCount < 3 && !priorCapture)
                update_continuation_histories(ss - 1, pos.piece_on(prevSq), prevSq,
                                              -TtQuietPenalty);
        }

        // Partial workaround for the graph history interaction problem.
        // For high rule60 counts don't produce transposition table cutoffs.
        if (pos.rule60_count() < 116)
        {
            if (depth >= 7 && ttData.move && pos.pseudo_legal(ttData.move) && pos.legal(ttData.move)
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
    }  // No cutoff, but why? Compare the aspiration window to the inexact bound
    else if (!PvNode && !excludedMove && ttData.depth > depth - (ttData.value <= beta)
             && is_valid(ttData.value) && ttData.bound != BOUND_EXACT
             && ttData.bound & (ttData.value >= beta ? BOUND_UPPER : BOUND_LOWER) && depth > 5)
    {
        // If such a mismatch is the only reason cutoff failed, the TT entry is now useless
        ttWriter.penalize(1);
    }

    if (ss->inCheck)
        goto moves_loop;

    // Use static evaluation difference to improve quiet move ordering
    if (((ss - 1)->currentMove).is_ok() && !(ss - 1)->inCheck && !priorCapture)
    {
        int evalDiff =
          std::clamp(-int((ss - 1)->staticEval + ss->staticEval), EvalDiffLower, EvalDiffUpper)
          + EvalDiffOffset;
        mainHistory[~us][((ss - 1)->currentMove).raw()] << evalDiff * EvalDiffMainWeight;
        if (!ttHit && type_of(pos.piece_on(prevSq)) != PAWN)
            sharedHistory.pawn_entry(pos)[pos.piece_on(prevSq)][prevSq] << evalDiff * 12;
    }

    // Step 8. Razoring
    // If eval is really low, skip search entirely and return the qsearch value
    if (!PvNode && eval < alpha - RazorDepth * depth * depth)
        return qsearch<NonPV>(pos, ss, alpha, beta);

    // Step 9. Futility pruning: child node
    // The depth condition is important for mate finding. It should NOT be tuned.
    if (!ss->ttPv && depth < (seekMate ? 6 : 15) && eval >= beta && (!ttData.move || ttCapture)
        && !is_loss(beta) && !is_win(eval))
    {
        Value futilityMult = std::min(FutilityBase + depth * 4, FutilityLimit);
        futilityMult -= 33 * !ss->ttHit;

        Value futilityMargin =
          futilityMult * depth
          - (FutilityImproving * improving + FutilityWorsening * opponentWorsening) * futilityMult
              / 1024
          + std::abs(correctionValue) / FutilityCorrectionDivisor;

        if (eval - futilityMargin >= beta)
            return ((1024 - FutilityEvalWeight) * beta + FutilityEvalWeight * eval) / 1024;
    }

    // Step 10. Null move search with verification search
    if (cutNode
        && ss->staticEval
             >= beta - NmpDepthMargin * depth - NmpImprovingMargin * improving + NmpBaseMargin
        && !excludedMove && pos.major_material(us) && ss->ply >= nmpMinPly && beta >= -2000)
    {
        assert((ss - 1)->currentMove != Move::null());

        // Null move dynamic reduction based on depth
        Depth R = 8 + depth / 3 + std::max((ss->staticEval - beta) / NmpEvalDivisor, 0);
        do_null_move(pos, st, ss);

        Value nullValue = -search<NonPV>(pos, ss + 1, -beta, -beta + 1, depth - R, false);

        undo_null_move(pos);

        // Do not return unproven mate
        if (nullValue >= beta && !is_win(nullValue))
        {
            if (nmpMinPly || depth < 15)
                return nullValue;

            // Recursive verification is not allowed
            assert(!nmpMinPly);

            // Do verification search at high depths, with null move pruning
            // disabled until ply exceeds nmpMinPly.
            nmpMinPly = ss->ply + 3 * (depth - R) / 4;

            Value v = search<NonPV>(pos, ss, beta - 1, beta, depth - R, false);

            nmpMinPly = 0;

            if (v >= beta)
                return nullValue;
        }
    }

    improving |= ss->staticEval >= beta;

    // Step 11. Internal iterative reductions
    // At sufficient depth, reduce depth for PV/Cut nodes without a TTMove.
    // (*Scaler) Making IIR more aggressive scales poorly.
    if (!ss->followPV && !allNode && depth >= 6 && !ttData.move)
        depth--;

    // Step 12. ProbCut
    // If we have a good enough capture (or queen promotion) and a reduced search
    // returns a value much above beta, we can (almost) safely prune the previous move.
    probCutBeta = beta + ProbCutMargin - ProbCutImproving * improving;
    if (depth >= 3 && !is_decisive(beta) && !(is_valid(ttData.value) && ttData.value < probCutBeta))
    {
        assert(probCutBeta < VALUE_INFINITE && probCutBeta > beta);

        MovePicker mp(pos, ttData.move, probCutBeta - ss->staticEval, &captureHistory);
        Depth      probCutDepth = depth - (improving ? 5 : 3);

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

    // Step 13. A small ProbCut idea
    probCutBeta = beta + 470;
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

    // Step 14. Loop through all pseudo-legal moves until no moves remain
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

        // Increase reduction for ttPv nodes
        // (*Scaler) Larger values scale well.
        if (ss->ttPv)
            r += LmrTtPvInitial;

        // Step 15. Pruning at shallow depths.
        // Depth conditions are important for mate finding.
        if (!rootNode && pos.major_material(us) && !is_loss(bestValue))
        {
            // Skip quiet moves if movecount exceeds our threshold
            if (moveCount >= (3 + depth * depth) / (2 - improving))
                mp.skip_quiet_moves();

            // Reduced depth of the next LMR search
            int lmrDepth = newDepth - r / 1005;

            if (capture || givesCheck)
            {
                Piece capturedPiece = pos.piece_on(move.to_sq());
                int   captHist = captureHistory[movedPiece][move.to_sq()][type_of(capturedPiece)];

                // Futility pruning for captures
                if (!givesCheck && lmrDepth < 19)
                {
                    Value futilityValue =
                      ss->staticEval + CaptureFutilityBase + CaptureFutilityDepth * lmrDepth
                      + PieceValue[capturedPiece] + CaptureFutilityHistory * captHist / 1024;

                    if (futilityValue <= alpha)
                        continue;
                }

                // SEE based pruning for captures and checks.
                int margin = CaptureSeeDepth * depth + captHist * 34 / 1024;
                if (!pos.see_ge(move, -margin))
                    continue;
            }
            else if (!ss->followPV || !PvNode)
            {
                int dIndex  = std::min(int(depth), int(lmrDivisor.size())) - 1;
                int history = (*contHist[0])[movedPiece][move.to_sq()]
                            + (*contHist[1])[movedPiece][move.to_sq()]
                            + sharedHistory.pawn_entry(pos)[movedPiece][move.to_sq()];

                // Continuation history based pruning
                if (history < -QuietHistoryPruning * depth)
                    continue;

                history += QuietHistoryWeight * mainHistory[us][move.raw()] / 32;

                // (*Scaler): Generally, lower divisors scale well
                lmrDepth += history / lmrDivisor[dIndex];

                Value futilityValue = ss->staticEval + QuietFutilityDepth * lmrDepth
                                    + QuietFutilityEval * (ss->staticEval > alpha)
                                    + QuietFutilityBase;

                // Futility pruning: parent node
                // (*Scaler): Generally, more frequent futility pruning scales well
                if (!ss->inCheck && lmrDepth < 10 && futilityValue <= alpha)
                {
                    if (bestValue <= futilityValue && !is_decisive(bestValue)
                        && !is_win(futilityValue))
                        bestValue = futilityValue;
                    continue;
                }

                lmrDepth = std::max(lmrDepth, 0);

                // Prune moves with negative SEE
                if (!pos.see_ge(move, -QuietSeeDepth * lmrDepth * lmrDepth))
                    continue;
            }
        }

        // Step 16. Singular Extensions
        //
        // We check for "only moves": if one move fails high on (alpha, beta) but all
        // others fail low on (alpha-s, beta-s), then that move is singular. Singular
        // moves are extended to better estimate the result of the (putatively)
        // forced line. If it's non-singular, we may do some pruning or reduction.
        //
        // Recursive excluded search is avoided. The `excludedMove` mechanism
        // was historically a hack and remains a bit fragile.
        //
        // (*Scaler) Generally, higher singularBeta (i.e closer to ttValue)
        // and lower extension margins scale well.
        if (!rootNode && move == ttData.move && !excludedMove && depth >= 5 + ss->ttPv
            && is_valid(ttData.value) && !is_decisive(ttData.value) && (ttData.bound & BOUND_LOWER)
            && ttData.depth >= depth - 3 && !is_shuffling(move, ss, pos) && !seekMate)
        {
            Value singularBeta  = ttData.value
                                - (SingularMargin + SingularTtPvMargin * (ss->ttPv && !PvNode))
                                    * depth / SingularDivisor;
            Depth singularDepth = newDepth / 2;

            ss->excludedMove = move;
            value = search<NonPV>(pos, ss, singularBeta - 1, singularBeta, singularDepth, cutNode);
            ss->excludedMove = Move::none();

            if (value < singularBeta)
            {
                int corrValAdj = std::abs(correctionValue) / SingularCorrectionDivisor;
                int doubleMargin =
                  DoubleExtensionBase + DoubleExtensionPv * PvNode
                  - DoubleExtensionQuiet * !ttCapture - corrValAdj
                  - DoubleExtensionHistory * ttMoveHistory / DoubleExtensionHistoryDivisor
                  - (ss->ply > rootDepth) * DoubleExtensionPly;
                int tripleMargin = TripleExtensionBase + TripleExtensionPv * PvNode
                                 - 263 * !ttCapture + TripleExtensionTtPv * ss->ttPv - corrValAdj
                                 - (ss->ply > rootDepth) * TripleExtensionPly;

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
                ttMoveHistory << -MultiCutHistoryBase - MultiCutHistoryDepth * depth;

                if (!ss->inCheck && value > ss->staticEval)
                {
                    const int bonus = std::clamp(
                      int(value - ss->staticEval) * singularDepth * MultiCutCorrectionWeight / 1024,
                      -CORRECTION_HISTORY_LIMIT / 4, CORRECTION_HISTORY_LIMIT / 4);
                    update_correction_history(pos, ss, *this, bonus);
                }

                return value;
            }

            // Negative extensions
            // If other moves failed high over (ttValue - margin) without the
            // ttMove on a reduced search, but we cannot do multi-cut because
            // (ttValue - margin) is lower than the original beta, we do not know
            // if the ttMove is singular or can do a multi-cut, so we reduce the
            // ttMove in favor of other moves based on some conditions:

            // If the ttMove is assumed to fail high over current beta or
            // if we are on a cutNode
            else if (ttData.value >= beta || cutNode)
                extension = -3;
        }

        u64 nodeCount = rootNode ? u64(nodes) : 0;

        // Step 17. Make the move
        do_move(pos, move, st, givesCheck, ss);

        // Add extension to new depth
        newDepth += extension;

        // Step 18. Compute and apply late moves reductions/extensions (LMR)

        // Decrease reduction for PvNodes (*Scaler)
        if (ss->ttPv)
            r -= LmrTtPvBase + PvNode * LmrPvWeight + (ttData.value > alpha) * LmrTtValueWeight
               + (ttData.depth >= depth) * (LmrTtDepthWeight + cutNode * LmrTtCutWeight);

        // Base reduction offset to compensate for other tweaks
        r += LmrBase;

        r -= moveCount * LmrMoveCount;
        r -= std::abs(correctionValue) / LmrCorrectionDivisor;

        // Increase reduction for cut nodes
        if (cutNode)
            r += LmrCutBase + LmrCutNoTt * !ttData.move;

        // Increase reduction if ttMove is a capture
        if (ttCapture)
            r += LmrTtCapture;

        // Increase reduction if next ply has a lot of fail high
        if ((ss + 1)->cutoffCnt > 1)
            r +=
              LmrCutoffBase + LmrCutoffExtra * ((ss + 1)->cutoffCnt > 2) + LmrCutoffAll * allNode;

        // For first picked move (ttMove) reduce reduction
        else if (move == ttData.move)
            r -= LmrFirstMove;

        if (capture)
            ss->statScore = LmrCaptureValue * int(PieceValue[pos.captured_piece()]) / 128
                          + captureHistory[movedPiece][move.to_sq()][type_of(pos.captured_piece())];
        else
            ss->statScore = (LmrMainHistory * mainHistory[us][move.raw()]
                             + LmrContinuationHistory * (*contHist[0])[movedPiece][move.to_sq()]
                             + LmrContinuation2History * (*contHist[1])[movedPiece][move.to_sq()])
                          / 1024;

        // Decrease/increase reduction for moves with a good/bad history
        r -= ss->statScore * LmrHistoryWeight / 8192;

        if (!capture && !is_decisive(alpha))
            r += LmrEvalWeight * std::clamp(alpha - eval, LmrEvalLower, LmrEvalUpper);

        // Scale up reductions for expected ALL nodes
        if (allNode)
            r += r * LmrAllWeight / (256 * depth + LmrAllOffset);

        // Apply the computed LMR
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
                const bool doDeeperSearch    = d < newDepth && value > bestValue + LmrDeeperMargin;
                const bool doShallowerSearch = value < bestValue + LmrShallowerMargin;

                newDepth += doDeeperSearch - doShallowerSearch;

                if (newDepth > d)
                    value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha, newDepth, !cutNode);

                // Post LMR continuation history updates
                update_continuation_histories(ss, movedPiece, move.to_sq(), LmrContinuationBonus);
            }
        }

        // Step 19. Full-depth search when LMR is skipped
        else if (!PvNode || moveCount > 1)
        {
            // Increase reduction if ttMove is not present
            if (!ttData.move)
                r += LmrNoTt;

            // If expected reduction is high, we reduce search depth here
            value = -search<NonPV>(pos, ss + 1, -(alpha + 1), -alpha,
                                   newDepth - (r > LmrSkipThreshold1)
                                     - (r > LmrSkipThreshold2 && newDepth > 2),
                                   !cutNode);
        }

        // Step 20. For PV nodes only, do a full PV search on the first move
        // or after a fail high, otherwise let the parent node fail low with
        // value <= alpha and try another move.
        if (PvNode && (moveCount == 1 || value > alpha))
        {
            (ss + 1)->pv = &pv;
            (ss + 1)->pv->clear();

            // Extend move from transposition table if we are about to dive
            // into qsearch. Decisive score handling improves mate finding
            // and retrograde analysis.
            if (move == ttData.move
                && ((is_valid(ttData.value) && is_decisive(ttData.value) && ttData.depth > 0)
                    || ttData.depth > 1))
                newDepth = std::max(newDepth, 1);

            value = -search<PV>(pos, ss + 1, -beta, -alpha, newDepth, false);
        }

        // Step 21. Undo move
        undo_move(pos, move);

        assert(value > -VALUE_INFINITE && value < VALUE_INFINITE);

        // Step 22. Check for a new best move
        // If a stop occurred, the value of the search cannot be trusted,
        // and we return immediately without updating the best move,
        // principal variation or transposition table.
        if (threads.stop.load(std::memory_order_relaxed))
            return VALUE_ZERO;

        if (rootNode)
        {
            RootMove& rm = *std::find(rootMoves.begin(), rootMoves.end(), move);

            rm.effort += nodes - nodeCount;

            u64 N      = nodes - nodeCount;
            u64 E_prev = std::max(u64(1), rm.effort - N);

            // Exponential moving average parameters for root move
            constexpr u64 Scale          = 32;
            constexpr u64 ChiNumerator   = 3;
            constexpr u64 ChiDenominator = 2;   // Chi = 3/2 = 1.5
            constexpr u64 MinWeight      = 12;  // 37.5% minimum weight
            constexpr u64 MaxWeight      = 24;  // 75% maximum weight

            u64 w     = std::clamp((Scale * N * ChiDenominator)
                                     / (N * ChiDenominator + ChiNumerator * E_prev),
                                   MinWeight, MaxWeight);
            u64 w_mss = std::min(w, u64(16));
            i64 v2    = i64(value) * std::abs(value);

            if (rm.averageScore == -VALUE_INFINITE)
                rm.averageScore = value;
            else
                rm.averageScore = Value((value * w + rm.averageScore * (Scale - w)) / Scale);

            if (rm.meanSquaredScore == -VALUE_INFINITE * VALUE_INFINITE)
                rm.meanSquaredScore = value * std::abs(value);
            else
                rm.meanSquaredScore =
                  Value((v2 * w_mss + int64_t(rm.meanSquaredScore) * (Scale - w_mss)) / Scale);

            // PV move or new best move?
            if (moveCount == 1 || value > alpha)
            {
                rm.score = rm.uciScore = value;
                rm.selDepth            = selDepth;
                rm.unset_inexact();

                if (value >= beta)
                {
                    rm.inexactLower = true;
                    rm.uciScore     = beta;
                }
                else if (value <= alpha)
                {
                    rm.inexactUpper = true;
                    rm.uciScore     = alpha;
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
                // All other moves but the PV are set to the lowest value: this
                // is not a problem when sorting because the sort is stable and the
                // move position in the list is preserved -- just the PV is pushed up.
                rm.score = -VALUE_INFINITE;
        }

        // If we have an alternative move equal in value to the current bestmove,
        // we sometimes promote it to bestmove by pretending it just exceeds
        // alpha (but not beta).
        int inc = (value == bestValue && ss->ply + 2 >= rootDepth && (int(nodes) & 14) == 0
                   && !is_win(std::abs(value) + 1));

        if (value + inc > bestValue)
        {
            bestValue = value;

            if (value + inc > alpha)
            {
                bestMove = move;

                // Update PV even in fail-high case
                if (PvNode && !rootNode)
                    ss->pv->update(move, (ss + 1)->pv);

                if (value >= beta)
                {
                    // (*Scaler) Infrequent and small updates scale well
                    ss->cutoffCnt += (extension < 2) || PvNode;
                    assert(value >= beta);  // Fail high
                    break;
                }

                // Reduce other moves if we have found at least one score improvement
                if (depth > 3 && depth < 11 && !is_decisive(value))
                    depth -= 3;

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

    // Step 23. Check for mate and stalemate, otherwise update bestmove/countermove stats

    assert(moveCount || !ss->inCheck || excludedMove || !MoveList<LEGAL>(pos).size());

    // Adjust best value for fail high cases
    if (bestValue >= beta && !is_decisive(bestValue) && !is_decisive(alpha))
        bestValue = (bestValue * depth + beta) / (depth + 1);

    // All legal moves have been searched: if there are no legal moves, it
    // must be a mate or a stalemate (just a fail low score if we are in a
    // singular extension search).
    if (!moveCount)
        bestValue = excludedMove ? alpha : mated_in(ss->ply);

    // If there is a move that produces search value greater than alpha,
    // we update the stats of searched moves.
    else if (bestMove)
    {
        update_all_stats(pos, ss, *this, bestMove, prevSq, quietsSearched, capturesSearched, depth,
                         ttData.move, PvNode);
        if (!PvNode)
            ttMoveHistory << (bestMove == ttData.move ? TtMoveBonus : -TtMoveMalus);
    }

    // Bonus for prior quiet countermove that caused the fail low
    else if (!priorCapture && prevSq != SQ_NONE)
    {
        int bonusScale = -231;
        bonusScale -= (ss - 1)->statScore / 73;
        bonusScale += std::min(62 * depth, 512);
        bonusScale += 152 * ((ss - 1)->moveCount > 13);
        bonusScale += 76 * (!ss->inCheck && bestValue <= ss->staticEval - 166);
        bonusScale += 163 * (!(ss - 1)->inCheck && bestValue <= -(ss - 1)->staticEval - 109);

        bonusScale = std::max(bonusScale, 0);

        // scaledBonus ranges from 0 to roughly 2.3M, overflows happen for
        // multipliers larger than 900
        const int scaledBonus = std::min(148 * depth - 86, 2188) * bonusScale;

        update_continuation_histories(ss - 1, pos.piece_on(prevSq), prevSq,
                                      scaledBonus * 192 / 16384);

        mainHistory[~us][((ss - 1)->currentMove).raw()] << scaledBonus * 216 / 32768;

        if (type_of(pos.piece_on(prevSq)) != PAWN)
            sharedHistory.pawn_entry(pos)[pos.piece_on(prevSq)][prevSq] << scaledBonus * 244 / 8192;
    }

    // Bonus for prior capture countermove that caused the fail low
    else if (priorCapture && prevSq != SQ_NONE)
    {
        Piece capturedPiece = pos.captured_piece();
        assert(capturedPiece != NO_PIECE);
        captureHistory[pos.piece_on(prevSq)][prevSq][type_of(capturedPiece)]
          << CaptureCountermoveBonus;
    }

    if (PvNode)
        bestValue = std::min(bestValue, maxValue);

    // If no good move is found and the previous position was ttPv, then the previous
    // opponent move is probably good and the new position is added to the search tree.
    if (bestValue <= alpha)
        ss->ttPv = ss->ttPv || (ss - 1)->ttPv;

    // Step 24. Write gathered information in transposition table. Note that the
    // static evaluation is saved as it was before correction history.
    if (!excludedMove && !(rootNode && pvIdx))
        ttWriter.write(posKey, value_to_tt(bestValue, ss->ply), ss->ttPv,
                       bestValue >= beta    ? BOUND_LOWER
                       : PvNode && bestMove ? BOUND_EXACT
                                            : BOUND_UPPER,
                       moveCount != 0 ? depth : std::min(MAX_PLY - 1, depth + 6), bestMove,
                       unadjustedStaticEval, tt.generation());

    // Adjust correction history if the best move is not a capture and
    // the error direction matches whether we are above/below bounds.
    if (!ss->inCheck && !(bestMove && pos.capture(bestMove))
        && (bestValue > ss->staticEval) == bool(bestMove))
    {
        auto bonus =
          std::clamp(int(bestValue - ss->staticEval) * depth * (bestMove ? 12 : 17) / 128,
                     -CORRECTION_HISTORY_LIMIT / 4, CORRECTION_HISTORY_LIMIT / 4);
        update_correction_history(pos, ss, *this, CorrectionUpdateWeight * bonus / 1024);
    }

    // The search is now complete
    assert(-VALUE_INFINITE < bestValue && bestValue < VALUE_INFINITE);
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

    ss->ttHit    = ttHit;
    ttData.move  = ttHit ? ttData.move : Move::none();
    ttData.value = ttHit ? value_from_tt(ttData.value, ss->ply, pos.rule60_count()) : VALUE_NONE;
    pvHit        = ttHit && ttData.is_pv;

    // At non-PV nodes we check for an early TT cutoff
    if (!PvNode && ttData.depth >= DEPTH_QS && is_valid(ttData.value)
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
                bestValue =
                  (QsearchStandPatWeight * bestValue + (1024 - QsearchStandPatWeight) * beta)
                  / 1024;

            if (!ss->ttHit)
                ttWriter.write(posKey, VALUE_NONE, false, BOUND_LOWER, DEPTH_UNSEARCHED,
                               Move::none(), unadjustedStaticEval, tt.generation());
            return bestValue;
        }

        if (bestValue > alpha)
            alpha = bestValue;

        futilityBase = ss->staticEval + QsearchFutility;
    }

    const PieceToHistory* contHist[] = {(ss - 1)->continuationHistory};

    Square prevSq = ((ss - 1)->currentMove).is_ok() ? ((ss - 1)->currentMove).to_sq() : SQ_NONE;

    // Initialize a MovePicker object for the current position, and prepare
    // to search the moves. We presently use two stages of move generator in
    // quiescence search: captures, or evasions only when in check.
    MovePicker mp(pos, ttData.move, DEPTH_QS, &mainHistory, &lowPlyHistory, &captureHistory,
                  contHist, &sharedHistory, ss->ply);

    // Step 5. Loop through all pseudo-legal moves until no moves remain
    // or a beta cutoff occurs.
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

                // If static exchange evaluation is low enough, we can prune
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
            if (!pos.see_ge(move, -106))
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

                // Update pv even in fail-high case
                if (PvNode)
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
        bestValue =
          (QsearchFailHighWeight * bestValue + (1024 - QsearchFailHighWeight) * beta) / 1024;

    // Step 10. Save gathered info in transposition table. The static evaluation
    // is saved as it was before adjustment by correction history.
    ttWriter.write(posKey, value_to_tt(bestValue, ss->ply), pvHit,
                   bestValue >= beta ? BOUND_LOWER : BOUND_UPPER, DEPTH_QS, bestMove,
                   unadjustedStaticEval, tt.generation());

    // The search is now complete
    assert(-VALUE_INFINITE < bestValue && bestValue < VALUE_INFINITE);
    return bestValue;
}

int Search::Worker::reduction(bool i, Depth d, int mn, int delta) const {
    int reductionScale = reductions[d] * reductions[mn];
    return reductionScale - delta * LmrWindowWeight / rootDelta
         + !i * reductionScale * LmrNotImproving / 512 + LmrOffset;
}

// elapsed() returns the time elapsed since the search started. If the
// 'nodestime' option is enabled, it will return the count of nodes searched
// instead. This function is called to check whether the search should be
// stopped based on predefined thresholds like time limits or nodes searched.
TimePoint Search::Worker::elapsed() const {
    return main_manager()->tm.elapsed([this]() { return threads.nodes_searched(); });
}


// Evaluate the current position of the game tree, from the point of view of
// the side to move.
Value Search::Worker::evaluate(const Position& pos) {
    return Eval::evaluate(network[numaAccessToken], pos, accumulatorStack, refreshTable,
                          optimism[pos.side_to_move()]);
}

namespace {

// Adjusts a mate score from "plies to mate from the root" to
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
        // Downgrade a potentially false mate score
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
                      Move            ttMove,
                      bool            PvNode) {

    CapturePieceToHistory& captureHistory = workerThread.captureHistory;
    Piece                  movedPiece     = pos.moved_piece(bestMove);
    PieceType              capturedPiece;

    int bonus = std::min(HistoryBonusDepth * depth - HistoryBonusOffset, HistoryBonusLimit)
              + HistoryTtBonus * (bestMove == ttMove) + (ss - 1)->statScore / HistoryStatDivisor;
    int malus = std::min(HistoryMalusDepth * depth - HistoryMalusOffset, HistoryMalusLimit);

    if (!PvNode)
        // Important: don't remove the cast to a 64-bit number else the multiplication
        // can overflow on 32-bit platforms which would change the bench signature
        bonus += int(bonus * u64(quietsSearched.size() + capturesSearched.size()) / 256);

    if (!pos.capture(bestMove))
    {
        update_quiet_histories(pos, ss, workerThread, bestMove, bonus * QuietBonusWeight / 1024);

        // Decrease stats for all non-best quiet moves
        int actualMalus = malus * QuietMalusWeight / 1024;
        for (Move move : quietsSearched)
        {
            actualMalus = actualMalus * QuietMalusDecay / 1024;
            update_quiet_histories(pos, ss, workerThread, move, -actualMalus);
        }
    }
    else
    {
        // Increase stats for the best move in case it was a capture move
        capturedPiece = type_of(pos.piece_on(bestMove.to_sq()));
        captureHistory[movedPiece][bestMove.to_sq()][capturedPiece]
          << bonus * CaptureBonusWeight / 1024;
    }

    // Extra penalty for a quiet early move that was not a TT move in
    // previous ply when it gets refuted.
    if (prevSq != SQ_NONE && ((ss - 1)->moveCount == 1 + (ss - 1)->ttHit) && !pos.captured_piece())
        update_continuation_histories(ss - 1, pos.piece_on(prevSq), prevSq,
                                      -malus * ContinuationMalusWeight / 1024);

    // Decrease stats for all non-best capture moves
    for (Move move : capturesSearched)
    {
        movedPiece    = pos.moved_piece(move);
        capturedPiece = type_of(pos.piece_on(move.to_sq()));
        captureHistory[movedPiece][move.to_sq()][capturedPiece]
          << -malus * CaptureMalusWeight / 1024;
    }
}


// Updates the continuation histories for the move pairs formed by
// the current move and the moves played in previous plies.
void update_continuation_histories(Stack* ss, Piece pc, Square to, int bonus) {
    const std::array<ConthistBonus, 6> conthist_bonuses = {{{1, 538},
                                                            {2, 319},
                                                            {3, ContinuationWeight3},
                                                            {4, ContinuationWeight4},
                                                            {5, ContinuationWeight5},
                                                            {6, ContinuationWeight6}}};

    // Multipliers for positive history consistency

    int positiveCount = 0;

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
            historyEntry << bonus * weight * multiplier / 65536 + ContinuationFirstBonus * (i < 2);
        }
    }
}

// Updates move sorting heuristics

void update_quiet_histories(
  const Position& pos, Stack* ss, Search::Worker& workerThread, Move move, int bonus) {

    Color us = pos.side_to_move();
    workerThread.mainHistory[us][move.raw()] << bonus;  // Untuned to prevent duplicate effort

    if (ss->ply < LOW_PLY_HISTORY_SIZE)
        workerThread.lowPlyHistory[ss->ply][move.raw()] << bonus * LowPlyBonusWeight / 1024;

    update_continuation_histories(ss, pos.moved_piece(move), move.to_sq(),
                                  bonus * ContinuationBonusWeight / 1024);

    workerThread.sharedHistory.pawn_entry(pos)[pos.moved_piece(move)][move.to_sq()]
      << bonus * (bonus > PawnBonusThreshold ? PawnBonusWeight : PawnMalusWeight) / 1024;
}
}

// Function to detect when we are out of available time and stop the search,
// and to print debug info.
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

void SearchManager::output_pv(Search::Worker&           worker,
                              const ThreadPool&         threads,
                              const TranspositionTable& tt,
                              Depth                     depth) {

    const auto nodes     = threads.nodes_searched();
    auto&      rootMoves = worker.rootMoves;
    auto&      pos       = worker.rootPos;
    usize      multiPV   = std::min(usize(worker.options["MultiPV"]), rootMoves.size());

    for (usize i = 0; i < multiPV; ++i)
    {
        bool usePreviousScore = rootMoves[i].score == -VALUE_INFINITE;

        if (depth == 1 && usePreviousScore && i > 0)
            continue;

        Depth d = usePreviousScore ? std::max(1, depth - 1) : depth;
        Value v = usePreviousScore ? rootMoves[i].previousScore : rootMoves[i].uciScore;

        if (v == -VALUE_INFINITE)
            v = VALUE_ZERO;

        std::string pv;
        for (Move m : usePreviousScore ? rootMoves[i].previousPV : rootMoves[i].pv)
            pv += UCIEngine::move(m) + " ";

        // Remove last whitespace
        if (!pv.empty())
            pv.pop_back();

        auto wdl = worker.options["UCI_ShowWDL"] ? UCIEngine::wdl(v, pos) : "";

        // Scores cannot be both exact and inexact
        assert(!(rootMoves[i].inexactLower && rootMoves[i].inexactUpper));
        auto bound = rootMoves[i].inexactLower ? "lowerbound"
                   : rootMoves[i].inexactUpper ? "upperbound"
                                               : "";

        InfoFull info;

        info.depth    = d;
        info.selDepth = rootMoves[i].selDepth;
        info.multiPV  = i + 1;
        info.score    = {v, pos};
        info.wdl      = wdl;

        // Previous scores are exact, even though their flags may say otherwise
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
// We try hard to have a ponder move to return to the GUI, otherwise
// in case of 'ponder on' we have nothing to think about.
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
