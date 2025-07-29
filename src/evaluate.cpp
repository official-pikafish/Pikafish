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
#if USE_NNUEEVAL 
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
#else
      return;
#endif
  }
}

namespace Trace {

    enum Tracing { NO_TRACE, TRACE };

    enum Term { // The first 8 entries are reserved for PieceType
        MATERIAL = 8, IMBALANCE, PAIR, MOBILITY, THREAT, PASSED, SPACE, WINNABLE, TOTAL, TERM_NB
    };

    Score scores[TERM_NB][COLOR_NB];

    double to_cp(Value v) { return double(v) / PawnValueEg; }

    static void add(int idx, Color c, Score s) {
        scores[idx][c] = s;
    }

    static void add(int idx, Score w, Score b = SCORE_ZERO) {
        scores[idx][WHITE] = w;
        scores[idx][BLACK] = b;
    }

    static std::ostream& operator<<(std::ostream& os, Score s) {
        os << std::setw(5) << to_cp(mg_value(s)) << " "
            << std::setw(5) << to_cp(eg_value(s));
        return os;
    }

    static std::ostream& operator<<(std::ostream& os, Term t) {

        if (t == MATERIAL || t == IMBALANCE || t == WINNABLE || t == TOTAL)
            os << " ----  ----" << " | " << " ----  ----";
        else
            os << scores[t][WHITE] << " | " << scores[t][BLACK];

        os << " | " << scores[t][WHITE] - scores[t][BLACK] << " |\n";
        return os;
    }
}

using namespace Trace;

namespace {

  // Threshold for space evaluation
  Value SpaceThreshold    =  Value(8943);

#define S(mg, eg) make_score(mg, eg)

  Score ConnectedPieces[PIECE_TYPE_NB] = {
    S(0, 0), S(-16, 106), S(-4, 23), S(185, -3), S(-33, 93), S(-60, -8), S(84, -144)
  };

  Score ThreatByPawn[PIECE_TYPE_NB] = {
    S(0, 0), S(-11, 2), S(39, -26), S(-57, -4), S(-47, 64), S(20, 72), S(-8, -61)
  };

  Score ThreatByAdvisor[PIECE_TYPE_NB] = {
    S(0, 0), S(23, 4), S(11, 39), S(-66, 143), S(-35, -49), S(70, 78), S(26, 67)
  };

  Score ThreatByBishop[PIECE_TYPE_NB] = {
    S(0, 0), S(-6, 44), S(15, -56), S(13, -19), S(54, -82), S(-38, 14), S(-46, 47)
  };

  // ThreatByMinor/ByRook[attacked PieceType] contains bonuses according to
  // which piece type attacks which one. Attacks on lesser pieces which are
  // pawn-defended are not considered.
  Score ThreatByMinor[PIECE_TYPE_NB] = {
    S(0, 0), S(-169, -94), S(-58, -109), S(73, -110), S(37, -89), S(-36, 319), S(-115, -96)
  };

  Score ThreatByRook[PIECE_TYPE_NB] = {
    S(0, 0), S(59, -229), S(-118, 45), S(-50, 84), S(56, 46), S(-13, -152), S(91, 22)
  }; 

  // Assorted bonuses and penalties
  Score Hanging = S(119, 9);
  Score RestrictedPiece     = S(-14, 109);
  Score ThreatByKing        = S(-135, -37);
  Score ThreatByPawnPush    = S(-96, 277);
  Score ThreatBySafePawn    = S(11, 39);
  Score ProtectedByPawn     = S(51, -166);
  Score RestrictedDarkPawn  = S(87, -54);

#undef S

  // Evaluation class computes and stores attacks tables and other working data
  template<Tracing T>
  class Evaluation {

  public:
    Evaluation() = delete;
    explicit Evaluation(const Position& p) : pos(p) {}
    Evaluation& operator=(const Evaluation&) = delete;
    Value value();

  private:
    template<Color Us> void initialize();
    template<Color Us, PieceType Pt> Score pieces();
    template<Color Us> Score pair();
    template<Color Us> Score threats() const;
    template<Color Us> Score space() const;
    Value winnable(Score score) const;

    const Position& pos;
    Material::Entry* me;

    // attackedBy[color][piece type] is a bitboard representing all squares
    // attacked by a given color and piece type. Special "piece types" which
    // is also calculated is ALL_PIECES.
    Bitboard attackedBy[COLOR_NB][PIECE_TYPE_NB];

    // attackedBy2[color] are the squares attacked by at least 2 units of a given
    // color, including x-rays. But diagonal x-rays through pawns are not computed.
    Bitboard attackedBy2[COLOR_NB];

    Bitboard DarkPieces[COLOR_NB][PIECE_TYPE_NB];
  };


  // Evaluation::initialize() computes king and pawn attacks, and the king ring
  // bitboard for a given color. This is done at the beginning of the evaluation.

  template<Tracing T> template<Color Us>
  void Evaluation<T>::initialize() {

    constexpr Color     Them = ~Us;
    const Square ksq = pos.square<KING>(Us);
    constexpr Bitboard LowRanks = (Us == WHITE ? Rank0BB | Rank1BB : Rank8BB | Rank9BB); 

   
    // Initialize attackedBy[] for king and pawns
    attackedBy[Us][KING] = attacks_bb<KING>(ksq);
    attackedBy[Us][PAWN] = pawn_attacks_bb<Us>(pos.pieces(Us, PAWN));
    attackedBy[Us][ALL_PIECES] = attackedBy[Us][KING] | attackedBy[Us][PAWN];
    attackedBy2[Us] = attackedBy[Us][KING] & attackedBy[Us][PAWN];
    memset(DarkPieces, 0, sizeof DarkPieces);
    for (PieceType i = ROOK; i <= BISHOP; ++i) {
        Bitboard b;
        b = pos.pieces(Us, i);
        while (b) {
            Square s = pop_lsb(b);
            if (pos.piece_on(s) >= BW_ROOK) {
                if (pos.piece_on(s) >= BB_ROOK)
                    DarkPieces[BLACK][i] |= square_bb(s);
                else
                    DarkPieces[WHITE][i] |= square_bb(s);
            }
        }
        DarkPieces[WHITE][ALL_PIECES] |= DarkPieces[WHITE][i];
        DarkPieces[BLACK][ALL_PIECES] |= DarkPieces[BLACK][i];
    }
    // Find our pawns that are on the first two ranks
    Bitboard b0 = pos.pieces(Us, PAWN) & LowRanks;

  }


  // Evaluation::pieces() scores pieces of a given color and type

  template<Tracing T> template<Color Us, PieceType Pt>
  Score Evaluation<T>::pieces() {

      Bitboard b1 = pos.pieces(Us, Pt);
      Bitboard b;

      attackedBy[Us][Pt] = 0;

      while (b1)
      {
          Square s = pop_lsb(b1);

          // Find attacked squares, including x-ray attacks for bishops and rooks
          b = attacks_bb<Pt>(s, pos.pieces());

          if (pos.blockers_for_king(Us) & s)
              b &= line_bb(pos.square<KING>(Us), s);

          attackedBy2[Us] |= attackedBy[Us][ALL_PIECES] & b;
          attackedBy[Us][Pt] |= b;
          attackedBy[Us][ALL_PIECES] |= b;
      }
      return SCORE_ZERO;
  }

  template<Tracing T> template<Color Us>
  Score Evaluation<T>::pair() {
      Score score = SCORE_ZERO;
      for (PieceType i = ROOK; i <= BISHOP; ++i) {
          Bitboard b = attackedBy[Us][i];
          b &= pos.pieces(Us, i);
          if (popcount(b) >= 2) {
              score += ConnectedPieces[i];
          }
      }
      if constexpr (T)
          Trace::add(PAIR, Us, score);
      return score;
  }

  // Evaluation::threats() assigns bonuses according to the types of the
  // attacking and the attacked pieces.

  template<Tracing T> template<Color Us>
  Score Evaluation<T>::threats() const {

    constexpr Color     Them     = ~Us;
    constexpr Direction Up       = (Us == WHITE ? NORTH : SOUTH);
    constexpr Bitboard  TRank3BB = (Us == WHITE ? Rank3BB : Rank6BB);

    Bitboard b, weak, defended, nonPawnEnemies, stronglyProtected, safe;
    Score score = SCORE_ZERO;

    // Non-pawn enemies
    nonPawnEnemies = pos.pieces(Them) & ~pos.pieces(PAWN);

    // Squares strongly protected by the enemy, either because they defend the
    // square with a pawn, or because they defend the square twice and we don't.
    stronglyProtected =  attackedBy[Them][PAWN]
                       | (attackedBy2[Them] & ~attackedBy2[Us]);

    // Non-pawn enemies, strongly protected
    defended = nonPawnEnemies & stronglyProtected;

    // Enemies not strongly protected and under our attack
    weak = pos.pieces(Them) & ~stronglyProtected & attackedBy[Us][ALL_PIECES];

    // Bonus according to the kind of attacking pieces
    if (defended | weak)
    {
        b = (defended | weak) & (attackedBy[Us][KNIGHT] | attackedBy[Us][BISHOP]);
        while (b)
            score += ThreatByMinor[type_of(pos.piece_on(pop_lsb(b)))];

        b = weak & attackedBy[Us][ROOK];
        while (b)
            score += ThreatByRook[type_of(pos.piece_on(pop_lsb(b)))];

        if (weak & attackedBy[Us][KING])
            score += ThreatByKing;

        b =  ~attackedBy[Them][ALL_PIECES]
           | (nonPawnEnemies & attackedBy2[Us]);
        score += Hanging * popcount(weak & b);
    }

    // Bonus for restricting their piece moves
    b =   attackedBy[Them][ALL_PIECES]
       & ~stronglyProtected
       &  attackedBy[Us][ALL_PIECES];
    score += RestrictedPiece * popcount(b);

    // Protected or unattacked squares
    safe = ~attackedBy[Them][ALL_PIECES] | attackedBy[Us][ALL_PIECES];

    // Bonus for attacking enemy pieces with our relatively safe pawns
    b = pos.pieces(Us, PAWN) & safe;
    b = pawn_attacks_bb<Us>(b) & nonPawnEnemies;
    score += ThreatBySafePawn * popcount(b);

    // Find squares where our pawns can push on the next move
    b  = shift<Up>(pos.pieces(Us, PAWN)) & ~pos.pieces();
    b |= shift<Up>(b & TRank3BB) & ~pos.pieces();

    // Keep only the squares which are relatively safe
    b &= ~attackedBy[Them][PAWN] & safe;

    // Bonus for safe pawn threats on the next move
    b = pawn_attacks_bb<Us>(b) & nonPawnEnemies;
    score += ThreatByPawnPush * popcount(b);

    b = attackedBy[Us][PAWN];
    b &= DarkPieces[Us][PAWN];

    score += popcount(b) * ProtectedByPawn;

    b = pawn_attacks_bb<Us>(DarkPieces[Us][PAWN]) & attackedBy[Them][PAWN];
    score -= popcount(b) * RestrictedDarkPawn;

    for (PieceType i = ROOK; i <= BISHOP; ++i) {
        b = attackedBy[Us][PAWN] & DarkPieces[Them][i];
        score += popcount(b) * ThreatByPawn[i];

        b = attackedBy[Us][ADVISOR] & DarkPieces[Them][i];
        score += popcount(b) * ThreatByAdvisor[i];

        b = attackedBy[Us][BISHOP] & DarkPieces[Them][i];
        score += popcount(b) * ThreatByBishop[i];
    }

    if constexpr (T)
        Trace::add(THREAT, Us, score);

    return score;
  }


  // Evaluation::space() computes a space evaluation for a given side, aiming to improve game
  // play in the opening. It is based on the number of safe squares on the four central files
  // on ranks 2 to 4. Completely safe squares behind a friendly pawn are counted twice.
  // Finally, the space bonus is multiplied by a weight which decreases according to occupancy.

  template<Tracing T> template<Color Us>
  Score Evaluation<T>::space() const {

    // Early exit if, for example, both queens or 6 minor pieces have been exchanged
    if (pos.material_sum() < SpaceThreshold)
        return SCORE_ZERO;

    constexpr Color Them     = ~Us;
    constexpr Direction Down = (Us == WHITE ? SOUTH : NORTH);
    constexpr Bitboard SpaceMask =
      Us == WHITE ? (FileDBB | FileEBB | FileFBB) & (Rank2BB | Rank3BB | Rank4BB)
                  : (FileDBB | FileEBB | FileFBB) & (Rank7BB | Rank6BB | Rank5BB);

    // Find the available squares for our pieces inside the area defined by SpaceMask
    Bitboard safe =   SpaceMask
                   & ~pos.pieces(Us, PAWN)
                   & ~pawn_attacks_bb<Them>(pos.pieces(Us, PAWN));

    // Find all squares which are at most three squares behind some friendly pawn
    Bitboard behind = pos.pieces(Us, PAWN);
    behind |= shift<Down>(behind);
    behind |= shift<Down+Down>(behind);

    // Compute space score based on the number of safe squares and number of our pieces
    // increased with number of total blocked pawns in position.
    int bonus = popcount(safe) + popcount(behind & safe & ~attackedBy[Them][ALL_PIECES]);
    int weight = pos.count<ALL_PIECES>(Us) - 3;
    Score score = make_score(bonus * weight * weight / 16, 0);

    if constexpr (T)
        Trace::add(SPACE, Us, score);

    return score;
  }


  // Evaluation::winnable() adjusts the midgame and endgame score components, based on
  // the known attacking/defending status of the players. The final value is derived
  // by interpolation from the midgame and endgame values.

  template<Tracing T>
  Value Evaluation<T>::winnable(Score score) const {
    return (mg_value(score) * (pos.count<ALL_PIECES>() * 1000 / 32) + eg_value(score) * (1000 - pos.count<ALL_PIECES>() * 1000 / 32)) / 1000;
  }


  // Evaluation::value() is the main function of the class. It computes the various
  // parts of the evaluation and returns the value of the position from the point
  // of view of the side to move.

  template<Tracing T>
  Value Evaluation<T>::value() {

    // Probe the material hash table
    me = Material::probe(pos);

    Score score = pos.psq_score() + me->imbalance();

    if constexpr (T) {
        Trace::add(MATERIAL, score);
        Trace::add(IMBALANCE, me->imbalance());
    }

    // Main evaluation begins here
    initialize<WHITE>();
    initialize<BLACK>();

    score += pieces<WHITE, KNIGHT>() - pieces<BLACK, KNIGHT>()
        + pieces<WHITE, BISHOP>() - pieces<BLACK, BISHOP>()
        + pieces<WHITE, ROOK>() - pieces<BLACK, ROOK>()
        + pieces<WHITE, ADVISOR>() - pieces<BLACK, ADVISOR>()
        + pieces<WHITE, CANNON>() - pieces<BLACK, CANNON>();

    score += pair<WHITE>() - pair<BLACK>();

    score +=  threats<WHITE>() - threats<BLACK>()
            + space<  WHITE>() - space<  BLACK>();

    if constexpr (T)
        Trace::add(TOTAL, score);

    // Derive single value from mg and eg parts of score
    Value v = winnable(score);

    // Side to move point of view
    v = (pos.side_to_move() == WHITE ? v : -v);

    return v;
  }

} // namespace Eval

/// evaluate() is the evaluator for the outer world. It returns a static
/// evaluation of the position from the point of view of the side to move.

Value Eval::evaluate(const Position& pos, int* complexity) {
#if USE_NNUEEVAL     
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
#else
    if (complexity)
        *complexity = 0;
    Value v = Evaluation<NO_TRACE>(pos).value();
    return v;
#endif

}

// format_cp_compact() converts a Value into (centi)pawns and writes it in a buffer.
// The buffer must have capacity for at least 5 chars.
static void format_cp_compact(Value v, char* buffer) {

    buffer[0] = (v < 0 ? '-' : v > 0 ? '+' : ' ');

    int cp = std::abs(100 * v / PawnValueEg);
    if (cp >= 10000)
    {
        buffer[1] = '0' + cp / 10000; cp %= 10000;
        buffer[2] = '0' + cp / 1000; cp %= 1000;
        buffer[3] = '0' + cp / 100;
        buffer[4] = ' ';
    }
    else if (cp >= 1000)
    {
        buffer[1] = '0' + cp / 1000; cp %= 1000;
        buffer[2] = '0' + cp / 100; cp %= 100;
        buffer[3] = '.';
        buffer[4] = '0' + cp / 10;
    }
    else
    {
        buffer[1] = '0' + cp / 100; cp %= 100;
        buffer[2] = '.';
        buffer[3] = '0' + cp / 10; cp %= 10;
        buffer[4] = '0' + cp / 1;
    }
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

  char board[3 * RANK_NB + 1][8 * FILE_NB + 2];
  std::memset(board, ' ', sizeof(board));
  for (int row = 0; row < 3 * RANK_NB + 1; ++row)
      board[row][8 * FILE_NB + 1] = '\0';


  // A lambda to output one box of the board
  auto writeSquare = [&board](File file, Rank rank, Piece pc, Value value) {
      const std::string PieceToChar(" RACPNBK racpnbk XXXXXX  xxxxxx");

      const int x = ((int)file) * 8;
      const int y = (RANK_9 - (int)rank) * 3;
      for (int i = 1; i < 8; ++i)
          board[y][x + i] = board[y + 3][x + i] = '-';
      for (int i = 1; i < 3; ++i)
          board[y + i][x] = board[y + i][x + 8] = '|';
      board[y][x] = board[y][x + 8] = board[y + 3][x + 8] = board[y + 3][x] = '+';
      if (pc != NO_PIECE)
          board[y + 1][x + 4] = PieceToChar[pc];
      if (value != VALUE_NONE)
          format_cp_compact(value, &board[y + 2][x + 2]);
  };

  // We estimate the value of each piece by doing a differential evaluation from
  // the current base eval, simulating the removal of the piece from its square.
  Value base = evaluate(pos);
  base = pos.side_to_move() == WHITE ? base : -base;

  for (File f = FILE_A; f <= FILE_I; ++f)
      for (Rank r = RANK_0; r <= RANK_9; ++r)
      {
          Square sq = make_square(f, r);
          Piece pc = pos.piece_on(sq);
          Value v = VALUE_NONE;

          if (pc != NO_PIECE && type_of(pc) != KING)
          {
              auto st = pos.state();

              pos.remove_piece(sq);
              Score score = pos.psq_score();
              Value eval = (mg_value(score) * (pos.count<ALL_PIECES>() * 1000 / 32) + eg_value(score) * (1000 - pos.count<ALL_PIECES>() * 1000 / 32)) / 1000;
              eval = pos.side_to_move() == WHITE ? eval : -eval;
              v = base - eval;

              pos.put_piece(pc, sq);
          }

          writeSquare(f, r, pc, v);
      }

  ss << "PSQT derived piece values:\n";
  for (int row = 0; row < 3 * RANK_NB + 1; ++row)
      ss << board[row] << '\n';
  ss << '\n';

  v = Evaluation<TRACE>(pos).value();

  ss << std::showpoint << std::noshowpos << std::fixed << std::setprecision(2)
     << " Contributing terms for the classical eval:\n"
     << "+------------+-------------+-------------+-------------+\n"
     << "|    Term    |    White    |    Black    |    Total    |\n"
     << "|            |   MG    EG  |   MG    EG  |   MG    EG  |\n"
     << "+------------+-------------+-------------+-------------+\n"
     << "|   Material | " << Term(MATERIAL)
     << "|  Imbalance | " << Term(IMBALANCE)
     << "|       Pair | " << Term(PAIR)
     << "|      Pawns | " << Term(PAWN)
     << "|    Knights | " << Term(KNIGHT)
     << "|    Bishops | " << Term(BISHOP)
     << "|      Rooks | " << Term(ROOK)
     << "|   Mobility | " << Term(MOBILITY)
     << "|King safety | " << Term(KING)
     << "|    Threats | " << Term(THREAT)
     << "|     Passed | " << Term(PASSED)
     << "|      Space | " << Term(SPACE)
     << "|   Winnable | " << Term(WINNABLE)
     << "+------------+-------------+-------------+-------------+\n"
     << "|      Total | " << Term(TOTAL)
     << "+------------+-------------+-------------+-------------+\n";

  ss << std::showpoint << std::showpos << std::fixed << std::setprecision(2) << std::setw(15);

  v = evaluate(pos);
  v = pos.side_to_move() == WHITE ? v : -v;
  ss << "Final evaluation       " << to_cp(v) << " (white side) [with scaled NNUE, optimism, ...]\n";

  return ss.str();
}

} // namespace Stockfish
