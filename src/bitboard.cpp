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
#include <bitset>

#include "bitboard.h"
#include "misc.h"
#include "magics.h"
#include <set>
#include <iostream>

namespace Stockfish {

uint8_t PopCnt16[1 << 16];
uint8_t SquareDistance[SQUARE_NB][SQUARE_NB];

Bitboard SquareBB[SQUARE_NB];
Bitboard LineBB[SQUARE_NB][SQUARE_NB];
Bitboard BetweenBB[SQUARE_NB][SQUARE_NB];
Bitboard PseudoAttacks[PIECE_TYPE_NB][SQUARE_NB];
Bitboard PawnAttacks[COLOR_NB][SQUARE_NB];
Bitboard PawnAttacksTo[COLOR_NB][SQUARE_NB];

Magic RookMagics[SQUARE_NB];
Magic CannonMagics[SQUARE_NB];
Magic BishopMagics[SQUARE_NB];
Magic KnightMagics[SQUARE_NB];
Magic KnightToMagics[SQUARE_NB];

namespace {
  // TODO: 象的攻击位棋盘的大小需要扩展，需要重新计算，计算的代码查看
  // https://github.com/PikaCat-OuO/Pikafish/blob/138caa98fdbfe18069f16ea99195cffb535e3191/src/bitboard.cpp#L216-L295
  // https://github.com/ianfab/Fairy-Stockfish/commit/7b2c51be61f7fab5b945b16adf28a3f5167725c0
  // 同时需要为象重新生成magic数字，放到magics.h里面
  Bitboard RookTable    [0x108000];  // To store rook attacks
  Bitboard CannonTable  [0x108000];  // To store cannon attacks
  Bitboard BishopTable  [0x2B0];     // To store bishop attacks
  Bitboard KnightTable  [0x380];     // To store knight attacks
  Bitboard KnightToTable[0x3E0];     // To store by knight attacks

  const std::set<Direction> KnightDirections { 2 * SOUTH + WEST, 2 * SOUTH + EAST, SOUTH + 2 * WEST, SOUTH + 2 * EAST,
                                               NORTH + 2 * WEST, NORTH + 2 * EAST, 2 * NORTH + WEST, 2 * NORTH + EAST };
  const std::set<Direction> BishopDirections { 2 * NORTH_EAST, 2 * SOUTH_EAST, 2 * SOUTH_WEST, 2 * NORTH_WEST };


  template <PieceType pt>
  void init_magics(Bitboard table[], Magic magics[], const Bitboard magicsInit[]);


  // template <PieceType pt>
  // void init_magics(Bitboard table[], Magic magics[]);


  template <PieceType pt>
  Bitboard lame_leaper_path(Direction d, Square s);

}

/// safe_destination() returns the bitboard of target square for the given step
/// from the given square. If the step is off the board, returns empty bitboard.

inline Bitboard safe_destination(Square s, int step) {
    Square to = Square(s + step);
    return is_ok(to) && distance(s, to) <= 2 ? square_bb(to) : Bitboard(0);
}


/// Bitboards::pretty() returns an ASCII representation of a bitboard suitable
/// to be printed to standard output. Useful for debugging.

std::string Bitboards::pretty(Bitboard b) {

  std::string s = "+---+---+---+---+---+---+---+---+---+\n";

  for (Rank r = RANK_9; r >= RANK_0; --r)
  {
      for (File f = FILE_A; f <= FILE_I; ++f)
          s += b & make_square(f, r) ? "| X " : "|   ";

      s += "| " + std::to_string(r) + "\n+---+---+---+---+---+---+---+---+---+\n";
  }
  s += "  a   b   c   d   e   f   g   h   i\n";

  return s;
}


/// Bitboards::init() initializes various bitboard tables. It is called at
/// startup and relies on global objects to be already zero-initialized.

void Bitboards::init() {

  for (unsigned i = 0; i < (1 << 16); ++i)
      PopCnt16[i] = uint8_t(std::bitset<16>(i).count());

  for (Square s = SQ_A0; s <= SQ_I9; ++s)
      SquareBB[s] = (Bitboard(1ULL) << s);

  for (Square s1 = SQ_A0; s1 <= SQ_I9; ++s1)
      for (Square s2 = SQ_A0; s2 <= SQ_I9; ++s2)
          SquareDistance[s1][s2] = std::max(distance<File>(s1, s2), distance<Rank>(s1, s2));
  //init_magics<   BISHOP>(BishopTable, BishopMagics);

  init_magics<     ROOK>(    RookTable,     RookMagics,     RookMagicsInit);
  init_magics<   CANNON>(  CannonTable,   CannonMagics,     RookMagicsInit);
  init_magics<   BISHOP>(  BishopTable,   BishopMagics,   BishopMagicsInit);
  init_magics<   KNIGHT>(  KnightTable,   KnightMagics,   KnightMagicsInit);
  init_magics<KNIGHT_TO>(KnightToTable, KnightToMagics, KnightToMagicsInit);

  for (Square s1 = SQ_A0; s1 <= SQ_I9; ++s1)
  {
      PawnAttacks[WHITE][s1] = pawn_attacks_bb<WHITE>(s1);
      PawnAttacks[BLACK][s1] = pawn_attacks_bb<BLACK>(s1);

      PawnAttacksTo[WHITE][s1] = pawn_attacks_to_bb<WHITE>(s1);
      PawnAttacksTo[BLACK][s1] = pawn_attacks_to_bb<BLACK>(s1);

      PseudoAttacks[  ROOK][s1] = attacks_bb<  ROOK>(s1, 0);
      PseudoAttacks[BISHOP][s1] = attacks_bb<BISHOP>(s1, 0);
      PseudoAttacks[KNIGHT][s1] = attacks_bb<KNIGHT>(s1, 0);


      for (int step : { NORTH_WEST, NORTH_EAST, SOUTH_WEST, SOUTH_EAST }){
          PseudoAttacks[ADVISOR][s1] |= safe_destination(s1, step);
      }

      // Only generate pseudo attacks in the palace squares for king and advisor
      if (Palace & s1) {
          for (int step : { NORTH, SOUTH, WEST, EAST } )
              PseudoAttacks[KING][s1] |= safe_destination(s1, step);
          PseudoAttacks[KING][s1] &= Palace;

          for (int step : { NORTH_WEST, NORTH_EAST, SOUTH_WEST, SOUTH_EAST })
              PseudoAttacks[ADVISOR_B][s1] |= safe_destination(s1, step);
          PseudoAttacks[ADVISOR_B][s1] &= Palace;
      }

      for (Square s2 = SQ_A0; s2 <= SQ_I9; ++s2)
      {
          if (PseudoAttacks[ROOK][s1] & s2)
          {
              LineBB[s1][s2]    = (attacks_bb(ROOK, s1, 0) & attacks_bb(ROOK, s2, 0)) | s1 | s2;
              BetweenBB[s1][s2] = (attacks_bb(ROOK, s1, square_bb(s2)) & attacks_bb(ROOK, s2, square_bb(s1)));
          }

          if (PseudoAttacks[KNIGHT][s1] & s2)
              BetweenBB[s1][s2] |= lame_leaper_path<KNIGHT_TO>(Direction(s2 - s1), s1);


          // TODO: 上面考虑了马的BetweenBB，还需要额外考虑象的BetweenBB，象本来的位置[s1]和走到的位置[s2]中间的象眼和象的位置s1的位棋盘
          // 可以参考between_bb函数的定义 Between[s1][s2] |= ....;
          if (PseudoAttacks[BISHOP][s1] & s2)
              BetweenBB[s1][s2] |= lame_leaper_path<BISHOP>(Direction(s2 - s1), s1);

          BetweenBB[s1][s2] |= s2;
      }
  }
}

namespace {

  template <PieceType pt>
  Bitboard sliding_attack(Square sq, Bitboard occupied) {
    assert(pt == ROOK || pt == CANNON);
    Bitboard attack = 0;

    for (auto const& d : { NORTH, SOUTH, EAST, WEST } )
    {
      bool hurdle = false;
      for (Square s = sq + d; is_ok(s) && distance(s - d, s) == 1; s += d)
      {
        if (pt == ROOK || hurdle)
          attack |= s;

        if (occupied & s)
        {
          if (pt == CANNON && !hurdle)
            hurdle = true;
          else
            break;
        }
      }
    }

    return attack;
  }

  template <PieceType pt>
  Bitboard lame_leaper_path(Direction d, Square s) {
    Bitboard b = 0;
    Square to = s + d;
    if (!is_ok(to) || distance(s, to) >= 4)
        return b;

    // If piece type is by knight attacks, swap the source and destination square
    if (pt == KNIGHT_TO) {
      std::swap(s, to);
      d = -d;
    }

    Direction dr = d > 0 ? NORTH : SOUTH;
    Direction df = (std::abs(d % NORTH) < NORTH / 2 ? d % NORTH : -(d % NORTH)) < 0 ? WEST : EAST;

    int diff = std::abs(file_of(to) - file_of(s)) - std::abs(rank_of(to) - rank_of(s));
    if (diff > 0)
      s += df;
    else if (diff < 0)
      s += dr;
    else
      s += df + dr;

    b |= s;
    return b;
  }

  template <PieceType pt>
  Bitboard lame_leaper_path(Square s) {
    Bitboard b = 0;
    for (const auto& d : pt == BISHOP ? BishopDirections : KnightDirections)
      b |= lame_leaper_path<pt>(d, s);
    return b;
  }

  template <PieceType pt>
  Bitboard lame_leaper_attack(Square s, Bitboard occupied) {
    Bitboard b = 0;
    for (const auto& d : pt == BISHOP  ? BishopDirections : KnightDirections)
    {
      Square to = s + d;
      if (is_ok(to) && distance(s, to) < 4 && !(lame_leaper_path<pt>(d, s) & occupied))
        b |= to;
    }
    return b;
  }


  // init_magics() computes all rook and bishop attacks at startup. Magic
  // bitboards are used to look up attacks of sliding pieces. As a reference see
  // www.chessprogramming.org/Magic_Bitboards. In particular, here we use the so
  // called "fancy" approach.

  template <PieceType pt>
  void init_magics(Bitboard table[], Magic magics[], const Bitboard magicsInit[]) {

    Bitboard edges, b;
    uint64_t size = 0;

    for (Square s = SQ_A0; s <= SQ_I9; ++s)
    {
        // Board edges are not considered in the relevant occupancies
        edges = ((Rank0BB | Rank9BB) & ~rank_bb(s)) | ((FileABB | FileIBB) & ~file_bb(s));

        // Given a square 's', the mask is the bitboard of sliding attacks from
        // 's' computed on an empty board. The index must be big enough to contain
        // all the attacks for each possible subset of the mask and so is 2 power
        // the number of 1s of the mask.
        Magic& m = magics[s];
        m.mask = pt == ROOK   ? sliding_attack<pt>(s, 0) :
                 pt == CANNON ? RookMagics[s].mask       :
                                lame_leaper_path<pt>(s)  ;
        if (pt != KNIGHT_TO)
          m.mask &= ~edges;

        if (HasPext)
          m.shift = popcount(uint64_t(m.mask));
        else
          m.shift = 128 - popcount(m.mask);

        m.magic = magicsInit[s];

        // Set the offset for the attacks table of the square. We have individual
        // table sizes for each square with "Fancy Magic Bitboards".
        m.attacks = s == SQ_A0 ? table : magics[s - 1].attacks + size;

        // Use Carry-Rippler trick to enumerate all subsets of masks[s] and
        // store the corresponding attack bitboard in m.attacks.
        b = size = 0;
        do {
            m.attacks[m.index(b)] = pt == ROOK || pt == CANNON ? sliding_attack<pt>(s, b) :
                                                                 lame_leaper_attack<pt>(s, b);

            size++;
            b = (b - m.mask) & m.mask;
        } while (b);
    }
  }


  // init_magics() computes all rook and bishop attacks at startup. Magic
  // bitboards are used to look up attacks of sliding pieces. As a reference see
  // www.chessprogramming.org/Magic_Bitboards. In particular, here we use the so
  // called "fancy" approach.

  // template <PieceType pt>
  // void init_magics(Bitboard table[], Magic magics[]) {

  //     // Optimal PRNG seeds to pick the correct magics in the shortest time
  //     int seeds[][RANK_NB] = { { 734, 10316, 55013, 32803, 12281, 15100, 16645, 255, 346, 89123 },
  //                              { 734, 10316, 55013, 32803, 12281, 15100, 16645, 255, 346, 89123 } };

  //     Bitboard* occupancy = new Bitboard[0x8000], * reference = new Bitboard[0x8000], edges, b;
  //     int* epoch = new int[0x8000]{ }, cnt = 0, size = 0;

  //     for (Square s = SQ_A0; s <= SQ_I9; ++s)
  //     {
  //         // Board edges are not considered in the relevant occupancies
  //         edges = ((Rank0BB | Rank9BB) & ~rank_bb(s)) | ((FileABB | FileIBB) & ~file_bb(s));

  //         // Given a square 's', the mask is the bitboard of sliding attacks from
  //         // 's' computed on an empty board. The index must be big enough to contain
  //         // all the attacks for each possible subset of the mask and so is 2 power
  //         // the number of 1s of the mask.
  //         Magic& m = magics[s];
  //         m.mask = ~edges & (pt == ROOK || pt == CANNON ? sliding_attack<pt>(s, 0) :
  //             lame_leaper_path<pt>(s));
          
  //         if (HasPext)
  //             m.shift = popcount(uint64_t(m.mask));
  //         else
  //             m.shift = 128 - popcount(m.mask);

  //         // Set the offset for the attacks table of the square. We have individual
  //         // table sizes for each square with "Fancy Magic Bitboards".
  //         m.attacks = s == SQ_A0 ? table : magics[s - 1].attacks + size;

  //         // Use Carry-Rippler trick to enumerate all subsets of masks[s] and
  //         // store the corresponding sliding attack bitboard in reference[].
  //         b = size = 0;
  //         do {
  //             occupancy[size] = b;
  //             reference[size] = pt == ROOK || pt == CANNON ? sliding_attack<pt>(s, b) :
  //                 lame_leaper_attack<pt>(s, b);

  //             if (HasPext)
  //                 m.attacks[pext(b, m.mask)] = reference[size];

  //             size++;
  //             b = (b - m.mask) & m.mask;
  //         } while (b);

  //         if (HasPext)
  //             continue;

  //         PRNG rng(seeds[Is64Bit][rank_of(s)]);

  //         // Find a magic for square 's' picking up an (almost) random number
  //         // until we find the one that passes the verification test.
  //         for (int i = 0; i < size; )
  //         {
  //             for (m.magic = 0; popcount((m.magic * m.mask) >> 119) < 7; )
  //                 m.magic = (rng.sparse_rand<Bitboard>() << 64) ^ rng.sparse_rand<Bitboard>();

  //             // A good magic must map every possible occupancy to an index that
  //             // looks up the correct sliding attack in the attacks[s] database.
  //             // Note that we build up the database for square 's' as a side
  //             // effect of verifying the magic. Keep track of the attempt count
  //             // and save it in epoch[], little speed-up trick to avoid resetting
  //             // m.attacks[] after every failed attempt.
  //             for (++cnt, i = 0; i < size; ++i)
  //             {
  //                 unsigned idx = m.index(occupancy[i]);

  //                 if (epoch[idx] < cnt)
  //                 {
  //                     epoch[idx] = cnt;
  //                     m.attacks[idx] = reference[i];
  //                 }
  //                 else if (m.attacks[idx] != reference[i])
  //                     break;
  //             }
  //         }
  //         std::cout << "      B(0x" << std::hex << std::uppercase << uint64_t(m.magic >> 64) << ", 0x" << uint64_t(m.magic) << "),"
  //             << std::nouppercase << std::dec << std::endl;
  //     }
  //     std::cout << "Size: 0x" << std::hex << magics[SQ_I9].attacks - magics[SQ_A1].attacks + size << std::endl;
  //     delete[] occupancy;
  //     delete[] reference;
  //     delete[] epoch;
  // }

}

} // namespace Stockfish
