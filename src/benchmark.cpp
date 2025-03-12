/*
  Stockfish, a UCI chess playing engine derived from Glaurung 2.1
  Copyright (C) 2004-2025 The Stockfish developers (see AUTHORS file)

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

#include "benchmark.h"
#include "numa.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

// clang-format off
// Positions from https://www.chessprogramming.org/Chinese_Chess_Perft_Results
const std::vector<std::string> Defaults = {

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
    "3ak1bn1/4a4/1c2b1c2/r3p1N1p/p1p6/6P2/n1P1P3P/N1C1C3B/3R5/2BAKA3 w",
    "1rb1kabr1/4a4/1c7/p1p1R3p/7n1/2P3p2/P3P1c1P/C1N6/4N4/1RBAKAB2 w",
    "r1b1kabr1/4a1c2/1cn3n2/p1p1pR2p/3NP4/2P6/P5p1P/1C2C4/9/RNBAKAB2 b",
    "rn1akab2/9/1c2b1n1c/3Pp1p1p/p8/6P2/P1N1P3P/2C1C4/3rN4/R2AKAB1R w",
    "2bakab2/6r2/2n1c1nc1/p1p2rp1p/4p4/2PN2PC1/P3P3P/6N2/3CA4/1RBAK1B1R w",
    "rn2kab2/3ra4/2c1b2cn/p5p1C/9/2p6/P3P1P1P/NC4N2/9/1RBAKABR1 w",
    "rnbakab2/2r6/1c4nc1/p3p1C1p/2p3p2/2P6/P3P1P1P/1CN3N2/8R/R1BAKAB2 b",
    "r2akabr1/4n4/4b1nc1/p1N1p1R1p/6p2/2p3P2/Pc2P3P/2C1C1N2/9/R1BAKAB2 b",
    "3ak1b1r/4a4/b1n2c3/p3C2Rp/5np2/2P6/P2rP1P1P/3C2N2/4A4/R1B1KAB2 b",
    "rnba1aCn1/4k4/8r/p1p1p3p/1c4P2/2P6/P3c3P/1C4N2/4K4/RNBA1AB1R b",
    "4kab2/4a4/n3b4/p5p1p/2r1C4/2N1P2r1/P4nPcP/N3B4/2R6/2RAKAB2 w",
    "4kab2/4a4/2n1bcc2/p1N1p1Crp/5RP2/2P2N3/P3r4/4B4/C3A2n1/2BAK3R w",
    "r2akabr1/4n1c2/4b1c2/pC2C3p/2P1P4/9/P3N1p1P/9/9/RNBAKAB2 w",
    "2b1ka1r1/4a4/4b1n2/p3p1p1p/3n5/2p1P1P2/PR6P/2cCBRN2/3rA4/1NBAK4 b",
    "4k2n1/9/1c2b4/p3p1N1p/7r1/6P2/P1R1P3P/4B4/4A4/2BAK4 b",
    "2c1kab2/4a4/2n1b3c/p1pN4R/3r2p1p/2P6/P3P1P1n/4BC2N/4A4/2C1KAB2 w",
    "6b2/3ka1N2/5a3/p3p4/1n3P3/P1N5C/5nc2/3AB4/9/3AK1B2 b",
    "3k1a3/2P1aP3/4b1n2/8C/6b2/1R5R1/9/9/1rcpr4/3c1K3 w",
    "4ka3/3Pa4/r6R1/2C4C1/9/9/8n/9/4p3r/3K3R1 w",
    "4ka3/4a4/N8/p8/C8/9/9/8B/3p2ppc/4K4 w",
    "9/4k4/3aba3/3P5/1cb6/2BC5/n3N4/B2A5/9/3AK4 w",
    "3ak4/3Pa4/4b3b/5r3/1R3N3/9/9/B8/2p1A4/2B1KA3 w",
    "4k1b2/4a4/5a3/6P1C/9/p4Nn2/2n6/9/4K4/5AB2 b",

    // Positions with complicated checks and evasions
    "CRN1k1b2/3ca4/4ba3/9/2nr5/9/9/4B4/4A4/4KA3 w",
    "R1N1k1b2/9/3aba3/9/2nr5/2B6/9/4B4/4A4/4KA3 w",
    "C1nNk4/9/9/9/9/9/n1pp5/B3C4/9/3A1K3 w",
    "4ka3/4a4/9/9/4N4/p8/9/4C3c/7n1/2BK5 w",
    "2b1ka3/9/b3N4/4n4/9/9/9/4C4/2p6/2BK5 w",
    "1C2ka3/9/C1Nab1n2/p3p3p/6p2/9/P3P3P/3AB4/3p2c2/c1BAK4 w",
    "CnN1k1b2/c3a4/4ba3/9/2nr5/9/9/4C4/4A4/4KA3 w"
};
// clang-format on

// clang-format off
// Human-randomly picked 5 games with <60 moves from kaka's selfplay games
// only moves for one side
const std::vector<std::vector<std::string>> BenchmarkPositions = {
    {
        "2bakab2/9/c3c1n2/p3p1p1p/1npr5/2P2NP2/P3P3P/2CCB4/4A4/1NBAKR3 b - - 1 1",
        "2bakab2/9/c3c1n2/p3p1p1p/1nC1r4/2P2NP2/P3P3P/3CB4/4A4/1NBAKR3 b - - 0 2",
        "2bakab2/9/c2c2n2/p3p1p1p/1nC1r4/2P2NP2/P3P3P/2NCB4/4A4/2BAKR3 b - - 2 3",
        "2baka3/9/c2cb1n2/p1C1p1p1p/1n2r4/2P2NP2/P3P3P/2NCB4/4A4/2BAKR3 b - - 4 4",
        "2baka3/9/2ccb1n2/p1C1p1N1p/1n2r4/2P3P2/P3P3P/2NCB4/4A4/2BAKR3 b - - 0 5",
        "2baka3/9/2ccb1n2/p1C1p1N1p/1n5r1/1NP3P2/P3P3P/3CB4/4A4/2BAKR3 b - - 2 6",
        "2baka3/9/1c1cb1n2/pC2p1N1p/1n5r1/1NP3P2/P3P3P/3CB4/4A4/2BAKR3 b - - 4 7",
        "2bak4/4a4/1c1cb1n2/pC2p1N1p/1n5r1/PNP3P2/4P3P/3CB4/4A4/2BAKR3 b - - 6 8",
        "2bak4/4a4/1c2b1n2/pC2p1N1p/1n5r1/P1Pc2P2/4P3P/N2CB4/4A4/2BAKR3 b - - 8 9",
        "2bak4/4a4/1c2b1n2/pC1cp1N1p/1n5r1/P1P2RP2/4P3P/N2CB4/4A4/2BAK4 b - - 10 10",
        "2bak4/4a4/2c1b1n2/pC1cp1N1p/1n5r1/P1P2RP2/4P3P/N2CB4/9/2BAKA3 b - - 12 11",
        "2bak4/2c1a4/4b1n2/pC1cp1N1p/1n5r1/P1P2RP2/4P3P/N2CB4/4A4/2B1KA3 b - - 14 12",
        "2bak4/2c1a4/4b1n2/pCc1p1N1p/1n5r1/P1P2RP1P/4P4/N2CB4/4A4/2B1KA3 b - - 16 13",
        "2bak4/1c2a4/4b1n2/pCc1p1N1p/1n5r1/P1P2RP1P/4P4/N3B4/3CA4/2B1KA3 b - - 18 14",
        "2bak4/1c2a4/4b1n2/pCc1p1N1p/1n1r5/P1P2RP1P/4P4/N2AB4/3C5/2B1KA3 b - - 20 15",
        "2bak4/1c2a4/4b1n2/pCc1p1N1p/1n7/P1P2RP1P/4P4/N2rB4/6C2/2B1KA3 b - - 1 16",
        "2bak4/1c2a4/4N1n2/pCc1p3p/9/P1P2RP1P/n3P4/N2rB4/6C2/2B1KA3 b - - 0 17",
        "3ak4/1c2a4/4b1C2/pCc1p3p/9/P1P2RP1P/n3P4/N2rB4/9/2B1KA3 b - - 0 18",
        "3ak4/1c2a4/4b1C2/p1c1p3p/9/PCP2RP1P/n3P4/N3B4/3r5/2B1KA3 b - - 2 19",
        "3ak4/1c2a4/4b1C2/p1c1p3p/9/PCP2RP1P/4P4/N1n1B4/3rA4/2B1K4 b - - 4 20",
        "3ak4/1c2a4/4b1C2/p1c1p3p/9/PCP2RP1P/4P4/N1n1B4/4r4/2B2K3 b - - 1 21",
        "3ak4/1c2a4/4b1C2/p1c1p3p/2P6/PC3RP1P/4P4/N1n1B4/3r5/2B2K3 b - - 3 22",
        "3ak4/1c2a4/4b1C2/p1c1p3p/2P6/PC3RP1P/4P4/N1n1B4/5K3/2Br5 b - - 5 23",
        "3ak4/1c7/4bRC2/p1c1p3p/2P6/PC4P1P/4P4/N1n1B4/5K3/2Br5 b - - 0 24",
        "3ak4/1c7/4bRC2/p1c1p3p/2P6/PC4P1P/4P4/N1n1B4/3r5/2B2K3 b - - 2 25",
        "4k4/1c2a4/4b1C2/p1c1p3p/2P6/PC3RP1P/4P4/N1n1B4/3r5/2B2K3 b - - 4 26",
        "2c1k4/1c2a4/4b1C2/p1P1p3p/9/PC3RP1P/4P4/N1n1B4/3r5/2B2K3 b - - 6 27",
        "2c1k4/1c2a4/4b1C2/p1P1p3p/9/P4RP1P/4P4/N3B4/n2r5/1CB2K3 b - - 8 28",
        "4k4/1c2a4/4b1C2/p1P1p3p/9/P1R3P1P/4P4/N3B4/n2r5/1Cc2K3 b - - 1 29",
        "4k4/1c2a4/4b1C2/p1P1p3p/9/P1R3P1P/4P4/N3B4/n4K3/1Ccr5 b - - 3 30",
        "4k4/1c2a4/4b1C2/p1c1p3p/9/P5P1P/4P4/N3B4/n1R2K3/1C1r5 b - - 1 31",
        "4k4/1c2a4/4b1C2/p1c1p3p/9/P5P1P/4P4/N3B4/R4K3/1r7 b - - 0 32",
        "4k4/1c2a4/4b1C2/p1c1p3p/6P2/P7P/4P4/Nr2B4/R4K3/9 b - - 2 33",
        "4k4/1c2a4/4b1C2/p3p3p/2c2P3/P7P/4P4/Nr2B4/R4K3/9 b - - 4 34",
        "4k4/1c2a4/4b1C2/p3p3p/2c2P3/P7P/4P4/N3r4/1R3K3/9 b - - 1 35",
        "4k4/4a4/4b1C2/p3p3p/2c2P3/P7P/4P4/Nc2r4/5K3/1R7 b - - 3 36",
        "4k4/4a4/4b1C2/p3p3p/2c3P2/P7P/4P4/Nc4r2/5K3/1R7 b - - 5 37",
        "4k4/4a4/4b1C2/p3p3p/2c2P3/P7P/4P4/Nc6r/5K3/1R7 b - - 7 38",
        "4k4/4a4/4b1C2/p3p3p/2c2P3/P7P/4P4/Nc3K3/8r/1R7 b - - 9 39",
        "4k4/4a4/4b2C1/p3p3p/2c2P3/P7P/4P4/Nc3K3/6r2/1R7 b - - 11 40",
        "4k4/4a4/4b1C2/p3p3p/2c2P3/P7P/4P4/Nc3K3/7r1/1R7 b - - 13 41",
        "4k4/4a4/4b1C2/p3p3p/2c2P1r1/P7P/4P4/Nc2K4/9/1R7 b - - 15 42",
        "4k4/4a4/4b1C2/p3p3p/2c2r3/P3P3P/9/Nc2K4/9/1R7 b - - 1 43",
        "4k4/4a4/4b3C/p3p3p/2c3r2/P3P3P/9/Nc2K4/9/1R7 b - - 3 44",
        "4k4/4a4/4b3C/p3p3p/6r2/P3P3P/9/Nc7/2c1K4/1R7 b - - 5 45",
        "4k4/4a4/4b3C/p3p3p/6r2/P3P3P/9/Nc7/1c2K4/7R1 b - - 7 46",
        "4k4/4a4/4b3C/p3p3p/6r2/P3P3P/9/Nc7/c3K4/R8 b - - 9 47",
        "4k4/4a4/4b3C/p7p/4P1r2/P7P/9/Nc7/c3K4/R8 b - - 0 48",
        "4k4/4a4/4b3C/p7p/4r4/P7P/9/Nc7/c2K5/R8 b - - 1 49",
        "4k4/9/4ba2C/p7p/4r4/PN6P/9/1c7/c2K5/R8 b - - 3 50",
        "4k4/9/5a1C1/p7p/4r1b2/PN6P/9/1c7/c2K5/R8 b - - 5 51",
        "4k4/9/5a1C1/p7p/6b2/PN6P/9/1c1K5/c3r4/R8 b - - 7 52",
        "4k4/9/5a3/p7p/6b2/PN2r3P/7C1/1c1K5/c8/R8 b - - 9 53",
        "4k4/9/5a3/p7p/6b2/Pr6P/7C1/1c1K5/c8/4R4 b - - 1 54",
        "4k4/4a4/9/p7p/6b2/Pr6P/4C4/1c1K5/c8/4R4 b - - 3 55",
        "4k4/9/3a5/p7p/6b2/Pr6P/3C5/1c1K5/c8/4R4 b - - 5 56",
        "5k3/9/3a5/p7p/6b2/Pr6P/3C5/1c1K5/c3R4/9 b - - 7 57",
    },
    {
        "r1bakr3/4a4/2ncb1n2/2p1p1p1p/p8/2PN5/P3P1P1P/C3B4/6Cc1/1RBAKA1NR w - - 0 1",
        "r1bakr3/4a4/2ncb1n2/2N1p3p/p5p2/2P6/P3P1P1P/C3B4/6Cc1/1RBAKA1NR w - - 1 2",
        "r1bakr3/4a4/2ncb1n2/2N1p2cp/p5p2/2P6/P3P1P1P/C3B4/2C6/1RBAKA1NR w - - 3 3",
        "r1bakr3/4a4/3cN1n2/4p2cp/p2n2p2/2P6/P3P1P1P/C3B4/2C6/1RBAKA1NR w - - 1 4",
        "r2akr3/4a4/3cb1n2/4p2cp/pR1n2p2/2P6/P3P1P1P/C3B4/2C6/2BAKA1NR w - - 0 5",
        "1r1akr3/4a4/3cb1n2/4p2cp/p2R2p2/2P6/P3P1P1P/C3B4/2C6/2BAKA1NR w - - 1 6",
        "1r1akr3/4a4/3cb1n2/4p3p/p2R2pc1/2P6/P3P1P1P/C3B1N2/2C6/2BAKA2R w - - 3 7",
        "1r1ak4/4a4/3cb1n2/4p3p/p5pc1/2PR5/P3P1P1P/C3B1N2/2C2r3/2BAKA2R w - - 5 8",
        "1r1ak4/4a4/3cb1n2/4p3p/p4rpc1/2PR5/P3P1P1P/C3B1N2/2C1A4/2BAK3R w - - 7 9",
        "1r1ak4/4a4/3cb1n2/4p3p/p1r3pc1/3R5/P3P1P1P/C3B1N2/2C1A4/2BAK3R w - - 0 10",
        "1r1ak4/4a4/3cb4/4p3p/p1r2npc1/2CR5/P3P1P1P/C3B1N2/4A4/2BAK3R w - - 2 11",
        "1r1ak4/4a4/3cb2c1/4p3p/p1r2np2/2C4R1/P3P1P1P/C3B1N2/4A4/2BAK3R w - - 4 12",
        "1r1ak4/4a4/3cb2c1/4p3p/p1r2n3/2C3pR1/P3P3P/C3B1N2/4A4/2BAK3R w - - 0 13",
        "1r1aka3/9/3cb2c1/4p3p/p1r2n3/2C3R2/P3P3P/C3B1N2/4A4/2BAK3R w - - 1 14",
        "1r1aka3/9/2c1b2c1/4p3p/p1r2n3/2C3R2/P3P3P/C3B1N2/4A4/2BAK2R1 w - - 3 15",
        "1r1aka3/9/2c1b2c1/4p3p/p3rn3/2C3R2/P3P3P/2C1B1N2/4A4/2BAK2R1 w - - 5 16",
        "1r1aka3/9/2c1b2c1/4p3p/p2r1n3/2C1P1R2/P7P/2C1B1N2/4A4/2BAK2R1 w - - 7 17",
        "1r2ka3/4a4/2c1b2c1/4p3p/p2r1n3/2C1P1R2/P7P/3CB1N2/4A4/2BAK2R1 w - - 9 18",
        "4ka3/4a4/2c1b2c1/4p3p/pr1r1n1R1/2C1P1R2/P7P/3CB1N2/4A4/2BAK4 w - - 11 19",
        "4ka3/4a4/2c1b2c1/4p1R1p/pr3n1R1/2CrP4/P7P/3CB1N2/4A4/2BAK4 w - - 13 20",
        "4ka3/4a4/2c1b2c1/4R3p/pr3n1R1/2r1P4/P7P/3CB1N2/4A4/2BAK4 w - - 0 21",
        "4ka3/4a4/2c1b2c1/8p/p3rn1R1/2r1P4/P7P/3CB1N2/4A4/2BAK4 w - - 0 22",
        "4ka3/4a4/2c1b2c1/8p/p3P2R1/2r6/P5n1P/3CB1N2/4A4/2BAK4 w - - 1 23",
        "4ka3/4a4/4b2c1/8p/p3P4/2r6/P5n1P/3CB1N2/4A4/2BAK4 w - - 0 24",
        "4ka3/4a4/4b4/8p/p3P4/2B6/P5ncP/3C2N2/4A4/2BAK4 w - - 1 25",
        "4ka3/4a4/4b4/8p/p3P4/2B6/c5n1P/C5N2/4A4/2BAK4 w - - 0 26",
        "4kab2/4a4/9/8p/C3P4/2B6/c5n1P/6N2/4A4/2BAK4 w - - 1 27",
        "4kab2/4a4/9/8p/C3P4/2B6/c3N3P/8n/4A4/2BAK4 w - - 3 28",
        "4kab2/4a4/9/8p/C3P4/2B3N2/c7P/9/4A1n2/2BAK4 w - - 5 29",
        "3k1ab2/4a4/9/8p/C3P4/2B3N2/c7P/9/4A1n2/2BA1K3 w - - 7 30",
        "3k1ab2/4a4/9/8p/3CP4/2B3N2/c6nP/9/4A4/2BA1K3 w - - 9 31",
        "3k1ab2/c3a4/9/8p/4P4/2B3N2/3C3nP/9/4A4/2BA1K3 w - - 11 32",
        "3k1a3/c3a4/4b4/8p/4P4/2B3N2/6CnP/9/4A4/2BA1K3 w - - 13 33",
        "3k1a3/c3a4/9/7Np/4P1b2/2B6/6CnP/9/4A4/2BA1K3 w - - 15 34",
        "3k1a3/4a4/9/7Np/4P1b2/2B6/6CnP/4B4/4A4/c2A1K3 w - - 17 35",
        "3k1a3/4a4/c8/7Np/4P1b2/2B6/6CnP/4B4/4AK3/3A5 w - - 19 36",
        "3k1a3/4a4/5c3/7Np/4P1b2/2B6/6CnP/4BA3/5K3/3A5 w - - 21 37",
        "3k1a3/4a4/9/7Np/4P1b2/2B6/5cCnP/4BA3/4K4/3A5 w - - 23 38",
        "3k1a3/4a4/9/7N1/4P1b1p/2B6/5cCnP/5A3/4K4/3A2B2 w - - 25 39",
        "3k1a3/9/3a5/7N1/4P1b1p/2B6/5cCnP/5A2B/4K4/3A5 w - - 27 40",
        "3k1a3/4a4/9/7N1/5Pb1p/2B6/5cCnP/5A2B/4K4/3A5 w - - 29 41",
        "3k1a3/4a4/8b/9/5P2p/2B3N2/5cCnP/5A2B/4K4/3A5 w - - 31 42",
        "3k1a3/4a4/8b/9/4NP2p/2B6/4c1CnP/5A2B/4K4/3A5 w - - 33 43",
        "3k1a3/4a4/8b/2N6/5P2p/2B6/4c1C1P/5A2B/4K1n2/3A5 w - - 35 44",
        "4ka3/1N2a4/8b/9/5P2p/2B6/4c1C1P/5A2B/4K1n2/3A5 w - - 37 45",
        "4ka3/1N7/5aC1b/9/5P2p/2B6/4c3P/5A2B/4K1n2/3A5 w - - 39 46",
        "4k4/1N2a4/5a2b/6C2/5P2p/2B6/4c3P/5A2B/4K1n2/3A5 w - - 41 47",
        "4k4/1N2a4/5a2b/6C2/6P1p/2B6/4c3P/5A2n/4K4/3A5 w - - 0 48",
        "5k3/1N2a4/5a2b/4C4/6P1p/2B6/4c3P/5A2n/4K4/3A5 w - - 2 49",
        "5k3/1N2a4/5a2b/4C4/7Pp/2B6/4c1n1P/5A3/4K4/3A5 w - - 4 50",
        "5k3/1N2a4/5a3/4C4/6b1P/2B6/4c1n1P/5A3/4K4/3A5 w - - 1 51",
        "5k3/1N2a4/5a3/8C/4c1b1P/2B6/6n1P/5A3/4K4/3A5 w - - 3 52",
    },
    {
        "rnbakabnr/9/1c7/p3p1p1p/7c1/2P6/P3P1P1P/5C3/C8/RNBAKABNR b - - 0 1",
        "rnbakabnr/9/1c7/p3p3p/6pc1/2P6/P3P1P1P/2N2C3/C8/R1BAKABNR b - - 2 2",
        "r1bakabnr/9/1cn6/p3p3p/6pc1/2P6/P3P1P1P/2N1BC3/C8/R1BAKA1NR b - - 4 3",
        "2bakabnr/9/rcn6/p3p3p/6pc1/2P6/P3P1P1P/2N1BC3/C4N3/R1BAKA2R b - - 6 4",
        "2bakab1r/9/rcn3n2/p3p3p/6pc1/2P6/P3P1P1P/2N1BC3/C4N3/R1BAKAR2 b - - 8 5",
        "2bakab1r/1c7/r1n3n2/p3p3p/6pc1/2P6/P3P1P1P/2N1BC3/2C2N3/R1BAKAR2 b - - 10 6",
        "2bakab1r/6c2/r1n3n2/p3p3p/2P3pc1/9/P3P1P1P/2N1BC3/2C2N3/R1BAKAR2 b - - 12 7",
        "2bakab1r/6c2/1rn3n2/p3p3p/2P3pc1/9/P3P1P1P/R1N1BC3/2C2N3/2BAKAR2 b - - 14 8",
        "2bakab1r/6c2/1rn6/p3p3p/2P2npc1/6P2/P3P3P/R1N1BC3/2C2N3/2BAKAR2 b - - 16 9",
        "2bakab1r/4n1c2/1r7/p3p3p/2P2npc1/6P2/P3P3P/R1N1B1C2/2C2N3/2BAKAR2 b - - 18 10",
        "2bakab1r/6c2/1r4n2/p3p3p/2P2nCc1/6P2/P3P3P/R1N1B4/2C2N3/2BAKAR2 b - - 0 11",
        "2bakab1r/5c3/1r4n2/p3p1C1p/2P2n1c1/6P2/P3P3P/R1N1B4/2C2N3/2BAKAR2 b - - 2 12",
        "2bakab1r/9/1r4n2/p3p1C1p/2P2nPc1/9/P3P3P/R1N1B4/2C2c3/2BAKAR2 b - - 1 13",
        "2bakab1r/9/6n2/p3p1C1p/2P2nPc1/9/P3P3P/R1N1B4/1r2Cc3/2BAKAR2 b - - 3 14",
        "2bakab1r/9/6n2/p3p1C1p/2P2P3/9/P3P3P/R1N1B2c1/1r2Cc3/2BAKAR2 b - - 0 15",
        "2bakabr1/9/6n2/p3p1C1p/2P2P3/4P4/P7P/R1N1B2c1/1r2Cc3/2BAKAR2 b - - 2 16",
        "2baka1r1/9/4b1n2/p3p1C1p/2P2P3/P3P4/8P/R1N1B2c1/1r2Cc3/2BAKAR2 b - - 4 17",
        "2baka1r1/9/4b1n2/p3p1C1p/2P2P3/P3P4/Rr6P/2N1B2c1/4Cc3/2BAKAR2 b - - 6 18",
        "2baka1r1/9/4b1n2/p3p1C1p/2P2P3/P3P4/N7P/4B2c1/4Cc3/2BAKAR2 b - - 0 19",
        "2baka1r1/9/4b1n2/p3p3p/2P2P3/P3P4/N7P/4B4/4CcC2/2BAKARc1 b - - 2 20",
        "2baka1r1/9/4b1n2/p3p3p/2P1PP3/P8/N4c2P/4B4/4C1C2/2BAKARc1 b - - 4 21",
        "2baka3/9/4b1n2/p3p3p/2P1PP3/P1N6/5c1rP/4B4/4C1C2/2BAKARc1 b - - 6 22",
        "2baka3/9/4b1n2/p3p3p/2P1PP3/P1N6/7rc/4B4/4C3C/2BAKARc1 b - - 1 23",
        "2baka3/9/4b4/p1P1p3p/4PP1n1/P1N6/7rc/4B4/4C3C/2BAKARc1 b - - 3 24",
        "2baka3/9/4b4/pNP1p3p/4PP3/P4n3/7rc/4B4/4C3C/2BAKARc1 b - - 5 25",
        "2bak4/4a4/4b4/pNP1P3p/5P3/P4n3/7rc/4B4/4C3C/2BAKARc1 b - - 0 26",
        "2bak4/4a4/4b4/pNP1P3p/5P3/P8/7rc/4B1n2/5C2C/2BAKARc1 b - - 2 27",
        "2bak4/4a4/4b4/pNP1P3p/5P3/P8/5r2c/4B1n2/5CC2/2BAKARc1 b - - 4 28",
        "2ba1k3/4a4/4b4/pNP1P3p/5P3/P8/5r2c/4B1n2/5CC2/2BAKA1R1 b - - 0 29",
        "2ba1k1R1/4a4/4b4/pNP1P3p/5P3/P8/8c/4B1n2/5rC2/2BAKA3 b - - 1 30",
        "2ba5/4ak3/4b4/pNP1P3p/5P3/P8/8c/4B1n2/5rCR1/2BAKA3 b - - 3 31",
        "2ba5/4ak1R1/4b4/pNP1P3p/5P3/P8/6c2/4B1n2/5rC2/2BAKA3 b - - 5 32",
        "2ba1k1R1/4a4/4b4/pNP1P3p/5P3/P8/6c2/4B1n2/5rC2/2BAKA3 b - - 7 33",
        "2ba2R2/4ak3/4b4/pNP1P3p/5P3/P8/6c2/4B1n2/5rC2/2BAKA3 b - - 9 34",
        "2ba2R2/4ak3/4b4/pNP1P3p/5r3/P8/6c2/4B1n2/4A1C2/2B1KA3 b - - 1 35",
        "2ba2R2/4ak3/4b4/pNP2P2p/5r3/P8/4c4/4B1n2/4A1C2/2B1KA3 b - - 3 36",
        "2ba2R2/4ak3/4b4/pNP2r2p/9/P8/4c4/4B1n2/4A1C2/2BK1A3 b - - 1 37",
        "2ba5/4ak3/4b4/pNP2r2p/9/P8/4c1R2/4B4/4A1C1n/2BK1A3 b - - 3 38",
        "2ba5/4ak3/4b4/p1P5p/9/P1N6/4c1R2/4B4/4ArC1n/2BK1A3 b - - 5 39",
        "2ba5/4ak3/2P1b4/p7p/9/P1N6/5cR2/4B4/4ArC1n/2BK1A3 b - - 7 40",
        "2ba5/2P1ak3/4bc3/p7p/9/P1N6/6R2/4B4/4ArC1n/2BK1A3 b - - 9 41",
        "2ba1k3/2P1a4/4bc3/p7p/4N4/P8/6R2/4B4/4ArC1n/2BK1A3 b - - 11 42",
        "2Pa1k3/4a4/4b2c1/p7p/4N4/P8/6R2/4B4/4ArC1n/2BK1A3 b - - 0 43",
        "3P1k3/4a4/4b2c1/p8/4N3p/P8/6R2/4B4/4ArC1n/2BK1A3 b - - 0 44",
        "3P1k3/4a4/4b4/p8/4N3p/P8/6R2/4B4/4Ar2n/2BK1ACc1 b - - 2 45",
        "3P1k3/4a4/4b1R2/p4r3/4N3p/P8/9/4B4/4A3n/2BK1ACc1 b - - 4 46",
        "3P1k3/4a4/4b1R2/p2r5/4N3p/P8/9/4B4/4A3n/2B1KACc1 b - - 6 47",
        "3P1k3/4a4/4b1R2/p3r1N2/8p/P8/9/4B4/4A3n/2B1KACc1 b - - 8 48",
        "3P1k3/4a4/4b1R2/p3r1N2/8p/P8/9/9/4A4/2B1KABc1 b - - 0 49",
    },
    {
        "rnbaka1nr/9/6c1b/pCp1p1pcp/9/9/P1P1P1P1P/2C6/9/RNBAKABNR w - - 0 1",
        "r1baka1nr/9/n5c1b/pCp1p1pcp/9/9/P1P1P1P1P/2C3N2/9/RNBAKAB1R w - - 2 2",
        "r1baka1nr/9/n6cb/p1p1p1pcp/9/1C7/P1P1P1P1P/2C3N2/9/RNBAKAB1R w - - 4 3",
        "r1baka1n1/8r/n6cb/p1p1p1pcp/9/PC7/2P1P1P1P/2C3N2/9/RNBAKAB1R w - - 6 4",
        "r1baka1n1/5r3/n6cb/p1p1p1pcp/9/PC7/2P1P1P1P/N1C3N2/9/R1BAKAB1R w - - 8 5",
        "r1b1ka1n1/4ar3/n6cb/p1p1p1pcp/9/PC7/2P1P1P1P/N1C3N2/8R/R1BAKAB2 w - - 10 6",
        "r3ka1n1/4ar3/n3b2cb/p1p1p1pcp/9/PC7/2P1P1P1P/N1C3N2/R7R/2BAKAB2 w - - 12 7",
        "3rka1n1/4ar3/n3b2cb/p1p1p1pcp/9/PC7/2P1P1P1P/N1C3N2/5R2R/2BAKAB2 w - - 14 8",
        "3rka1n1/4a4/n3b2cb/p1p1p1pcp/9/PC7/2P1P1P1P/N1C3N2/4Ar2R/2BAK1B2 w - - 0 9",
        "4ka1n1/4a4/n3b2cb/p1p1p1pcp/3r5/PC7/2P1P1P1P/N1C3N2/4AR3/2BAK1B2 w - - 1 10",
        "4ka1n1/2n1a4/4b2cb/p1p1p1pcp/3r5/P3C4/2P1P1P1P/N1C3N2/4AR3/2BAK1B2 w - - 3 11",
        "4ka1n1/2n1a4/4b2cb/p3p1pcp/2pr5/P3CR3/2P1P1P1P/N1C3N2/4A4/2BAK1B2 w - - 5 12",
        "4ka1n1/2n1a4/4b2cb/p3p1pcp/2p4r1/PN2CR3/2P1P1P1P/2C3N2/4A4/2BAK1B2 w - - 7 13",
        "4ka1n1/2n1a4/4b2cb/p3p1pc1/2p4rp/PN2CR3/2P1P1P1P/2C1B1N2/4A4/2BAK4 w - - 9 14",
        "4kabn1/2n1a4/4b2c1/p3p1pc1/2p4rp/PN1C1R3/2P1P1P1P/2C1B1N2/4A4/2BAK4 w - - 11 15",
        "4kabn1/2n1a4/4b2c1/p3p1p1c/2p4rp/PN3R3/2P1P1P1P/2CCB1N2/4A4/2BAK4 w - - 13 16",
        "4kabn1/2n1a4/4b3c/N3p1p1c/2p4rp/P4R3/2P1P1P1P/2CCB1N2/4A4/2BAK4 w - - 1 17",
        "4kabn1/2n1a4/4b3c/N3p1p2/2p4rp/PR7/2P1P1P1c/2CCB1N2/4A4/2BAK4 w - - 0 18",
        "4kabn1/2n1a4/4b4/N3p1p2/2p4rp/PR7/2P1P1P1c/2CCB4/4A4/2BAK4 w - - 0 19",
        "4kab2/2n1a4/4b3n/N3p1p2/2p4rp/PR4P2/2P1P3c/2CCB4/4A4/2BAK4 w - - 2 20",
        "4kab2/2n1a4/4b3n/N3p1p2/2p5p/PR4P2/2P1P3c/3CB4/2C1A4/2BAK2r1 w - - 4 21",
        "4kab2/2n1a4/4b3n/N3p1p2/2p5p/PR4P2/2P1P3c/3CB4/2C4r1/2BAKA3 w - - 6 22",
        "4kab2/2n1a4/4b3n/N3p1p2/2p5p/PR4P2/2P1P3c/3CB4/4Cr3/2BAKA3 w - - 8 23",
        "4kab2/2n1a4/4b3n/N3p1p2/2p5p/PR4P2/2P1P3c/3CB4/1C1r5/2BAKA3 w - - 10 24",
        "4kab2/2n1a4/4b3n/N3p1p2/2p5p/PR4P2/2c1P4/3CB4/1C1rA4/2BAK4 w - - 0 25",
        "3akab2/1Rn6/4b3n/N3p1p2/2p5p/P5P2/2c1P4/3CB4/1C1rA4/2BAK4 w - - 2 26",
        "3akab2/1Rn6/4b3n/N3p1p2/1Cp5p/P5P2/2c1P4/3CB4/r3A4/2BAK4 w - - 4 27",
        "3akab2/1Rn6/4b3n/N3p1p2/2p5C/r5P2/2c1P4/3CB4/4A4/2BAK4 w - - 0 28",
        "3akab2/1R7/4b3n/3np1p2/2p5C/rN4P2/2c1P4/3CB4/4A4/2BAK4 w - - 2 29",
        "3akab2/1R7/4b3n/3np1p2/2p5C/r5P2/4P4/N1cCB4/4A4/2BAK4 w - - 4 30",
        "3akab2/1R7/4b3n/3np1p2/2p5C/r5P2/4P4/3CB4/2c1A4/1NBAK4 w - - 6 31",
        "3akab2/9/4b3n/3np1p2/2p5C/r5P2/2c1P4/3CB4/1R2A4/1NBAK4 w - - 8 32",
        "3akab2/2n6/4b3n/1R2p1p2/2p5C/r5P2/2c1P4/3CB4/4A4/1NBAK4 w - - 10 33",
        "4kab2/2n1a4/4b3n/4R1p2/2p5C/r5P2/2c1P4/3CB4/4A4/1NBAK4 w - - 1 34",
        "4kab2/2n1a4/4b3n/4R1p2/4C4/r1p3P2/2c1P4/3CB4/4A4/1NBAK4 w - - 3 35",
        "4kab2/2n1a4/4b3n/4R1p2/4C4/r1p3P2/4P4/N1cCB4/4A4/2BAK4 w - - 5 36",
        "4kab2/2n1a4/4b3n/5Rp2/4C4/2p3P2/r3P4/N1cCB4/4A4/2BAK4 w - - 7 37",
        "3k1ab2/2n1a4/4b3n/5Rp2/4C4/2p3P2/r3P4/N1cCB4/4A4/2BA1K3 w - - 9 38",
        "3k1ab2/2n1a4/4b3n/5Rp2/4C4/2B3P2/4r4/N1cC5/4A4/2BA1K3 w - - 0 39",
        "3k1ab2/4a4/4b3n/1n4p2/4CR3/2B3P2/4r4/N1cC5/4A4/2BA1K3 w - - 2 40",
        "3k1ab2/4a4/4b3n/1n4p2/4CR3/2B3P2/2c1r4/N8/3CA4/2BA1K3 w - - 4 41",
    },
    {
        "1n1akab1r/9/4bc3/p1p1p3p/5n3/2P3R2/P3P3P/2N2CN1C/4A4/2c1KAB2 b - - 1 1",
        "1n1akab2/9/4bc2r/p1p1p3p/5n3/2P3R2/P3P3P/2N1BCN1C/4A4/2c1KA3 b - - 3 2",
        "1n1akab2/9/4bc2r/p1p1p3p/5n3/2P2NR2/P3P3P/2N1BC2C/4A4/c3KA3 b - - 5 3",
        "1n1akab2/9/4bc2r/p1p1p1Rnp/9/2P2N3/P3P3P/2N1BC2C/4A4/c3KA3 b - - 7 4",
        "1n1akab2/9/4b3r/p1p1p1Rnp/9/2P2N3/P3P3P/2N1BA2C/9/c3KA3 b - - 0 5",
        "1n1akab2/9/4br3/p1p1p1Rnp/9/2P6/P3P3P/2N1BAN1C/9/c3KA3 b - - 2 6",
        "1n1akab2/8n/4br3/p1p1p1R1p/9/2PN5/P3P3P/4BAN1C/9/c3KA3 b - - 4 7",
        "1n2kab2/4a3n/4br3/p1p1p1R1p/9/2PN4P/P3P4/4BAN1C/9/c3KA3 b - - 6 8",
        "1n2kab2/4a3n/4br3/p1p1R3p/9/2PN4P/P3P4/4BAN1C/9/1c2KA3 b - - 0 9",
        "4kab2/3na3n/4br3/p1pR4p/9/2PN4P/P3P4/4BAN1C/9/1c2KA3 b - - 2 10",
        "4kab2/4a3n/1n2br3/p1R5p/9/2PN4P/P3P4/4BAN1C/9/1c2KA3 b - - 0 11",
        "4kab2/4a3n/1n2br3/p1R5p/9/1cP5P/P3P4/2N1BAN1C/9/4KA3 b - - 2 12",
        "4kab2/4a3n/1n2br3/p1R5p/9/2P5P/P3P4/1c2BAN1C/4N4/4KA3 b - - 4 13",
        "4kab2/4a3n/1n2br3/p1R5C/9/2P5P/P3P4/4BcN2/4N4/4KA3 b - - 0 14",
        "4kab2/4a3n/1n2b4/p1R5C/9/2P5P/P3Pr3/4BcN2/9/4KAN2 b - - 2 15",
        "4kab2/4a3n/1n2b4/p5R1C/9/2P5P/P3Pr3/4B1N2/5c3/4KAN2 b - - 4 16",
        "4kab2/4a3n/1n2b4/p5R1C/9/2P4NP/P3Pr3/4B4/6c2/4KAN2 b - - 6 17",
        "4kab2/4a3n/1n2b4/p7C/9/2P2r1NP/P3P4/4B4/6R2/4KAN2 b - - 0 18",
        "4kab2/4a1R1n/1n2b4/p7C/9/2P4rP/P3P4/4B4/9/4KAN2 b - - 1 19",
        "4kab2/4a3R/1n2b4/p7C/9/2P5r/P3P4/4B4/9/4KAN2 b - - 0 20",
        "4kab2/4a3R/1n2b4/p7C/9/2P6/P3P4/4B3r/4A4/4K1N2 b - - 2 21",
        "4kab2/4a3R/1n2b4/p8/9/2P6/P3P4/4r3C/4A4/4K1N2 b - - 1 22",
        "4kab2/4a3R/4b4/p8/n8/2P6/P3P4/4rN2C/4A4/4K4 b - - 3 23",
        "4kab2/4a4/4b4/p7R/n8/2P6/P3r4/5N2C/4A4/4K4 b - - 1 24",
        "4kab2/4a4/4b4/R8/9/2n6/P3r4/5N2C/4A4/4K4 b - - 0 25",
        "4kab2/4a4/4b4/R8/9/2n6/P7r/5NC2/4A4/4K4 b - - 2 26",
        "4kab2/4a4/4b4/9/9/2n6/R8/5NC2/4A4/4K4 b - - 0 27",
        "4kab2/4a4/4b4/9/9/6N2/n8/6C2/4A4/4K4 b - - 1 28",
        "4kab2/4a4/4b4/9/9/2n3N2/9/7C1/4A4/4K4 b - - 3 29",
        "3k1ab2/4a4/4b4/5N3/9/2n6/9/7C1/4A4/4K4 b - - 5 30",
        "3k1ab2/4a4/4b4/5N3/4n4/9/9/3C5/4A4/4K4 b - - 7 31",
        "4kab2/4a1N2/4b4/9/4n4/9/9/3C5/4A4/4K4 b - - 9 32",
        "3k1ab2/4a1N2/4b4/9/4n4/9/9/9/3CA4/4K4 b - - 11 33",
        "3k1ab2/4a4/4b4/5N3/9/2n6/9/9/3CA4/4K4 b - - 13 34",
        "3k1ab2/4a4/3Nb4/9/4n4/9/9/9/3CA4/4K4 b - - 15 35",
        "3k1ab2/4a4/4b4/1N7/9/9/3n5/9/3CA4/4K4 b - - 17 36",
        "3k1ab2/4a4/4b4/1N7/4n4/9/9/9/3CA4/3K5 b - - 19 37",
        "3k1ab2/4a4/9/1N1C5/2b1n4/9/9/9/4A4/3K5 b - - 21 38",
        "5ab2/3ka4/9/3C5/2b1n4/N8/9/9/4A4/3K5 b - - 23 39",
        "5ab2/3ka4/9/3C5/2b6/N8/3n5/9/4A4/4K4 b - - 25 40",
        "5a3/3ka4/4b4/9/2b6/N2C5/3n5/9/4A4/4K4 b - - 27 41",
        "5a3/3k5/4ba3/1N7/2b6/3C5/3n5/9/4A4/4K4 b - - 29 42",
        "5a3/2N1k4/4ba3/9/2b6/3C5/3n5/9/4A4/4K4 b - - 31 43",
        "5a3/2N1k4/4ba3/9/2b6/3C5/9/9/2n1A4/5K3 b - - 33 44",
        "5a3/2N1k4/4ba3/9/2b6/C8/3n5/9/4A4/5K3 b - - 35 45",
        "5a3/3k5/4ba3/3N5/2b6/C8/3n5/9/4A4/5K3 b - - 37 46",
        "3k1a3/9/4ba3/3N5/2b6/3C5/3n5/9/4A4/5K3 b - - 39 47",
        "4ka3/9/4bN3/9/2b6/3C5/3n5/9/4A4/5K3 b - - 0 48",
        "5a3/4k4/4bN3/9/2b6/3C5/3n5/9/4A4/4K4 b - - 2 49",
        "5a3/4k4/4b4/7N1/2b6/3C1n3/9/9/4A4/4K4 b - - 4 50",
        "5a3/4k4/4b4/7N1/2b6/4C4/9/4n4/4A4/4K4 b - - 6 51",
        "5a3/5k3/4b4/9/2b6/4C1N2/9/4n4/4A4/4K4 b - - 8 52",
        "9/4ak3/4b4/9/2b6/4C1N2/9/4n4/4A4/3K5 b - - 10 53",
        "3a5/5k3/4b4/5N3/2b6/4C4/9/4n4/4A4/3K5 b - - 12 54",
        "3a1k3/9/4b4/5N3/2b6/9/4C4/4n4/4A4/3K5 b - - 14 55",
        "3a1k3/9/4b4/5N3/2b6/9/5Cn2/9/4A4/3K5 b - - 16 56",
        "3ak4/9/4b4/5N3/2b6/9/6n2/9/4A4/3K1C3 b - - 18 57",
        "3ak4/6N2/4b4/9/2b6/9/9/4n4/4A4/3K1C3 b - - 20 58",
        "3a5/4k1N2/4b4/9/2b6/9/9/4n4/4A4/3KC4 b - - 22 59",
    }
};
// clang-format on

}  // namespace

namespace Stockfish::Benchmark {

// Builds a list of UCI commands to be run by bench. There
// are five parameters: TT size in MB, number of search threads that
// should be used, the limit value spent for each position, a file name
// where to look for positions in FEN format, and the type of the limit:
// depth, perft, nodes and movetime (in milliseconds). Examples:
//
// bench                            : search default positions up to depth 13
// bench 64 1 15                    : search default positions up to depth 15 (TT = 64MB)
// bench 64 1 100000 default nodes  : search default positions for 100K nodes each
// bench 64 4 5000 current movetime : search current position with 4 threads for 5 sec
// bench 16 1 5 blah perft          : run a perft 5 on positions in file "blah"
std::vector<std::string> setup_bench(const std::string& currentFen, std::istream& is) {

    std::vector<std::string> fens, list;
    std::string              go, token;

    // Assign default values to missing arguments
    std::string ttSize    = (is >> token) ? token : "16";
    std::string threads   = (is >> token) ? token : "1";
    std::string limit     = (is >> token) ? token : "13";
    std::string fenFile   = (is >> token) ? token : "default";
    std::string limitType = (is >> token) ? token : "depth";

    go = limitType == "eval" ? "eval" : "go " + limitType + " " + limit;

    if (fenFile == "default")
        fens = Defaults;

    else if (fenFile == "current")
        fens.push_back(currentFen);

    else
    {
        std::string   fen;
        std::ifstream file(fenFile);

        if (!file.is_open())
        {
            std::cerr << "Unable to open file " << fenFile << std::endl;
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

    for (const std::string& fen : fens)
        if (fen.find("setoption") != std::string::npos)
            list.emplace_back(fen);
        else
        {
            list.emplace_back("position fen " + fen);
            list.emplace_back(go);
        }

    return list;
}

BenchmarkSetup setup_benchmark(std::istream& is) {
    // TT_SIZE_PER_THREAD is chosen such that roughly half of the hash is used all positions
    // for the current sequence have been searched.
    static constexpr int TT_SIZE_PER_THREAD = 128;

    static constexpr int DEFAULT_DURATION_S = 150;

    BenchmarkSetup setup{};

    // Assign default values to missing arguments
    int desiredTimeS;

    if (!(is >> setup.threads))
        setup.threads = get_hardware_concurrency();
    else
        setup.originalInvocation += std::to_string(setup.threads);

    if (!(is >> setup.ttSize))
        setup.ttSize = TT_SIZE_PER_THREAD * setup.threads;
    else
        setup.originalInvocation += " " + std::to_string(setup.ttSize);

    if (!(is >> desiredTimeS))
        desiredTimeS = DEFAULT_DURATION_S;
    else
        setup.originalInvocation += " " + std::to_string(desiredTimeS);

    setup.filledInvocation += std::to_string(setup.threads) + " " + std::to_string(setup.ttSize)
                            + " " + std::to_string(desiredTimeS);

    auto getCorrectedTime = [&](int ply) {
        // time per move is fit roughly based on LTC games
        // seconds = 50/{ply+15}
        // ms = 50000/{ply+15}
        // with this fit 10th move gets 2000ms
        // adjust for desired 10th move time
        return 50000.0 / (static_cast<double>(ply) + 15.0);
    };

    float totalTime = 0;
    for (const auto& game : BenchmarkPositions)
    {
        setup.commands.emplace_back("ucinewgame");
        int ply = 1;
        for (int i = 0; i < static_cast<int>(game.size()); ++i)
        {
            const float correctedTime = getCorrectedTime(ply);
            totalTime += correctedTime;
            ply += 1;
        }
    }

    float timeScaleFactor = static_cast<float>(desiredTimeS * 1000) / totalTime;

    for (const auto& game : BenchmarkPositions)
    {
        setup.commands.emplace_back("ucinewgame");
        int ply = 1;
        for (const std::string& fen : game)
        {
            setup.commands.emplace_back("position fen " + fen);

            const int correctedTime = static_cast<int>(getCorrectedTime(ply) * timeScaleFactor);
            setup.commands.emplace_back("go movetime " + std::to_string(correctedTime));

            ply += 1;
        }
    }

    return setup;
}

}  // namespace Stockfish
