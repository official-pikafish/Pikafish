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

#include "movegen.h"

#include <cassert>

#include "bitboard.h"
#include "position.h"

#if defined(USE_AVX512ICL) && defined(USE_PEXT)
    #include <array>
    #include <algorithm>
    #include <immintrin.h>
#endif

namespace Stockfish {

namespace {

#if defined(USE_AVX512ICL) && defined(USE_PEXT)

constexpr int constexpr_lsb64(uint64_t b) {
    assert(b);
    for (int i = 0; i < 64; ++i)
        if (b & (1ULL << i))
            return i;
    return 0;
}

constexpr std::array<Move, 32> make_splat_moves(Square from, Bitboard pseudoAttacks) {
    std::array<Move, 32> moves{};

    uint64_t b = uint64_t(pseudoAttacks);
    int      i = 0;
    while (b)
    {
        int to = constexpr_lsb64(b);
        moves[i++] = Move(from, Square(to));
        b &= b - 1;
    }

    b = uint64_t(pseudoAttacks >> 64);
    i = 16;
    while (b)
    {
        int to = constexpr_lsb64(b);
        moves[i++] = Move(from, Square(to + 64));
        b &= b - 1;
    }

    return moves;
}

template<PieceType Pt>
constexpr auto make_splat_table() {
    std::array<std::array<Move, 32>, SQUARE_NB> moves{};

    for (Square s = SQ_A0; s <= SQ_I9; ++s)
        moves[s] = make_splat_moves(s, PseudoAttacks[Pt][s]);

    return moves;
}

alignas(64) constexpr auto RookMoves    = make_splat_table<ROOK>();
alignas(64) constexpr auto AdvisorMoves = make_splat_table<ADVISOR>();
alignas(64) constexpr auto BishopMoves  = make_splat_table<BISHOP>();
alignas(64) constexpr auto KnightMoves  = make_splat_table<KNIGHT>();
alignas(64) constexpr auto KingMoves    = make_splat_table<KING>();

inline uint32_t splat_mask(Bitboard target, Bitboard pseudoAttacks) {
    return uint32_t(pext(target, pseudoAttacks, 16));
}

inline Move* splat_precomputed_moves(Move*                                moveList,
                                     const std::array<Move, 32>&          moves,
                                     uint32_t                             mask) {
    static_assert(sizeof(Move) == sizeof(uint16_t));

    const __m512i moveVec = _mm512_load_si512(reinterpret_cast<const __m512i*>(moves.data()));
    _mm512_mask_compressstoreu_epi16(reinterpret_cast<void*>(moveList), __mmask32(mask), moveVec);

    return moveList + popcount(Bitboard(mask));
}

template<PieceType Pt>
inline Move* splat_piece_moves(Move* moveList, Square from, Bitboard occupied, Bitboard target) {
    static_assert(Pt == ROOK || Pt == ADVISOR || Pt == BISHOP || Pt == KNIGHT || Pt == KING);

    if constexpr (Pt == ROOK)
    {
        const Magic& m    = RookMagics[from];
        uint32_t     mask = uint32_t(m.attacks[m.index(occupied)]) & splat_mask(target, m.pseudoAttacks);
        return splat_precomputed_moves(moveList, RookMoves[from], mask);
    }
    else if constexpr (Pt == BISHOP)
    {
        const Magic& m    = BishopMagics[from];
        uint32_t     mask = uint32_t(m.attacks[m.index(occupied)]) & splat_mask(target, m.pseudoAttacks);
        return splat_precomputed_moves(moveList, BishopMoves[from], mask);
    }
    else if constexpr (Pt == KNIGHT)
    {
        const Magic& m    = KnightMagics[from];
        uint32_t     mask = uint32_t(m.attacks[m.index(occupied)]) & splat_mask(target, m.pseudoAttacks);
        return splat_precomputed_moves(moveList, KnightMoves[from], mask);
    }
    else if constexpr (Pt == ADVISOR)
    {
        uint32_t mask = splat_mask(target, PseudoAttacks[ADVISOR][from]);
        return splat_precomputed_moves(moveList, AdvisorMoves[from], mask);
    }
    else
    {
        uint32_t mask = splat_mask(target, PseudoAttacks[KING][from]);
        return splat_precomputed_moves(moveList, KingMoves[from], mask);
    }
}

template<Color Us, GenType Type>
inline Move* splat_cannon_moves(const Position& pos, Move* moveList, Square from, Bitboard target) {
    Bitboard occupied = pos.pieces();

    if constexpr (Type != QUIETS)
    {
        const Magic& m = CannonMagics[from];
        Bitboard     captureTarget =
          Type == EVASIONS ? pos.pieces(~Us) & target : pos.pieces(~Us);
        uint32_t mask = uint32_t(m.attacks[m.index(occupied)]) & splat_mask(captureTarget, m.pseudoAttacks);

        moveList = splat_precomputed_moves(moveList, RookMoves[from], mask);
    }

    if constexpr (Type != CAPTURES)
    {
        const Magic& m = RookMagics[from];
        Bitboard     quietTarget = Type == EVASIONS ? ~occupied & target : ~occupied;
        uint32_t     mask = uint32_t(m.attacks[m.index(occupied)]) & splat_mask(quietTarget, m.pseudoAttacks);

        moveList = splat_precomputed_moves(moveList, RookMoves[from], mask);
    }

    return moveList;
}

#endif

template<Color Us, PieceType Pt, GenType Type>
Move* generate_moves(const Position& pos, Move* moveList, Bitboard target) {

    static_assert(Pt != KING, "Unsupported piece type in generate_moves()");

    Bitboard bb = pos.pieces(Us, Pt);

    while (bb)
    {
        Square   from = pop_lsb(bb);
        Bitboard b    = 0;
#if defined(USE_AVX512ICL) && defined(USE_PEXT)
        if constexpr (Pt == ROOK || Pt == ADVISOR || Pt == BISHOP || Pt == KNIGHT)
        {
            moveList = splat_piece_moves<Pt>(moveList, from, pos.pieces(), target);
            continue;
        }
        else if constexpr (Pt == CANNON)
        {
            moveList = splat_cannon_moves<Us, Type>(pos, moveList, from, target);
            continue;
        }
#endif
        if constexpr (Pt != CANNON)
            b = (Pt != PAWN ? attacks_bb<Pt>(from, pos.pieces()) : attacks_bb<PAWN>(from, Us))
              & target;
        else
        {
            // Generate cannon capture moves.
            if (Type != QUIETS)
                b |= attacks_bb<CANNON>(from, pos.pieces()) & pos.pieces(~Us);

            // Generate cannon quite moves.
            if (Type != CAPTURES)
                b |= attacks_bb<ROOK>(from, pos.pieces()) & ~pos.pieces();

            // Restrict to target if in evasion generation
            if (Type == EVASIONS)
                b &= target;
        }

        while (b)
            *moveList++ = Move(from, pop_lsb(b));
    }

    return moveList;
}

template<Color Us, GenType Type>
Move* generate_moves(const Position& pos, Move* moveList, Bitboard target) {
    moveList = generate_moves<Us, PAWN, Type>(pos, moveList, target);
    moveList = generate_moves<Us, BISHOP, Type>(pos, moveList, target);
    moveList = generate_moves<Us, ADVISOR, Type>(pos, moveList, target);
    moveList = generate_moves<Us, KNIGHT, Type>(pos, moveList, target);
    moveList = generate_moves<Us, CANNON, Type>(pos, moveList, target);
    moveList = generate_moves<Us, ROOK, Type>(pos, moveList, target);
    return moveList;
}

template<Color Us, GenType Type>
Move* generate_all(const Position& pos, Move* moveList) {

    const Square ksq    = pos.king_square(Us);
    Bitboard     target = Type == PSEUDO_LEGAL ? ~pos.pieces(Us)
                        : Type == CAPTURES     ? pos.pieces(~Us)
                                               : ~pos.pieces();  // QUIETS

    moveList = generate_moves<Us, Type>(pos, moveList, target);

    if (Type != EVASIONS)
    {
#if defined(USE_AVX512ICL) && defined(USE_PEXT)
        moveList = splat_piece_moves<KING>(moveList, ksq, pos.pieces(), target);
#else
        Bitboard b = attacks_bb<KING>(ksq) & target;
        while (b)
            *moveList++ = Move(ksq, pop_lsb(b));
#endif
    }

    return moveList;
}

}  // namespace


// <CAPTURES>     Generates all pseudo-legal captures
// <QUIETS>       Generates all pseudo-legal non-captures
// <PSEUDO_LEGAL> Generates all pseudo-legal captures and non-captures
//
// Returns a pointer to the end of the move list.
template<GenType Type>
Move* generate(const Position& pos, Move* moveList) {

    static_assert(Type != LEGAL && Type != EVASIONS, "Unsupported type in generate()");

    return pos.side_to_move() == WHITE ? generate_all<WHITE, Type>(pos, moveList)
                                       : generate_all<BLACK, Type>(pos, moveList);
}

// Explicit template instantiations
template Move* generate<CAPTURES>(const Position&, Move*);
template Move* generate<QUIETS>(const Position&, Move*);
template Move* generate<PSEUDO_LEGAL>(const Position&, Move*);


// generate<EVASIONS> generates all pseudo-legal check evasions when the side
// to move is in check. Returns a pointer to the end of the move list.

template<>
Move* generate<EVASIONS>(const Position& pos, Move* moveList) {

    assert(bool(pos.checkers()));

    // If there are more than one checker, use slow version
    if (more_than_one(pos.checkers()))
        return generate<PSEUDO_LEGAL>(pos, moveList);

    Color     us      = pos.side_to_move();
    Square    ksq     = pos.king_square(us);
    Square    checksq = lsb(pos.checkers());
    PieceType pt      = type_of(pos.piece_on(checksq));

    // Generate blocking evasions or captures of the checking piece
    Bitboard target = (between_bb(ksq, checksq)) & ~pos.pieces(us);
    moveList        = us == WHITE ? generate_moves<WHITE, EVASIONS>(pos, moveList, target)
                                  : generate_moves<BLACK, EVASIONS>(pos, moveList, target);

    // Generate evasions for king, capture and non capture moves
    Bitboard b = attacks_bb<KING>(ksq) & ~pos.pieces(us);
    // For all the squares attacked by slider checkers. We will remove them from
    // the king evasions in order to skip known illegal moves, which avoids any
    // useless legality checks later on.
    if (pt == ROOK || pt == CANNON)
        b &= ~line_bb(checksq, ksq) | pos.pieces(~us);
    while (b)
        *moveList++ = Move(ksq, pop_lsb(b));

    // Generate move away hurdle piece evasions for cannon
    if (pt == CANNON)
    {
        Bitboard hurdle = between_bb(ksq, checksq) & pos.pieces(us);
        if (hurdle)
        {
            Square hurdleSq = pop_lsb(hurdle);
            pt              = type_of(pos.piece_on(hurdleSq));
            if (pt == PAWN)
                b = attacks_bb<PAWN>(hurdleSq, us) & ~line_bb(checksq, hurdleSq) & ~pos.pieces(us);
            else if (pt == CANNON)
                b = (attacks_bb<ROOK>(hurdleSq, pos.pieces()) & ~line_bb(checksq, hurdleSq)
                     & ~pos.pieces())
                  | (attacks_bb<CANNON>(hurdleSq, pos.pieces()) & pos.pieces(~us));
            else
                b = attacks_bb(pt, hurdleSq, pos.pieces()) & ~line_bb(checksq, hurdleSq)
                  & ~pos.pieces(us);
            while (b)
                *moveList++ = Move(hurdleSq, pop_lsb(b));
        }
    }

    return moveList;
}


// generate<LEGAL> generates all the legal moves in the given position

template<>
Move* generate<LEGAL>(const Position& pos, Move* moveList) {

    Move* cur = moveList;

    moveList =
      pos.checkers() ? generate<EVASIONS>(pos, moveList) : generate<PSEUDO_LEGAL>(pos, moveList);

    while (cur != moveList)
        if (!pos.legal(*cur))
            *cur = *(--moveList);
        else
            ++cur;

    return moveList;
}

}  // namespace Stockfish
