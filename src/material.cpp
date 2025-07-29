/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2023 The Stockfish developers (see AUTHORS file)
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

#include <cassert>
#include <cstring>   // For std::memset

#include "material.h"
#include "thread.h"

using namespace std;

namespace Stockfish {

    namespace {
#define S(mg, eg) make_score(mg, eg)

        // Polynomial material imbalance parameters

        // One Score parameter for each pair (our piece, another of our pieces)
        Score QuadraticOurs[][13] = {
            // OUR PIECE 
            // None         ROOK      ADVISOR    CANNON        PAWN       KNIGHT      BISHOP    ROOK_DB    ADVISOR_DB    CANNON_DB     PAWN_DB    KNIGHT_DB   BISHOP_DB
{ S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S(-209,  -8), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 68, 142), S( 76,  56), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 16, -39), S(-25,  44), S(-25, 111), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S(-28,  42), S(113,  51), S(107,  16), S( 42,  84), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 58, 119), S( 86,  39), S( 36, -135), S( 29, -54), S( 35,  10), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 46, -113), S(134, 110), S(  7, 129), S(103, -18), S(109, -78), S( 69,  14), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 71, 130), S(-37, -59), S(-41,  -7), S(-27, -29), S(-60, 122), S(-161, 148), S( -6, 111), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 90, -84), S( 51, -11), S( 59, -78), S(  3, -111), S(192,  56), S( 75,  69), S( 27, -180), S( 13, -78), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 35, -25), S( 60,  45), S( 81, 120), S( 35, -71), S(106,  -7), S(  2, -37), S( 36, -28), S(-122, -74), S(-16, -17), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 29,  18), S( 28,  61), S( 75,  61), S(-128, -89), S( 98, -50), S(-10, -168), S( 21, -60), S(-97, 147), S( 44, -16), S( -1,   6), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 52, -54), S( 60,  36), S(-97, 200), S( 61, -65), S(-21, -117), S( 67, -66), S(-97, -70), S(  1, -46), S(-27, -97), S( -7, -118), S(  4, -17), S(  0,   0), },

{ S(  0,   0), S( 82,  82), S( 41, 128), S(-140, -77), S( -8, 145), S(-124,  19), S(-19, -93), S(  1,  -3), S( 72,  89), S(-27,  -3), S(-22, -10), S(161,  60), S(-97, -43), },
        };

        // One Score parameter for each pair (our piece, their piece)
        Score QuadraticTheirs[][13] = {
            // THEIR PIECE 
            // None         ROOK      ADVISOR    CANNON        PAWN       KNIGHT      BISHOP    ROOK_DB    ADVISOR_DB    CANNON_DB     PAWN_DB    KNIGHT_DB   BISHOP_DB
{ S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S(118, -18), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 23, -74), S( 30,  24), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S(-12,  46), S(-27,  56), S( -4, -76), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S(-105,  32), S(-72, -69), S(-157, -119), S( 30, -98), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S(-11, -92), S(-90,   1), S(-25, -44), S(  0,  27), S( 15, -158), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 65, -147), S( 50, -15), S(-83,  19), S(-13, -149), S(114, -13), S(100, -42), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S(150,  16), S( 51,  33), S(-80, -68), S(221,  34), S(-31,  12), S(-98, -72), S(-25,  56), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 35,  44), S( 53,  15), S(-154,  36), S( 67,  78), S( 54, -95), S( 50,  36), S(-13, 182), S(-126,  23), S(  0,   0), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 95, -126), S(  8,  41), S(103, 110), S(-151, 220), S(-165, 167), S( -2,  75), S(-67,  10), S(-22, -32), S(133, 110), S(  0,   0), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S(-62, -108), S(132, -157), S( 30,  36), S(-210,  24), S(-26,   1), S(-29, 134), S(-131,   1), S( 25, 137), S(-63,  97), S(-90, -62), S(  0,   0), S(  0,   0), },

{ S(  0,   0), S( 79,  48), S( 25, -33), S(-76, 140), S(-25,  36), S(-71, -13), S( 75, -31), S( 44,  16), S( 44, -22), S(-93, 225), S(-116, 109), S(  3, -127), S(  0,   0), },

{ S(  0,   0), S( 18, -99), S( 94, 134), S(107, -137), S(-87, -57), S( 21,  30), S( 35,   0), S(-95, -57), S(  1,  15), S( 11,  -5), S( 87,  72), S( 50, -63), S(  3,   7), },
        };
        
#undef S


        /// imbalance() calculates the imbalance by comparing the piece count of each
        /// piece type for both colors.

        template<Color Us>
        Score imbalance(const int pieceCount[][13]) {

            constexpr Color Them = ~Us;

            Score bonus = SCORE_ZERO;

            // Second-degree polynomial material imbalance, by Tord Romstad
            for(int pt1 = 1; pt1 < 13; ++pt1)
            {
                if (!pieceCount[Us][pt1])
                    continue;

                int v = QuadraticOurs[pt1][pt1] * pieceCount[Us][pt1];

                for (int pt2 = 1; pt2 < pt1; ++pt2)
                    v += QuadraticOurs[pt1][pt2] * pieceCount[Us][pt2]
                    + QuadraticTheirs[pt1][pt2] * pieceCount[Them][pt2];

                bonus += pieceCount[Us][pt1] * v;
            }

            return bonus;
        }

    } // namespace

    namespace Material {


        /// Material::probe() looks up the current position's material configuration in
        /// the material hash table. It returns a pointer to the Entry if the position
        /// is found. Otherwise a new Entry is computed and stored there, so we don't
        /// have to recompute all when the same material configuration occurs again.

        Entry* probe(const Position& pos) {

            Key key = pos.material_key();
            Entry* e = pos.this_thread()->materialTable[key];

            if (e->key == key)
                return e;

            std::memset(e, 0, sizeof(Entry));
            e->key = key;

            // Evaluate the material imbalance. We use PIECE_TYPE_NONE as a place holder
            // for the bishop pair "extended piece", which allows us to be more flexible
            // in defining bishop pair bonuses.
            const int pieceCount[COLOR_NB][13] = {
            { 0,pos.count<ROOK>(WHITE), pos.count<ADVISOR>(WHITE), pos.count<CANNON>(WHITE),
              pos.count<PAWN>(WHITE), pos.count<KNIGHT>(WHITE), pos.count<BISHOP>(WHITE),
              pos.darkcount<ROOK>(WHITE), pos.darkcount<ADVISOR>(WHITE), pos.count<CANNON>(WHITE),
              pos.darkcount<PAWN>(WHITE), pos.count<KNIGHT>(WHITE), pos.count<BISHOP>(WHITE) },
            { 0,pos.count<ROOK>(BLACK), pos.count<ADVISOR>(BLACK), pos.count<CANNON>(BLACK),
              pos.count<PAWN>(BLACK), pos.count<KNIGHT>(BLACK), pos.count<BISHOP>(BLACK),
              pos.darkcount<ROOK>(BLACK), pos.darkcount<ADVISOR>(BLACK), pos.darkcount<CANNON>(BLACK),
              pos.darkcount<PAWN>(BLACK), pos.darkcount<KNIGHT>(BLACK), pos.darkcount<BISHOP>(BLACK)} };

            e->score = (imbalance<WHITE>(pieceCount) - imbalance<BLACK>(pieceCount)) / 16;
            return e;
        }

    } // namespace Material

} // namespace Stockfish