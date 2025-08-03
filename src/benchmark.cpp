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
const std::vector<std::string> Defaults = {
    // Middle game
    "x1xxkxxxx/9/c6x1/2x6/a3r1n1p/R1P3A2/4X3X/1X2B1AX1/4P4/1C2KXX1X w R1N1n1B1b2a1C1c1P3p4 1 8",
    "xx2kx1Bx/4p4/1x2r2xc/2x6/p5a2/2P1p1A1c/X1B1X4/1X2A1N1P/9/XX2KXP2 w R2r1N1n1b2a1C2P1p2 1 11"
    "3xkx1xx/9/2a1p2ap/9/9/2r1P4/1R4p2/1CP1n3r/4N4/4K1CB1 w b2p2 0 25",
    "P3kxx1x/4a4/4p1a2/9/4b1p1p/CR2B4/8X/B1r1r4/9/X2XK2XX w R1N1n1A1P2p2 0 18",
    "C1xx1x1xx/4k4/2a5c/1r2C4/2pA2p1b/p6p1/2X3X1X/2B3P1B/3N5/X2XKX2X b R1r1N1n1b1A1a1c1P4 0 12",
    "x1x1k1x2/3r5/1xb3n2/2x1x4/p3c4/2PR4P/X2r5/2N3N2/9/X1X1KXX2 w R1n1B1b1A2a2c1P1p1 4 18",
    "x2x1k1x1/3pp4/6n1p/2R3BNx/2a6/P5P2/9/1XP1C4/9/X2XKXXXX b R1r1N1n1B1A2C1c1P1p1 2 17",
    "x2x1k3/3p2r2/4p2xp/x8/2a1b1N2/Pp7/2X1X1b2/6B2/4A4/XXXXKCX2 b R2r1n1B1A1a1P3p1 0 14",
    "2n1kx2x/9/axp1n1px1/4x1x2/2b6/C1N5A/4X4/2R1P1rX1/3R1P3/X1XXK3X w r1B1b1A1a1C1c1P3p2 3 14",

    // Endgame
    "3P5/4kb1P1/9/3a5/1Np1p4/2p6/4X1a1p/2N3b2/4CK3/c8 w r1n1B1c1P1 1 33",
    "x2k5/9/2n3p2/6x2/9/6P2/9/4B2n1/9/4KC3 b p2 0 50",
    "9/5k1c1/2R1p4/7C1/p7p/4P4/X8/5r3/P3K4/2X4c1 w B1P1 3 51",
    "x1x1k3x/4p4/b1p4x1/2n4a1/1B7/5a3/2PAP1b2/4N3P/3K5/8r w n1c1p2 0 34"

    // Bright Jieqi
    "rnbakabnr/9/1c5c1/p1p1p1p1p/9/9/P1P1P1P1P/1C5C1/9/RNBAKABNR w - 0 1",
    "2bk5/9/4b4/2a4N1/1n1a4P/p8/3nP1A2/4BA3/4K2C1/9 w - 14 40"
};
// clang-format on

// clang-format off
// only moves for one side
const std::vector<std::vector<std::string>> BenchmarkPositions = {
    {
        "xxxxkxxxx/9/1x5x1/x1x1x1x1x/9/9/X1X1X1X1X/1X5X1/9/XXXXKXXXX w A2B2N2R2C2P5a2b2n2r2c2p5 0 1",
        "xxxxkxxxx/9/1x5x1/x1x1x1x2/8b/6B2/X1X1X3X/1X5X1/9/XXXXKXXXX w R2r2N2n2B1b1A2a2C2c2P5p5 0 2",
        "xxxxkxxxx/9/1x5x1/x1x1x4/6r1b/6B1P/X1X1X4/1X5X1/9/XXXXKXXXX w R2r1N2n2B1b1A2a2C2c2P4p5 0 3",
        "xxxxkxxxx/9/1x5x1/x1x1x4/8b/A5r1P/2X1X4/1X5X1/9/XXXXKXXXX w R2r1N2n2B1b1A1a2C2c2P4p5 0 4",
        "xxxxkxxxx/9/1x5x1/2x1x4/r7P/A5r2/2X1X4/1X5X1/9/XXXXKXXXX w R2N2n2B1b1A1a2C2c2P4p5 0 5",
        "xxxxkxxxx/9/1x5x1/2x1x4/r7P/A8/2X1X4/1XP4X1/9/X1XXKXrXX w R2N2n2b1A1a2C2c2P3p5 0 6",
        "xxxx1xxxx/4k4/1x5x1/2x1x4/r7P/A8/2X1X4/1XP4X1/4P4/X1X1KXrXX w R2N2n2b1A1a2C2c2P2p5 1 7",
        "xxxx1xxxx/4k4/1x7/2x1x4/r7P/A3N4/2X6/1XP4X1/4P4/X1X1KXrpX w R2N1n2b1A1a2C1c2P2p4 0 8",
        "xxxx1xxxx/4k1r2/1x7/2x1x4/r7P/A8/2X6/1XP2N1X1/4P4/X1X1KX1pX w R2N1n2b1A1a2C1c2P2p4 2 9",
        "xxxx1xxxx/4k1r2/1x7/2x1x4/8r/A8/2X6/1XP2N1X1/4P4/X1X1KX1R1 w R1N1n2b1A1a2C1c2P2p4 0 10",
        "xxxx1xxxx/4k1rA1/1x7/2x1x4/9/A8/2X6/1XP2N3/4P4/X1X1KX1Rr w R1N1n2b1a2C1c2P2p4 1 11",
        "xxxx1xxx1/4k1rA1/1x7/2x1x4/9/A8/2X6/1XP2N3/4P4/X1X1KX2p w R1N1n2b1a2C1c2P2p3 0 12",
        "xxxx1xxx1/4k2r1/1x7/2x1x4/9/A1N6/9/1XP2N3/4P4/X1X1KX2p w R1n2b1a2C1c2P2p3 0 13",
        "xxxx1xxx1/5k1r1/1x7/2xNx4/9/A8/9/1XP2N3/4P4/X1X1KX2p w R1n2b1a2C1c2P2p3 2 14",
        "xxxx1xxx1/5k3/1x7/2xNx4/9/A6r1/C8/1XP2N3/4P4/2X1KX2p w R1n2b1a2c2P2p3 1 15",
        "xxxx1xxx1/5k3/1x7/2x6/4b4/A3N2r1/C8/1XP2N3/4P4/2X1KX2p w R1n2a2c2P2p3 0 16",
        "C1xx1xxx1/5k3/1xc6/2x6/4b4/A3N2r1/9/1XP2N3/4P4/2X1KX2p w R1n2a1c1P2p3 0 17",
        "C1xx2xx1/4ak3/1xc6/2x6/4b4/A3N2r1/9/1XP1PN3/9/2X1KX2p w R1n2c1P2p3 0 18",
        "2xx2xx1/C4k3/1xca5/2x6/4b4/A3N2r1/9/1XP1PN3/9/2X1KX2p w R1n2c1P2p3 2 19",
        "2xx2xx1/C4k3/1x1a5/1Px6/4b4/A3N2r1/9/2c1PN3/9/2X1KX2p w R1n2c1P1p3 0 20",
        "2xx2xx1/C4k3/1x1a5/1Px6/4b4/A3N2r1/9/P3Pc3/9/4KX2p w R1n2c1p3 0 21",
        "2xx2xx1/C4k3/1x1a5/1Px6/4b4/A1r6/9/P3PN3/9/4KX2p w R1n2c1p3 1 22",
        "2xx2xx1/C4k3/1x1a5/1Px6/1A2b4/9/9/P3PN3/9/2r1KX2p w R1n2c1p3 3 23",
        "2xx2xx1/C4k3/1x1a5/1Px6/1A2b4/9/9/P3PN3/2r1K4/5X2p w R1n2c1p3 5 24",
        "2xx2xx1/C4k3/1x1a5/1Px6/1A2b4/9/9/P3PN3/9/2r1KX2p w R1n2c1p3 7 25",
        "2xx2xx1/C4k3/1x1a5/1Px6/1A2b4/9/9/P3PN3/2r1K4/5X2p w R1n2c1p3 9 26",
        "2xx2xx1/C4k3/3a5/1Px6/1c2b4/9/9/P3PN3/2r6/4KX2p w R1n2p3 0 27",
        "2xx2xx1/C4k3/3a5/1Px6/1c2b4/4N4/9/P3P4/9/2r1KX2p w R1n2p3 2 28",
        "2xx2xx1/C4k3/3a5/1Px6/1c2b4/4N4/9/P3P4/4K4/5r2p w n2p3 0 29",
        "2xx2xx1/C4k3/3a5/2P6/1c2b4/4N4/9/P3P4/4Kr3/8p w n2p2 1 30",
        "2xx2xx1/C4k3/3a5/2P6/1c2b4/4N4/9/P3P4/9/4Kr2p w n2p2 3 31",
        "2xx2xx1/C4k3/9/2a6/1c2b4/4N4/9/P3P4/4K4/5r2p w n2p2 0 32",
        "2xx2xx1/5k3/9/2a6/Cc2b4/4Nr3/9/P3P4/4K4/8p w n2p2 2 33",
        "2xx2xx1/5k3/9/2a6/Cc7/4Nr3/2b1P4/P8/4K4/8p w n2p2 4 34",
        "2xx2xx1/5k3/9/2a6/C3c4/5r3/2N1P4/P8/4K4/8p w n2p2 1 35",
        "2xx2xx1/5k3/9/2a6/C3c4/9/4P4/P3Nr3/4K4/8p w n2p2 3 36",
        "2xx2xx1/5k3/9/2a6/C3c4/9/P3P4/4r4/4K4/8p w n2p2 0 37",
        "2xx2xx1/5k3/9/2a6/C3c4/9/P3r4/9/3K5/8p w n2p2 0 38",
        "2xx2xx1/4k4/9/2a6/2C1c4/9/P3r4/9/3K5/8p w n2p2 2 39",
        "2xx2xx1/4k4/9/2a6/2Cc5/P8/4r4/9/3K5/8p w n2p2 4 40",
        "2xx2xx1/4k4/9/2a6/3c5/P8/3r5/2C6/3K5/8p w n2p2 6 41",
        "2xx2xx1/4k4/9/2a6/9/P8/3r5/3c5/3K5/8p w n2p2 0 42",
        "2xx2xx1/4k4/9/2a6/P8/9/3r5/8c/3K5/8p w n2p2 2 43",
    },
    {
        "xxxxkxxxx/9/1x5x1/x1x1x1x1x/9/6R2/X1X1X3X/1X5X1/9/XXXXKXXXX b R1r2N2n2B2b2A2a2C2c2P5p5 0 1",
        "xxxxkxxxx/9/1x5x1/x1x1x1x2/8a/6R1N/X1X1X4/1X5X1/9/XXXXKXXXX b R1r2N1n2B2b2A2a1C2c2P5p5 0 2",
        "x1xxkxxxx/9/1xn4x1/x1x1x1x2/8a/2B3R1N/X3X4/1X5X1/9/XXXXKXXXX b R1r2N1n1B1b2A2a1C2c2P5p5 0 3",
        "x1xxkxxxx/9/1xn4x1/2x1x1x2/p7a/2B1R1R1N/X8/1X5X1/9/XXXXKXXXX b r2N1n1B1b2A2a1C2c2P5p4 0 4",
        "x1xxkxx1x/9/1xn3ax1/2x1x1R2/p7a/2B1R3N/X8/1X5X1/9/XXXXKXXXX b r2N1n1B1b2A2C2c2P5p3 0 5",
        "x1xxk1x1x/4b4/1xn3ax1/2x1x4/p7a/2B1R1R1N/X8/1X5X1/9/XXXXKXXXX b r2N1n1B1b1A2C2c2P5p3 1 6",
        "x1xxk1x1x/4b4/1xn3ax1/4B4/p1c5a/4R1R1N/X8/1X5X1/9/XXXXKXXXX b r2N1n1B1b1A2C2P5p3 0 7",
        "x1xxk1x1x/4b4/1xn3a2/4B4/p1c3R1a/4R2pN/X8/1X5X1/9/XXXXKXXXX b r2N1n1B1b1A2C2P5p2 1 8",
        "x1xxk1x1x/4b4/1xn3a2/1Pc1B4/p5R1a/4R2pN/X8/7X1/9/XXXXKXXXX b r2N1n1B1b1A2C2P4p2 0 9",
        "x1xxk1x1x/4b4/2n3R2/1Pc1B4/p7a/4R2pN/X8/7X1/9/XbXXKXXXX b r2N1n1B1A2C1P4p2 0 10",
        "x1xxk3x/4b4/2n1R4/1Pc1B4/p7a/4R2pN/X8/7X1/9/XbXXKXXXX b r1N1n1B1A2C1P4p2 0 11",
        "x2xk3x/4b1B2/2n1p4/1Pc6/p7a/4R2pN/X8/7X1/9/XbXXKXXXX b r1N1n1B1A2C1P4p1 1 12",
        "x2xk3x/4b4/2n1p4/1P2B4/p7a/4R2pN/X8/7X1/9/XbXXKXXXX b r1N1n1B1A2C1P4p1 0 13",
        "x2xk3x/4b4/4p4/1P2n4/p3R3a/7pN/X8/7X1/9/XbXXKXXXX b r1N1n1B1A2C1P4p1 1 14",
        "x2xk3x/4b2P1/4p4/1P2n4/p3R3a/8p/X8/9/9/XbXXKXXXX b r1N1n1B1A2C1P3p1 0 15",
        "x2xk3x/7P1/4p4/1P2n1b2/p3R3a/8p/X8/9/9/1AXXKXXXX b r1N1n1B1A1C1P3p1 0 16",
        "x2xkr3/7P1/4p4/1P2n1b2/p7R/8p/X8/9/9/1AXXKXXXX b N1n1B1A1C1P3p1 0 17",
        "x3kr3/4p2P1/4p4/1P2n1b2/p7R/8p/X8/9/4K4/1AXX1XXXX b N1n1B1A1C1P3 1 18",
        "x3kr3/4p2P1/4p4/1P4b2/p7R/3n4p/X8/4C4/4K4/1AXX1X1XX b N1n1B1A1P3 0 19",
        "x3kr3/4p2P1/8R/1P2p1b2/p8/3n4p/X8/4C4/4K4/1AXX1X1XX b N1n1B1A1P3 2 20",
        "x3kr3/4p2P1/8R/1P2p1b2/p8/8p/X4n3/4C4/3K5/1AXX1X1XX b N1n1B1A1P3 4 21",
        "x3k4/4p2P1/8R/1P2p1b2/p8/5r2p/X4n3/4C4/2AK5/2XX1X1XX b N1n1B1A1P3 6 22",
        "x3k4/4p1P2/8R/1P2p1b2/9/p4r2p/X4n3/4C4/2AK5/2XX1X1XX b N1n1B1A1P3 8 23",
        "x3k4/4p1P2/8R/1P2p1b2/9/p4r2p/X8/7C1/2AK2n2/2XX1X1XX b N1n1B1A1P3 10 24",
        "x3k4/4p1P2/7R1/1P2p1b2/9/p4r1p1/X8/7C1/2AK2n2/2XX1X1XX b N1n1B1A1P3 12 25",
        "x3k4/6P2/4R4/1P2p1b2/9/p4r1p1/X8/7C1/2AK2n2/2XX1X1XX b N1n1B1A1P3 0 26",
        "x3k4/4b1P2/1P2R4/4p4/9/p4r1p1/X8/7C1/2AK2n2/2XX1X1XX b N1n1B1A1P3 2 27",
        "x3k4/4b1P2/1P5R1/4p4/9/p4r1p1/X8/4n2C1/2AK5/2XX1X1XX b N1n1B1A1P3 4 28",
        "x3k4/6P2/2P4R1/4p1b2/9/p4r1p1/X8/4n2C1/2AK5/2XX1X1XX b N1n1B1A1P3 6 29",
        "x8/4k1P2/3P3R1/4p1b2/9/p4r1p1/X8/4n2C1/2AK5/2XX1X1XX b N1n1B1A1P3 8 30",
        "5n3/4k1P2/3P3R1/4p1b2/9/p4r1p1/X8/4n3C/2AK5/2XX1X1XX b N1B1A1P3 1 31",
        "5n3/4k1P2/3P3R1/4p4/9/P4r1pb/9/4n3C/2AK5/2XX1X1XX b N1B1A1P2 0 32",
        "5n3/4k1P2/3P3R1/4p4/9/P4r2b/7p1/3An3C/3K5/2XX1X1XX b N1B1A1P2 2 33",
        "5n3/4k1P2/3P3R1/4p4/9/P4r2b/6p2/3An3C/3KP4/2X2X1XX b N1B1A1P1 0 34",
        "5n3/4k1P2/3P5/4p2R1/9/P4r2b/6p2/3A4C/3KP1n2/2X2X1XX b N1B1A1P1 2 35",
        "9/4k1PR1/3P2n2/4p4/9/P4r2b/6p2/3A4C/3KP1n2/2X2X1XX b N1B1A1P1 4 36",
        "9/4k1PR1/3P2n2/4p4/9/P4r2b/6p2/3A4A/3KP4/2X2X2X b N1B1P1 0 37",
        "9/4kP1R1/3P2n2/4p4/9/P4r3/6p2/3A2b1A/3KP4/2X2X2X b N1B1P1 2 38",
        "9/3Pkr1R1/6n2/4p4/9/P8/6p2/3A2b1A/3KP4/2X2X2X b N1B1P1 1 39",
        "9/3k1R3/6n2/4p4/9/P8/6p2/3A2b1A/3KP4/2X2X2X b N1B1P1 0 40",
        "3k5/5R3/6n2/4p4/9/P8/6p2/3A2b2/3KP2A1/2X2X2X b N1B1P1 2 41",
        "3k5/9/6n2/4p4/9/P4R2b/6p2/3A5/3KP2A1/2X2X2X b N1B1P1 4 42",
        "3k5/9/5Rn2/4p4/9/P8/6p2/3A2b2/3KP2A1/2X2X2X b N1B1P1 6 43",
        "3k5/9/9/4p4/5R1n1/P8/6p2/3A2b2/3KP2A1/2X2X2X b N1B1P1 8 44",
        "3k5/9/6n2/4p4/6R2/P8/6p2/3A2b2/3KP2A1/2X2X2X b N1B1P1 10 45",
        "3k5/4n4/9/4p4/9/P8/6R2/3A2b2/3KP2A1/2X2X2X b N1B1P1 0 46",
        "3k5/4n4/9/4p4/9/P5R1b/9/3A5/3KP2A1/2X2X2X b N1B1P1 2 47",
        "3k5/9/9/4pn3/9/P7R/9/3A5/3KP2A1/2X2X2X b N1B1P1 0 48",
        "3k5/9/7n1/4p4/9/P7R/9/3AP4/3K3A1/2X2X2X b N1B1P1 2 49",
        "3k5/5n3/9/4p3R/9/P8/9/3AP4/3K3A1/2X2X2X b N1B1P1 4 50",
        "9/3k1n3/9/4p3R/P8/9/9/3AP4/3K3A1/2X2X2X b N1B1P1 6 51",
        "9/3k1n3/9/3R5/P3p4/9/9/3AP4/3K3A1/2X2X2X b N1B1P1 8 52",
        "9/4kn3/9/3R5/P3p4/9/4P4/3A5/3K3A1/2X2X2X b N1B1P1 10 53",
        "4k4/5n3/9/3R5/1P2p4/9/4P4/3A5/3K3A1/2X2X2X b N1B1P1 12 54",
        "4k4/9/7n1/4R4/1P2p4/9/4P4/3A5/3K3A1/2X2X2X b N1B1P1 14 55",
        "3k5/9/7n1/9/1P2R4/9/4P4/3A5/3K3A1/2X2X2X b N1B1P1 0 56",
        "9/3k5/7n1/9/1P3R3/9/4P4/3A5/3K3A1/2X2X2X b N1B1P1 2 57",
        "9/4k4/7n1/9/1P3R3/4P4/9/3A5/3K3A1/2X2X2X b N1B1P1 4 58",
        "6n2/4k4/9/9/1P2PR3/9/9/3A5/3K3A1/2X2X2X b N1B1P1 6 59",
        "9/4k3n/9/4P4/1P3R3/9/9/3A5/3K3A1/2X2X2X b N1B1P1 8 60",
        "9/4k4/9/4P2n1/1P2R4/9/9/3A5/3K3A1/2X2X2X b N1B1P1 10 61",
        "9/4k4/4P4/9/1P2R4/6n2/9/3A5/3K3A1/2X2X2X b N1B1P1 12 62",
        "4k4/4P4/9/9/1P2R4/6n2/9/3A5/3K3A1/2X2X2X b N1B1P1 14 63",
        "5k3/4P4/9/9/1P3R3/6n2/9/3A5/3K3A1/2X2X2X b N1B1P1 16 64",
        "5k3/4P4/9/5n3/P4R3/9/9/3A5/3K3A1/2X2X2X b N1B1P1 18 65",
    },
    {
        "xxxxkxxxx/9/1x5x1/x1x1x1x1x/9/9/X1X1X1X1X/1X5X1/9/XXXXKXXXX w A2B2N2R2C2P5a2b2n2r2c2p5 0 1",
        "xxxxkxxxx/9/1x5x1/x1x1x3x/6b2/8P/X1X1X1X2/1X5X1/9/XXXXKXXXX w R2r2N2n2B2b1A2a2C2c2P4p5 0 2",
        "xxxxkxxxx/9/1x5x1/2x1x3x/p5b2/6N1P/X1X1X4/1X5X1/9/XXXXKXXXX w R2r2N1n2B2b1A2a2C2c2P4p4 0 3",
        "xxxxkxxxx/9/1x5x1/4x3x/p1n3b2/6N1P/X1X1X4/1X4RX1/9/XXXXKXX1X w R1r2N1n1B2b1A2a2C2c2P4p4 0 4",
        "x1xxkxxxx/9/1xp4x1/4x3x/p1n3b2/2C3N1P/X3X4/1X4RX1/9/XXXXKXX1X w R1r2N1n1B2b1A2a2C1c2P4p3 0 5",
        "x1xxk1xxx/4n4/1xp4x1/4x3x/p1n3b2/2C3N1P/X3X4/1X4RX1/4P4/XXX1KXX1X w R1r2N1B2b1A2a2C1c2P3p3 0 6",
        "x1xxk1xxx/4n4/1xp4x1/4x3x/p1n6/2C5P/X3b4/1X3NRX1/4P4/XXX1KXX1X w R1r2B2b1A2a2C1c2P3p3 0 7",
        "x1xxk1xxx/4n4/1xp4x1/8x/p1n1p4/2C5P/X3b4/1X2PNRX1/9/XXX1KXX1X w R1r2B2b1A2a2C1c2P3p2 0 8",
        "x1xxk1x1x/4n4/1xp3ax1/8x/p1n1p4/2C5P/X3P4/1X3NRX1/9/XXX1KXX1X w R1r2B2b1A2a1C1c2P3p2 0 9",
        "x1xxk3x/4n4/1xp1c1ax1/8x/p1n1p4/8P/X3P4/1X3NRX1/2C6/XXX1KXX1X w R1r2B2b1A2a1C1c1P3p2 0 10",
        "x1xxk3x/4n4/1xp1c1ax1/8x/p3p4/8P/Xn2P4/1XB2NRX1/2C6/X1X1KXX1X w R1r2B1b1A2a1C1c1P3p2 1 11",
        "x1xxk3x/4n4/2p1c1ax1/8x/p3p4/8P/Xn2P4/1rB2NRX1/1C7/X1X1KXX1X w r1B1b1A2a1C1c1P3p2 0 12",
        "x1xxk3x/4n4/2p1c1ax1/8x/p3p4/8P/Xr2P4/2B2NRX1/9/X1X1KXX1X w r1B1b1A2a1C1c1P3p2 0 13",
        "x1xxk3x/4n4/2p1c1ax1/8x/pr2p4/8P/X3P4/2B1PNRX1/9/X3KXX1X w r1B1b1A2a1C1c1P2p2 1 14",
        "x1xxk3x/4n4/2p2cax1/8x/pr2p4/8P/X3P4/2B1PN1X1/6R2/X3KXX1X w r1B1b1A2a1C1c1P2p2 3 15",
        "x1xxk3x/9/2p2cax1/5n2x/pr2p4/8P/X3P4/2B1PN1X1/4P1R2/X3K1X1X w r1B1b1A2a1C1c1P1p2 1 16",
        "x1x1k3x/4c4/2p2cax1/5n2x/pr2p4/6N1P/X3P4/2B1P2X1/4P1R2/X3K1X1X w r1B1b1A2a1C1P1p2 0 17",
        "x1x1k3x/4c4/2p2c1x1/5a2x/pr2p4/8P/X3P4/2B1P2X1/4P1R2/X3K1X1X w r1B1b1A2a1C1P1p2 0 18",
        "x1x1k3x/5c3/2p2c1x1/5aR1x/pr2p4/8P/X3P4/2B1P2X1/4P4/X3K1X1X w r1B1b1A2a1C1P1p2 2 19",
        "x1x1k3x/5c3/2p2cp2/5aRBx/pr2p4/8P/X3P4/2B1P4/4P4/X3K1X1X w r1b1A2a1C1P1p1 0 20",
        "x1x1k3x/5c3/2p2cp2/5a1B1/pr2p3a/6R1P/X3P4/2B1P4/4P4/X3K1X1X w r1b1A2C1P1p1 0 21",
        "x1x1k3x/5c3/2p2cp2/5a1B1/1r2p3a/p5R1P/4P4/2B1P4/4P4/X3K1X1X w r1b1A1C1P1p1 0 22",
        "x1x1k3x/8c/2p2cp2/5a1B1/1r2p3P/p5R2/4P4/2B1P4/4P4/X3K1X1X w r1b1A1C1P1p1 1 23",
        "x3k3x/8c/2p1rcp2/5a1B1/1r2p3P/p5R2/4P4/2B1P3P/4P4/X3K3X w b1A1C1p1 0 24",
        "x3k3x/8c/2p2cp2/4ra1B1/1r2p3P/p2R5/4P4/2B1P3P/4P4/X3K3X w b1A1C1p1 2 25",
        "x3k3x/8c/2p2cp2/4ra1B1/4p3P/p2R5/1r2P3P/2B1P4/4P4/X3K3X w b1A1C1p1 4 26",
        "x3k3x/8c/2p2cp2/2r2a3/4p3P/p2R1B3/1r2P3P/2B1P4/4P4/X3K3X w b1A1C1p1 6 27",
        "x3k3x/8c/2p2cp2/5a3/4p3P/p2R1B3/1r2P3P/2r1P4/4P4/X3K2C1 w b1A1p1 0 28",
        "x3k3x/9/2p2cp2/5a3/4p3P/p2R5/1r2P3c/2r1P2B1/4P4/X3K2C1 w b1A1p1 0 29",
        "x3k3x/9/2p2cp2/2r2a3/4p3P/p2R5/1r2P3c/4P2B1/4P4/X3K3C w b1A1p1 2 30",
        "x3k3x/9/2p2cp2/2r2a3/4p2P1/p2R5/1r2P1c2/4P2B1/4P4/X3K3C w b1A1p1 4 31",
        "x3k3x/9/2p2cp2/2r2a3/4p3P/p2R5/1r2P3c/4P2B1/4P4/X3K3C w b1A1p1 6 32",
        "x3k3x/9/2p2cp2/2r2a3/4p3P/1p1R5/1r2P3c/4P4/4P4/X3KB2C w b1A1p1 8 33",
        "x3k3x/5c3/2p3p2/2r2a3/4p3P/1p1R5/1r2P3c/4P4/4P4/3AKB2C w b1p1 1 34",
        "x3k3x/c8/2p3p2/2r2a3/3Rp3P/1p7/1r2P3c/4P4/4P4/3AKB2C w b1p1 3 35",
        "x3k3x/c8/2p1a1p2/2r6/R3p3P/1p7/1r2P3c/4P4/4P4/3AKB2C w b1p1 5 36",
        "x3k3x/c8/2p1a1p2/2r6/R7P/1p2p4/1r6c/4P4/4P4/3AKB2C w b1p1 0 37",
        "x3k3x/c8/2p1a1p2/2r6/R7P/1p7/1r2p3c/9/4P4/3AKB2C w b1p1 0 38",
        "x3k3x/c8/2p1a1p2/2r6/R6P1/1p7/1r2p2c1/9/4P4/3AKB2C w b1p1 2 39",
        "x3k3x/c8/2p1a1p2/2r6/R6P1/1p7/1r3p1c1/9/4P4/3AKB1C1 w b1p1 4 40",
        "x3k3x/c8/2p1a1p2/2r6/R6P1/1p7/1r3p3/7B1/4P4/3AK2c1 w b1p1 0 41",
        "x3k3x/R8/2p1a1p2/2r6/7P1/1p7/1r3p3/7B1/4P4/3cK4 w b1p1 0 42",
        "R2ck3x/9/2p1a1p2/2r6/7P1/1p7/1r3p3/7B1/4P4/4K4 w p1 1 43",
        "3ck3x/9/2p1a1p2/9/7P1/1p7/1r3p3/7B1/2r1P4/R3K4 w p1 3 44",
        "3ck3x/9/2p1a1p2/9/7P1/1p7/1r7/4Pp1B1/2r6/R3K4 w p1 5 45",
        "3ck3x/9/2p1a1p2/9/8P/1p7/1r7/4p2B1/2r6/R3K4 w p1 0 46",
        "3ck3x/9/2p1a1p2/9/7P1/1p7/1r7/7B1/2r1p4/R3K4 w p1 2 47",
        "3ck3x/9/2p1a1p2/9/7P1/1p7/1r7/7B1/3rp4/R2K5 w p1 4 48",
    },
    {
        "xxxxkxxxx/9/1x5x1/x1x1x1x1x/9/6P2/X1X1X3X/1X5X1/9/XXXXKXXXX b R2r2N2n2B2b2A2a2C2c2P4p5 0 1",
        "xxxxkxxxx/9/1x5x1/x1x1x1x2/8b/6P1N/X1X1X4/1X5X1/9/XXXXKXXXX b R2r2N1n2B2b1A2a2C2c2P4p5 0 2",
        "xxxxkxxxx/9/1x5x1/2x1x1x2/n7b/B5P1N/2X1X4/1X5X1/9/XXXXKXXXX b R2r2N1n1B1b1A2a2C2c2P4p5 0 3",
        "xxxxkxxxx/9/1x5x1/4x1x2/n1p5b/B3A1P1N/2X6/1X5X1/9/XXXXKXXXX b R2r2N1n1B1b1A1a2C2c2P4p4 0 4",
        "x1xxkxxxx/9/1xr4x1/4x1x2/n1p5b/B3A1P1N/2X6/1XC4X1/9/X1XXKXXXX b R2r1N1n1B1b1A1a2C1c2P4p4 0 5",
        "x1xxkxxxx/9/1xr3bx1/4x1x2/n1p6/B3A1P1N/2X6/1XC4X1/4B4/X1XXK1XXX b R2r1N1n1b1A1a2C1c2P4p4 0 6",
        "x1xxkxxxx/9/1xr3bx1/6x2/n1p1p4/B3A1P1N/2X3B2/1XC4X1/9/X1XXK1XXX b R2r1N1n1b1A1a2C1c2P4p3 1 7",
        "x1x1kxxPx/4c4/1xr3bx1/6x2/n1p1p4/B3A1P1N/2X3B2/1XC6/9/X1XXK1XXX b R2r1N1n1A1a2C1c1P3p3 0 8",
        "x1x1kxxPx/4c4/1xr3bx1/6x2/n1p6/B3p1P1N/2X3B2/1XC6/4P4/X1X1K1XXX b R2r1N1n1A1a2C1c1P2p3 0 9",
        "x1x1kxP2/4c4/1xr3bx1/6x2/n1p6/B3p1P1a/2X3B2/1XC6/4P4/X1X1K1XXX b R2r1N1n1A1a1C1P2p3 0 10",
        "x1x1kxP2/4c4/1xr3bx1/6x2/n1p6/B3p1P2/2X3Ba1/1XC1N4/4P4/X1X1K2XX b R2r1n1A1a1C1P2p3 0 11",
        "x1x1kxP2/4c4/1xr3bx1/6x2/n1p6/B5P2/2X1p1Ba1/1XC1N4/4PA3/X1X1K3X b R2r1n1a1C1P2p3 0 12",
        "x1x1kxP2/4c4/2r3bx1/6x2/n1p6/B5P2/1rX1p1Ba1/2C1N4/4PA3/XPX1K3X b R2n1a1C1P1p3 0 13",
        "x1x1kxP2/1r2c4/2r3bx1/6x2/n1p6/B5P2/2X1p1Ba1/P1C1N4/4PA3/XP2K3X b R2n1a1C1p3 0 14",
        "x1x1kxP2/1r2c4/3r2bx1/6x2/n1p6/B5P2/2X1p1Ba1/P1C1N4/1P2PA3/X3K3X b R2n1a1C1p3 2 15",
        "x1xk1xP2/1r2c4/3r2bx1/6x2/n1p6/B5P2/2X1p1Ba1/P1C1N4/1P2PA3/X4K2X b R2n1a1C1p3 4 16",
        "x1xk1xP2/1r3c3/3r2bx1/6x2/n1p6/B5P2/2X1p1Ba1/P1C1N1A2/1P2P4/X4K2X b R2n1a1C1p3 6 17",
        "x1xk1xP2/1r3c3/3r2bx1/6x2/n1p6/B5P2/2X2pBa1/P1C1N1A2/1P2P4/X3K3X b R2n1a1C1p3 8 18",
        "x1xk1xP2/3r1c3/3r2bx1/6x2/n1p6/B5P2/2X2pBa1/P1C1N1A2/1P2P4/X3KR3 b R1n1a1C1p3 0 19",
        "x1xk1xP2/3r1c3/6bx1/6x2/n1p6/B5P2/2X2pBa1/P1C1N1A2/1P2P4/3CKR3 b R1n1a1p3 0 20",
    },
    {
        "xxxxkxxxx/9/1x5x1/x1x1x1x1x/9/9/X1X1X1X1X/1X5X1/9/XXXXKXXXX w A2B2N2R2C2P5a2b2n2r2c2p5 0 1",
        "xxxxkxxxx/9/1x5x1/x1x1x3x/6p2/8P/X1X1X1X2/1X5X1/9/XXXXKXXXX w R2r2N2n2B2b2A2a2C2c2P4p4 0 2",
        "xxxxkxxxx/9/1x5x1/x3x3x/2p3p2/P7P/2X1X1X2/1X5X1/9/XXXXKXXXX w R2r2N2n2B2b2A2a2C2c2P3p3 0 3",
        "xxx1kxxxx/4r4/1x5x1/x3x3x/2p3p2/P7P/2X1X1X2/1X5X1/4C4/XXXXK1XXX w R2r1N2n2B2b2A2a2C1c2P3p3 0 4",
        "x1x1kxxxx/4r4/1xc4x1/x3x3x/2p3p2/P3N3P/2X3X2/1X5X1/4C4/XXXXK1XXX w R2r1N1n2B2b2A2a2C1c1P3p3 0 5",
        "x1x1kxxxx/4r4/1xc4x1/x7x/2p1p1N2/P7P/2X3X2/1X5X1/4C4/XXXXK1XXX w R2r1N1n2B2b2A2a2C1c1P3p2 0 6",
        "x1x1kxx1x/4r4/1xc3bx1/x7x/2p1p1N2/P5N1P/2X6/1X5X1/4C4/XXXXK1XXX w R2r1n2B2b1A2a2C1c1P3p2 0 7",
        "x1x1kxx1x/4r4/1x4bc1/x7x/2p1p4/P5N1P/2X6/1X5X1/4C4/XXXXK1XXX w R2r1n2B2b1A2a1C1c1P3p2 0 8",
        "x1x1kx2x/4r4/1x4bcp/x4N2x/2p1p4/P7P/2X6/1X5X1/4C4/XXXXK1XXX w R2r1n2B2b1A2a1C1c1P3p1 0 9",
        "x3kx2x/4r4/1x2n1bcp/x4N2x/2p1p4/P7P/2X6/1X4PX1/4C4/XXXXK1X1X w R2r1n1B2b1A2a1C1c1P2p1 0 10",
        "x3kx2x/3r5/1x2n1bNp/x7x/2p1p4/P7P/2X6/1X4PX1/4C4/XXXXK1X1X w R2r1n1B2b1A2a1C1c1P2p1 1 11",
        "x3kx2x/3r5/4n1bNp/x7x/2p1p4/P7P/2X6/PX4PX1/4C4/Xc1XK1X1X w R2r1n1B1b1A2a1C1P1p1 0 12",
        "x3kx3/3r4r/4n1bNp/x7x/2p1p4/P7P/2X6/PX4PX1/4C4/1C1XK1X1X w R2n1B1b1A2a1P1p1 0 13",
        "x3kx3/8r/4C1bNp/x7x/2p1p4/P7P/2X6/PX4PX1/3r5/1C1XK1X1X w R2n1B1b1A2a1P1p1 1 14",
        "x3kx3/5r3/5CbNp/x7x/2p1p4/P7P/2X6/PX4PX1/3r5/1C1XK1X1X w R2n1B1b1A2a1P1p1 3 15",
        "x3kx3/5r3/5Cb1p/x7x/2p1p1N2/P7P/2X6/PX4PX1/1r7/1C1XK1X1X w R2n1B1b1A2a1P1p1 5 16",
        "x3kx3/5r3/5Cb1p/x7x/2p1p1N2/P7P/2X6/PX4PX1/2r6/2CXK1X1X w R2n1B1b1A2a1P1p1 7 17",
        "x3kx3/5r3/5Cb1p/x7x/2p1p1N2/P7P/2X6/PX4PX1/1r7/C2XK1X1X w R2n1B1b1A2a1P1p1 9 18",
        "x3kx3/5r3/5Cb1p/x7x/4p1N2/P1p5P/9/PX4PX1/1r7/C2XK1X1X w R1n1B1b1A2a1P1p1 0 19",
        "x3kx3/5r3/5Cb1p/x7x/4p1N2/P1p5P/9/P1R3PX1/9/Cr1XK1X1X w n1B1b1A2a1P1p1 1 20",
        "x3kx3/5r3/5Cb1p/x7x/4p1N2/P1p5P/9/P1R3PX1/Cr7/3XK1X1X w n1B1b1A2a1P1p1 3 21",
        "x3kx3/5r3/5Cb1p/x8/4p1N1a/P1p5P/9/P1R3PX1/1r7/C2XK1X1X w n1B1b1A2P1p1 0 22",
        "x3kx3/5r3/5C2p/x8/4p1N1b/P1p6/9/P1R3PX1/1r7/C2XK1X1X w n1B1b1A2P1p1 0 23",
        "x3kx3/5r3/5C2p/x8/4p1N1b/P1R6/9/P5PX1/3r5/C2XK1X1X w n1B1b1A2P1p1 1 24",
        "x2k1x3/5r3/5C2p/x8/4p1N1b/P1R6/9/P5PX1/3rP4/C3K1X1X w n1B1b1A2p1 1 25",
        "x2k1x3/9/8p/x8/4p1N1b/P1R6/9/P5PX1/3rPr3/C3KCX1X w n1B1b1A2p1 3 26",
        "x2k1x3/9/8p/x8/4p3b/P1R6/5r3/P5PX1/3rP4/C3KCX1X w n1B1b1A2p1 0 27",
        "x2k1x3/9/8p/x8/4p3b/P1R6/5r3/P5PX1/1r2P4/3CKCX1X w n1B1b1A2p1 2 28",
        "x2k1x3/9/8p/x8/4p3b/P1R6/7r1/P3A1PX1/1r2P4/3CKC2X w n1B1b1A1p1 1 29",
        "x3kx3/9/2R5p/x8/4p3b/P8/7r1/P3A1PX1/1r2P4/3CKC2X w n1B1b1A1p1 3 30",
        "x3k4/4n4/4R3p/x8/4p3b/P8/7r1/P3A1PX1/1r2P4/3CKC2X w B1b1A1p1 0 31",
        "x3k4/4n4/4R3p/x6r1/4p3B/P8/9/P3A1PX1/1r2P4/3CKC3 w b1A1p1 1 32",
        "2b1k4/4n4/4R3p/x6r1/4p3B/P8/9/P3A1PX1/1r2P4/3CK2C1 w A1p1 0 33",
        "2b1k4/4n4/8p/xr5r1/4R3B/P8/9/P3A1PX1/4P4/3CK2C1 w A1p1 1 34",
        "2b1k4/4n4/8p/x6r1/4R3B/P8/9/P3A1PX1/4P4/3CK4 w A1p1 0 35",
        "2b1k4/4n4/8p/x5r2/4R3B/P8/9/P2CA1PX1/4P4/4K4 w A1p1 2 36",
        "2b1k4/4n4/8p/x6r1/4R3B/P8/5A3/P2C2PX1/4P4/4K4 w A1p1 4 37",
        "2bk5/4n4/8p/x6r1/4R3B/P8/5A3/P3C1PX1/4P4/4K4 w A1p1 6 38",
        "2bk5/4R4/8p/x1r6/8B/P8/5A3/P3C1PX1/4P4/4K4 w A1p1 1 39",
        "2bk5/4R4/8p/x8/8B/P8/5A3/P3C1PX1/4P4/2r2K3 w A1p1 3 40",
        "2bk5/4R4/8p/x8/8B/P8/5A3/P1r1C1PX1/4PK3/9 w A1p1 5 41",
        "2bk5/4R4/2r5p/x8/8B/P8/5A3/P2C2PX1/4PK3/9 w A1p1 7 42",
        "2b6/3k5/2r5p/x8/8B/P8/4RA3/P2C2PX1/4PK3/9 w A1p1 9 43",
        "2bk5/9/2r5p/x8/8B/P8/4RA3/P2C1KPX1/4P4/9 w A1p1 11 44",
        "2b1k4/9/2r5p/x8/8B/P8/3R1A3/P2C1KPX1/4P4/9 w A1p1 13 45",
        "2b6/4k4/2r5p/x8/8B/P8/3R1A3/P2C2PX1/4PK3/9 w A1p1 15 46",
        "2b6/4k4/5r2p/x8/8B/P8/3R1A3/P3C1PX1/4PK3/9 w A1p1 17 47",
        "2b6/4k4/5rB2/x7p/9/P8/3R1A3/P3C1PX1/4PK3/9 w A1p1 19 48",
        "2b6/4k4/5rB2/8p/p8/P8/3R1A3/P3C1PX1/4P4/5K3 w A1 0 49",
        "2b6/4k4/5rB2/9/P7p/9/3R1A3/P3C1PX1/4P4/5K3 w A1 1 50",
        "2b6/4k4/5rBA1/9/P8/8p/3R1A3/P3C1P2/4P4/5K3 w - 1 51",
        "2b6/4krA2/6B2/9/P8/8p/3R1A3/P3C1P2/4P4/5K3 w - 3 52",
        "2b6/4k1r2/6B2/9/P8/8p/3R1A3/P3C1P2/4P4/4K4 w - 0 53",
        "9/4k1r2/4b1B2/9/P8/4A3p/3R5/P3C1P2/4P4/4K4 w - 2 54",
        "9/4k1r2/6B2/9/P2A2b2/8p/3R5/P3C1P2/4P4/4K4 w - 4 55",
        "9/4k1r2/6B2/4A4/P8/8p/3Rb4/P3C1P2/4P4/4K4 w - 6 56",
        "9/5kr2/5AB2/9/P8/8p/3Rb4/P3C1P2/4P4/4K4 w - 8 57",
        "9/5kA2/6B2/9/P8/9/3Rb3p/P3C1P2/4P4/4K4 w - 1 58",
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
