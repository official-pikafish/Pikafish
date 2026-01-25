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

#include "position.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string_view>
#include <utility>

#include "bitboard.h"
#include "history.h"
#include "misc.h"
#include "movegen.h"
#include "nnue/nnue_architecture.h"
#include "tt.h"
#include "uci.h"

using std::string;

namespace Stockfish {

namespace Zobrist {

Key psq[PIECE_NB][SQUARE_NB];
Key side, noPawns;
// Absorption Xiangqi: Zobrist keys for absorbed abilities
// absorption[square][piece_type] = key for having absorbed that piece type at that square
Key absorption[SQUARE_NB][PIECE_TYPE_NB];
}

namespace {

constexpr std::string_view PieceToChar(" RACPNBK racpnbk");

static constexpr Piece Pieces[] = {W_ROOK, W_ADVISOR, W_CANNON, W_PAWN, W_KNIGHT, W_BISHOP, W_KING,
                                   B_ROOK, B_ADVISOR, B_CANNON, B_PAWN, B_KNIGHT, B_BISHOP, B_KING};
}  // namespace

// Returns an ASCII representation of the position
std::ostream& operator<<(std::ostream& os, const Position& pos) {

    os << "\n +---+---+---+---+---+---+---+---+---+\n";

    for (Rank r = RANK_9;; --r)
    {
        for (File f = FILE_A; f <= FILE_I; ++f)
            os << " | " << PieceToChar[pos.piece_on(make_square(f, r))];

        os << " | " << int(r) << "\n +---+---+---+---+---+---+---+---+---+\n";

        if (r == RANK_0)
            break;
    }

    os << "   a   b   c   d   e   f   g   h   i\n"
       << "\nFen: " << pos.fen() << "\nKey: " << std::hex << std::uppercase << std::setfill('0')
       << std::setw(16) << pos.key() << std::setfill(' ') << std::dec << "\nCheckers: ";

    for (Bitboard b = pos.checkers(); b;)
        os << UCIEngine::square(pop_lsb(b)) << " ";

    return os;
}


// Initializes at startup the various arrays used to compute hash keys
void Position::init() {

    PRNG rng(1070372);

    for (Piece pc : Pieces)
        for (Square s = SQ_A0; s <= SQ_I9; ++s)
            Zobrist::psq[pc][s] = rng.rand<Key>();

    Zobrist::side    = rng.rand<Key>();
    Zobrist::noPawns = rng.rand<Key>();

    // Absorption Xiangqi: initialize Zobrist keys for absorbed abilities
    for (Square s = SQ_A0; s <= SQ_I9; ++s)
        for (PieceType pt = PAWN; pt <= KING; ++pt)
            Zobrist::absorption[s][pt] = rng.rand<Key>();
}


// Initializes the position object with the given FEN string.
// This function is not very robust - make sure that input FENs are correct,
// this is assumed to be the responsibility of the GUI.
Position& Position::set(const string& fenStr, StateInfo* si) {
    /*
   A FEN string defines a particular position using only the ASCII character set.

   A FEN string contains six fields separated by a space. The fields are:

   1) Piece placement (from white's perspective). Each rank is described, starting
      with rank 9 and ending with rank 0. Within each rank, the contents of each
      square are described from file A through file I. Following the Standard
      Algebraic Notation (SAN), each piece is identified by a single letter taken
      from the standard English names. White pieces are designated using upper-case
      letters ("RACPNBK") whilst Black uses lowercase ("racpnbk"). Blank squares are
      noted using digits 1 through 9 (the number of blank squares), and "/"
      separates ranks.

   2) Active color. "w" means white moves next, "b" means black.

   3) Halfmove clock. This is the number of halfmoves since the last pawn advance
      or capture. This is used to determine if a draw can be claimed under the
      fifty-move rule.

   4) Fullmove number. The number of the full move. It starts at 1, and is
      incremented after Black's move.
*/

    unsigned char      token;
    size_t             idx;
    Square             sq = SQ_A9;
    std::istringstream ss(fenStr);

    std::memset(reinterpret_cast<char*>(this), 0, sizeof(Position));

    midEncoding[WHITE] = midEncoding[BLACK] = Eval::NNUE::Features::HalfKAv2_hm::BalanceEncoding;

    std::memset(si, 0, sizeof(StateInfo));
    st = si;

    ss >> std::noskipws;

    // 1. Piece placement
    // Extended format for Absorption Xiangqi: piece(abilities)
    // e.g., R(cn) for rook with cannon and knight abilities
    while ((ss >> token) && !isspace(token))
    {
        if (isdigit(token))
            sq += (token - '0') * EAST;  // Advance the given number of files

        else if (token == '/')
            sq += 2 * SOUTH;

        else if ((idx = PieceToChar.find(token)) != string::npos)
        {
            put_piece(Piece(idx), sq);

            // Check for absorbed abilities in extended format: piece(abilities)
            char next = ss.peek();
            if (next == '(')
            {
                ss >> token;  // consume '('
                AbilityMask abilities = 0;
                while ((ss >> token) && token != ')')
                {
                    // Parse ability characters (lowercase)
                    char lc = tolower(token);
                    if (lc == 'r') abilities |= (1 << ROOK);
                    else if (lc == 'a') abilities |= (1 << ADVISOR);
                    else if (lc == 'c') abilities |= (1 << CANNON);
                    else if (lc == 'p') abilities |= (1 << PAWN);
                    else if (lc == 'n') abilities |= (1 << KNIGHT);
                    else if (lc == 'b') abilities |= (1 << BISHOP);
                }
                absorbed[sq] = abilities;
            }

            ++sq;
        }
    }

    // 2. Active color
    ss >> token;
    sideToMove = (token == 'w' ? WHITE : BLACK);
    ss >> token;

    while ((ss >> token) && !isspace(token))
        ;

    while ((ss >> token) && !isspace(token))
        ;

    // 3-4. Halfmove clock and fullmove number
    ss >> std::skipws >> st->rule60 >> gamePly;

    // Convert from fullmove starting from 1 to gamePly starting from 0,
    // handle also common incorrect FEN with fullmove = 0.
    gamePly = std::max(2 * (gamePly - 1), 0) + (sideToMove == BLACK);

    set_state();

    assert(pos_is_ok());

    return *this;
}


// Sets king attacks to detect if a move gives check
void Position::set_check_info() const {

    update_blockers<WHITE>();
    update_blockers<BLACK>();

    Square ksq = king_square(~sideToMove);

    // We have to take special cares about the hollow cannons and checks
    // Absorption Xiangqi: always do full check since pieces can have absorbed abilities
    // that make the standard optimizations unsafe (e.g., a pawn with rook ability can pin)
    st->needFullCheck = true;
    (void)(checkers() || (attacks_bb<ROOK>(king_square(sideToMove)) & pieces(~sideToMove, CANNON)));

    st->checkSquares[PAWN]   = attacks_bb<PAWN_TO>(ksq, sideToMove);
    st->checkSquares[KNIGHT] = attacks_bb<KNIGHT_TO>(ksq, pieces());
    st->checkSquares[CANNON] = attacks_bb<CANNON>(ksq, pieces());
    st->checkSquares[ROOK]   = attacks_bb<ROOK>(ksq, pieces());
    st->checkSquares[KING] = st->checkSquares[ADVISOR] = st->checkSquares[BISHOP] = 0;

    Bitboard hollowCannons = st->checkSquares[ROOK] & pieces(sideToMove, CANNON);
    if (hollowCannons)
    {
        Bitboard hollowCannonDiscover = Bitboard(0);
        while (hollowCannons)
            hollowCannonDiscover |= between_bb(pop_lsb(hollowCannons), ksq);
        for (PieceType pt = ROOK; pt < KING; ++pt)
            st->checkSquares[pt] |= hollowCannonDiscover;
    }
}


// Computes the hash keys of the position, and other
// data that once computed is updated incrementally as moves are made.
// The function is only used when a new position is set up
void Position::set_state() const {

    st->key               = 0;
    st->minorPieceKey     = 0;
    st->nonPawnKey[WHITE] = st->nonPawnKey[BLACK] = 0;
    st->pawnKey                                   = Zobrist::noPawns;
    st->majorMaterial[WHITE] = st->majorMaterial[BLACK] = VALUE_ZERO;
    st->checkersBB = checkers_to(~sideToMove, king_square(sideToMove));
    st->move       = Move::none();

    set_check_info();

    for (Bitboard b = pieces(); b;)
    {
        Square    s  = pop_lsb(b);
        Piece     pc = piece_on(s);
        PieceType pt = type_of(pc);
        st->key ^= Zobrist::psq[pc][s];

        if (pt == PAWN)
            st->pawnKey ^= Zobrist::psq[pc][s];

        else
        {
            st->nonPawnKey[color_of(pc)] ^= Zobrist::psq[pc][s];

            if (pt != KING && (pt & 1))
            {
                // Absorption Xiangqi: use effective piece value including absorbed abilities
                st->majorMaterial[color_of(pc)] += absorbed_piece_value(s);
                if (pt != ROOK)
                    st->minorPieceKey ^= Zobrist::psq[pc][s];
            }
        }

        // Absorption Xiangqi: include absorbed abilities in hash
        for (PieceType apt = ROOK; apt < KING; ++apt)
            if (absorbed[s] & (1 << apt))
                st->key ^= Zobrist::absorption[s][apt];
    }

    if (sideToMove == BLACK)
        st->key ^= Zobrist::side;
}


// Helper to convert absorbed abilities to a string
static string absorbed_to_string(AbilityMask abilities) {
    string result;
    // Order: r, a, c, p, n, b (matching piece type order)
    if (abilities & (1 << ROOK))    result += 'r';
    if (abilities & (1 << ADVISOR)) result += 'a';
    if (abilities & (1 << CANNON))  result += 'c';
    if (abilities & (1 << PAWN))    result += 'p';
    if (abilities & (1 << KNIGHT))  result += 'n';
    if (abilities & (1 << BISHOP))  result += 'b';
    return result;
}

// Absorption Xiangqi: piece value lookup by piece type
static constexpr Value AbsorptionPieceValue[PIECE_TYPE_NB] = {
    VALUE_ZERO,   // NO_PIECE_TYPE
    RookValue,    // ROOK
    AdvisorValue, // ADVISOR
    CannonValue,  // CANNON
    PawnValue,    // PAWN
    KnightValue,  // KNIGHT
    BishopValue,  // BISHOP
    VALUE_ZERO    // KING
};

// Absorption Xiangqi: synergy multiplier for combined pieces (23/20 = 1.15)
static constexpr int ABSORPTION_SYNERGY_NUMERATOR = 23;
static constexpr int ABSORPTION_SYNERGY_DENOMINATOR = 20;

// Absorption Xiangqi: calculate effective piece value including absorbed abilities
// A piece with absorbed abilities is worth the sum of its base value plus absorbed values,
// multiplied by a synergy factor (since combined pieces are more valuable than separate ones).
// Exception: Rook + Pawn ability doesn't add Pawn value (Rook already moves orthogonally).
Value Position::absorbed_piece_value(Square s) const {
    Piece pc = piece_on(s);
    if (pc == NO_PIECE)
        return VALUE_ZERO;

    PieceType basePt = type_of(pc);
    Value baseValue = PieceValue[pc];

    AbilityMask abilities = absorbed[s];
    if (!abilities)
        return baseValue;

    // Sum up absorbed ability values
    Value absorbedValue = VALUE_ZERO;
    for (PieceType apt = ROOK; apt < KING; ++apt) {
        if (abilities & (1 << apt)) {
            // Exception: Rook + Pawn ability doesn't add value
            // (Rook already has orthogonal movement, pawn ability is redundant)
            if (basePt == ROOK && apt == PAWN)
                continue;
            // Also skip if absorbed ability matches base type (shouldn't happen, but safety)
            if (apt == basePt)
                continue;
            absorbedValue += AbsorptionPieceValue[apt];
        }
    }

    // Apply synergy multiplier: combined pieces are worth more than sum of parts
    Value totalValue = baseValue + absorbedValue;
    if (absorbedValue > VALUE_ZERO)
        totalValue = totalValue * ABSORPTION_SYNERGY_NUMERATOR / ABSORPTION_SYNERGY_DENOMINATOR;

    return totalValue;
}

// Returns a FEN representation of the position.
// Extended format for Absorption Xiangqi: pieces with absorbed abilities
// are written as piece(abilities), e.g., R(cn) for a rook with cannon and knight.
string Position::fen() const {

    int                emptyCnt;
    std::ostringstream ss;

    for (Rank r = RANK_9;; --r)
    {
        for (File f = FILE_A; f <= FILE_I; ++f)
        {
            for (emptyCnt = 0; f <= FILE_I && empty(make_square(f, r)); ++f)
                ++emptyCnt;

            if (emptyCnt)
                ss << emptyCnt;

            if (f <= FILE_I)
            {
                Square sq = make_square(f, r);
                ss << PieceToChar[piece_on(sq)];

                // Absorption Xiangqi: output absorbed abilities if any
                AbilityMask abilities = absorbed[sq];
                if (abilities)
                    ss << '(' << absorbed_to_string(abilities) << ')';
            }
        }

        if (r == RANK_0)
            break;
        ss << '/';
    }

    ss << (sideToMove == WHITE ? " w " : " b ");

    ss << '-';

    ss << " - " << st->rule60 << " " << 1 + (gamePly - (sideToMove == BLACK)) / 2;

    return ss.str();
}


// Calculates st->blockersForKing[c] and st->pinners[~c],
// which store respectively the pieces preventing king of color c from being in check
// and the slider pieces of color ~c pinning pieces of color c to the king.
template<Color c>
void Position::update_blockers() const {

    Square ksq             = king_square(c);
    st->blockersForKing[c] = 0;
    st->pinners[~c]        = 0;

    // Snipers are pieces that attack 's' when a piece and other pieces are removed
    Bitboard snipers = ((attacks_bb<ROOK>(ksq) & (pieces(ROOK) | pieces(CANNON) | pieces(KING)))
                        | (attacks_bb<KNIGHT>(ksq) & pieces(KNIGHT)))
                     & pieces(~c);
    Bitboard occupancy = pieces() ^ (snipers & ~pieces(CANNON));

    while (snipers)
    {
        Square   sniperSq = pop_lsb(snipers);
        bool     isCannon = type_of(piece_on(sniperSq)) == CANNON;
        Bitboard b = between_bb(ksq, sniperSq) & (isCannon ? pieces() ^ sniperSq : occupancy);

        if (b && ((!isCannon && !more_than_one(b)) || (isCannon && popcount(b) == 2)))
        {
            st->blockersForKing[c] |= b;
            if (b & pieces(c))
                st->pinners[~c] |= sniperSq;
        }
    }
}


// Absorption Xiangqi: Check if a piece at 'from' can attack square 's'
// considering its base type and all absorbed abilities
bool can_attack_with_abilities(Square from, Square s, Bitboard occupied, PieceType basePt, AbilityMask abilities, Color pieceColor) {
    // Check base piece type
    switch (basePt) {
    case PAWN:
        if (attacks_bb<PAWN>(from, pieceColor) & s) return true;
        break;
    case ROOK:
        if (attacks_bb<ROOK>(from, occupied) & s) return true;
        break;
    case CANNON:
        if (attacks_bb<CANNON>(from, occupied) & s) return true;
        break;
    case KNIGHT:
        if (attacks_bb<KNIGHT>(from, occupied) & s) return true;
        break;
    case BISHOP:
        if (attacks_bb<BISHOP>(from, occupied) & s) return true;
        break;
    case ADVISOR:
        if (abilities)  // Has absorbed abilities, can attack outside palace
            { if (unconstrained_attacks_bb<ADVISOR>(from) & s) return true; }
        else
            { if (attacks_bb<ADVISOR>(from) & s) return true; }
        break;
    case KING:
        if (attacks_bb<KING>(from) & s) return true;
        break;
    default:
        break;
    }

    // Check absorbed abilities
    for (PieceType pt = ROOK; pt < KING; ++pt) {
        if (abilities & (1 << pt)) {
            switch (pt) {
            case PAWN:
                if (attacks_bb<PAWN>(from, pieceColor) & s) return true;
                break;
            case ROOK:
                if (attacks_bb<ROOK>(from, occupied) & s) return true;
                break;
            case CANNON:
                if (attacks_bb<CANNON>(from, occupied) & s) return true;
                break;
            case KNIGHT:
                if (attacks_bb<KNIGHT>(from, occupied) & s) return true;
                break;
            case BISHOP:
                if (attacks_bb<BISHOP>(from, occupied) & s) return true;
                break;
            case ADVISOR:
                if (unconstrained_attacks_bb<ADVISOR>(from) & s) return true;
                break;
            default:
                break;
            }
        }
    }
    return false;
}

// Computes a bitboard of all pieces which attack a given square.
// Slider attacks use the occupied bitboard to indicate occupancy.
// Absorption Xiangqi: considers absorbed abilities
Bitboard Position::attackers_to(Square s, Bitboard occupied) const {
    Bitboard attackers = 0;

    // Check all pieces on the board
    Bitboard allPieces = pieces();
    while (allPieces) {
        Square from = pop_lsb(allPieces);
        Piece pc = piece_on(from);
        if (pc == NO_PIECE) continue;

        if (can_attack_with_abilities(from, s, occupied, type_of(pc), absorbed[from], color_of(pc)))
            attackers |= from;
    }

    return attackers;
}


// Computes a bitboard of all pieces of a given color
// which gives check to a given square. Slider attacks use the occupied bitboard
// to indicate occupancy.
// Absorption Xiangqi: considers absorbed abilities
Bitboard Position::checkers_to(Color c, Square s, Bitboard occupied) const {
    Bitboard checkers = 0;

    // Check all pieces of color c
    Bitboard colorPieces = pieces(c);
    while (colorPieces) {
        Square from = pop_lsb(colorPieces);
        Piece pc = piece_on(from);
        if (pc == NO_PIECE) continue;

        if (can_attack_with_abilities(from, s, occupied, type_of(pc), absorbed[from], c))
            checkers |= from;
    }

    return checkers;
}


// Tests whether a pseudo-legal move is legal
bool Position::legal(Move m) const {

    assert(m.is_ok());

    Color    us       = sideToMove;
    Square   from     = m.from_sq();
    Square   to       = m.to_sq();
    Bitboard occupied = (pieces() ^ from) | to;

    assert(color_of(moved_piece(m)) == us);
    assert(piece_on(king_square(us)) == make_piece(us, KING));

    // If the moving piece is a king, check whether the destination square is
    // attacked by the opponent.
    if (type_of(piece_on(from)) == KING)
        return !(checkers_to(~us, to, occupied));

    // If we don't need full check. A non-king move is always legal when either:
    // 1. Not moving a pinned piece.
    // 2. Moving a pinned non-cannon piece and aligned with king.
    // 3. Moving a pinned cannon and aligned with king but it's not a capture move.
    if (!st->needFullCheck
        && (!(blockers_for_king(us) & from)
            || (((type_of(piece_on(from)) != CANNON) || !capture(m))
                && aligned(from, to, king_square(us)))))
        return true;

    // A non-king move is legal if the king is not under attack after the move.
    return !(checkers_to(~us, king_square(us), occupied) & ~square_bb(to));
}


// Takes a random move and tests whether the move is
// pseudo-legal. It is used to validate moves from TT that can be corrupted
// due to SMP concurrent access or hash position key aliasing.
bool Position::pseudo_legal(const Move m) const {

    Color  us   = sideToMove;
    Square from = m.from_sq();
    Square to   = m.to_sq();
    Piece  pc   = moved_piece(m);

    // If the 'from' square is not occupied by a piece belonging to the side to
    // move, the move is obviously not legal.
    if (pc == NO_PIECE || color_of(pc) != us)
        return false;

    // The destination square cannot be occupied by a friendly piece
    if (pieces(us) & to)
        return false;

    PieceType pt = type_of(pc);
    Bitboard occupied = pieces();
    bool isCapture = capture(m);
    AbilityMask abilities = absorbed[from];
    bool hasAnyAbsorbed = (abilities != 0);

    // Helper lambda to check if a move is valid for a given piece type
    auto is_valid_for_type = [&](PieceType checkPt) -> bool {
        if (checkPt == PAWN)
            return bool(attacks_bb<PAWN>(from, us) & to);
        else if (checkPt == CANNON) {
            // Cannon has different moves for captures vs quiets
            if (isCapture)
                return bool(attacks_bb<CANNON>(from, occupied) & to);
            else
                return bool(attacks_bb<ROOK>(from, occupied) & to);
        }
        else if (checkPt == ADVISOR) {
            // Advisor with absorbed abilities can move diagonally anywhere
            if (hasAnyAbsorbed)
                return bool(unconstrained_attacks_bb<ADVISOR>(from) & to);
            else
                return bool(attacks_bb<ADVISOR>(from) & to);
        }
        else if (checkPt == BISHOP) {
            // Bishop moves are already handled by attacks_bb
            return bool(attacks_bb<BISHOP>(from, occupied) & to);
        }
        else
            return bool(attacks_bb(checkPt, from, occupied) & to);
    };

    // Check if move is valid for base piece type
    if (is_valid_for_type(pt))
        return true;

    // Absorption Xiangqi: check if move is valid for any absorbed ability
    for (PieceType apt = ROOK; apt < KING; ++apt) {
        if ((abilities & (1 << apt)) && is_valid_for_type(apt))
            return true;
    }

    return false;
}


// Tests whether a pseudo-legal move gives a check
// Absorption Xiangqi: considers both base piece type and absorbed abilities,
// including abilities that will be gained from a capture
bool Position::gives_check(Move m) const {

    assert(m.is_ok());
    assert(color_of(moved_piece(m)) == sideToMove);

    Square from = m.from_sq();
    Square to   = m.to_sq();
    Square ksq  = king_square(~sideToMove);

    PieceType pt = type_of(moved_piece(m));

    // Helper lambda to check if a piece type gives check from 'to' square
    auto check_for_type = [&](PieceType checkPt) {
        if (checkPt == CANNON && (check_squares(ROOK) & from) && aligned(from, to, ksq))
        {
            if (capture(m) && (ray_pass_bb(ksq, from) & to))
                return true;
        }
        else if (check_squares(checkPt) & to)
            return true;
        return false;
    };

    // Is there a direct check from base piece type?
    if (check_for_type(pt))
        return true;

    // Absorption Xiangqi: check if any currently absorbed ability gives check
    AbilityMask abilities = absorbed[from];
    for (PieceType apt = ROOK; apt < KING; ++apt) {
        if ((abilities & (1 << apt)) && check_for_type(apt))
            return true;
    }

    // Absorption Xiangqi: if this is a capture, check if the newly absorbed ability gives check
    if (capture(m)) {
        PieceType capturedType = type_of(piece_on(to));
        // Will absorb if: different type, not KING, not already absorbed
        if (capturedType != pt && capturedType != KING && !(abilities & (1 << capturedType))) {
            if (check_for_type(capturedType))
                return true;
        }
    }

    // Is there a discovered check?
    if ((blockers_for_king(~sideToMove) & from) && (!aligned(from, to, ksq) || capture(m)))
        return true;

    return false;
}


// Makes a move, and saves all information necessary
// to a StateInfo object. The move is assumed to be legal. Pseudo-legal
// moves should be filtered out before this function is called.
// If a pointer to the TT table is passed, the entry for the new position
// will be prefetched, and likewise for shared history.
void Position::do_move(Move                      m,
                       StateInfo&                newSt,
                       bool                      givesCheck,
                       DirtyPiece&               dp,
                       DirtyThreats&             dts,
                       const TranspositionTable* tt      = nullptr,
                       const SharedHistories*    history = nullptr) {

    using namespace Eval::NNUE;

    assert(m.is_ok());
    assert(&newSt != st);

    // Update the bloom filter
    ++filter[st->key];

    Key k = st->key ^ Zobrist::side;

    // Copy some fields of the old state to our new StateInfo object except the
    // ones which are going to be recalculated from scratch anyway and then switch
    // our state pointer to point to the new (ready to be updated) state.
    std::memcpy(&newSt, st, offsetof(StateInfo, key));
    newSt.previous = st;
    st             = &newSt;
    st->move       = m;

    // Increment ply counters. Clamp to 10 checks for each side in rule 60
    // In particular, rule60 will be reset to zero later on in case of a capture.
    ++gamePly;
    if (!givesCheck || ++st->check10[sideToMove] <= 10)
    {
        if (st->check10[~sideToMove] > 10 && st->previous->checkersBB)
            ++st->check10[~sideToMove];
        else
            ++st->rule60;
    }
    ++st->pliesFromNull;

    Color  us       = sideToMove;
    Color  them     = ~us;
    Square from     = m.from_sq();
    Square to       = m.to_sq();
    Piece  pc       = piece_on(from);
    Piece  captured = piece_on(to);

    dp.pc   = pc;
    dp.from = from;
    dp.to   = to;

    assert(color_of(pc) == us);
    assert(captured == NO_PIECE || color_of(captured) == them);
    assert(type_of(captured) != KING);

    bool mirror_before[2] = {
      PSQFeatureSet::KingBuckets[king_square(us)][king_square(them)]
                                [PSQFeatureSet::requires_mid_mirror(*this, us)]
                                  .second,
      PSQFeatureSet::KingBuckets[king_square(them)][king_square(us)]
                                [PSQFeatureSet::requires_mid_mirror(*this, them)]
                                  .second};
    dp.requires_refresh[them] = false;
    dp.requires_refresh[us]   = pc == make_piece(us, KING);

    if (captured)
    {
        Square capsq = to;

        // If the captured piece is a pawn, update pawn hash key, otherwise
        // update major material.
        if (type_of(captured) == PAWN)
            st->pawnKey ^= Zobrist::psq[captured][capsq];

        else
        {
            st->nonPawnKey[them] ^= Zobrist::psq[captured][capsq];

            if (type_of(captured) & 1)
            {
                // Absorption Xiangqi: subtract effective value including absorbed abilities
                st->majorMaterial[them] -= absorbed_piece_value(capsq);
                if (type_of(captured) != ROOK)
                    st->minorPieceKey ^= Zobrist::psq[captured][capsq];
            }
        }

        dp.remove_pc = captured;
        dp.remove_sq = capsq;

        // Update hash key
        k ^= Zobrist::psq[captured][capsq];

        // Reset rule 60 counter
        st->check10[WHITE] = st->check10[BLACK] = st->rule60 = 0;

        // Absorption Xiangqi: absorb the captured piece's base type (not KING)
        PieceType capturedType = type_of(captured);
        PieceType movingType = type_of(pc);
        st->absorptionSquare = from;
        st->previousAbsorbed = absorbed[from];
        st->capturedAbsorbed = absorbed[capsq];  // Save captured piece's absorbed abilities

        // Absorption Xiangqi: save moving piece's old effective value for material update
        Value oldMovingValue = absorbed_piece_value(from);

        // XOR out old absorption state at from (will XOR in at 'to' later)
        for (PieceType pt = ROOK; pt < KING; ++pt)
            if (absorbed[from] & (1 << pt))
                k ^= Zobrist::absorption[from][pt];

        // XOR out captured piece's absorption state
        for (PieceType pt = ROOK; pt < KING; ++pt)
            if (absorbed[capsq] & (1 << pt))
                k ^= Zobrist::absorption[capsq][pt];

        // Only absorb if: different piece type, not KING, and not already absorbed
        if (capturedType != movingType && capturedType != KING && !(absorbed[from] & (1 << capturedType)))
        {
            absorbed[from] |= (1 << capturedType);

            // Absorption Xiangqi: update moving piece's material value due to absorption
            Value newMovingValue = absorbed_piece_value(from);
            st->majorMaterial[us] += (newMovingValue - oldMovingValue);
        }

        // XOR in new absorption state at 'to' (after absorption)
        for (PieceType pt = ROOK; pt < KING; ++pt)
            if (absorbed[from] & (1 << pt))
                k ^= Zobrist::absorption[to][pt];

        // Clear captured piece's absorbed abilities
        absorbed[capsq] = 0;
    }
    else
    {
        dp.remove_sq = SQ_NONE;
        st->absorptionSquare = SQ_NONE;
        st->capturedAbsorbed = 0;

        // Absorption Xiangqi: update hash for absorbed abilities moving from->to
        for (PieceType pt = ROOK; pt < KING; ++pt)
            if (absorbed[from] & (1 << pt))
                k ^= Zobrist::absorption[from][pt] ^ Zobrist::absorption[to][pt];
    }

    // Update hash key
    k ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
    if (tt)
        prefetch(tt->first_entry(adjust_key60(k)));

    // If the moving piece is a pawn, update pawn hash key.
    if (type_of(pc) == PAWN)
        st->pawnKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
    else
    {
        st->nonPawnKey[us] ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];

        if (type_of(pc) == KNIGHT || type_of(pc) == CANNON)
            st->minorPieceKey ^= Zobrist::psq[pc][from] ^ Zobrist::psq[pc][to];
    }

    if (captured)
    {
        auto attack_bucket_before = PSQFeatureSet::make_attack_bucket(*this, them);

        remove_piece(from, &dts);
        swap_piece(to, pc, &dts);

        auto attack_bucket_after = PSQFeatureSet::make_attack_bucket(*this, them);

        dp.requires_refresh[them] |= (attack_bucket_before != attack_bucket_after);

        // Absorption Xiangqi: move absorbed abilities from 'from' to 'to'
        absorbed[to] = absorbed[from];
        absorbed[from] = 0;
    }
    else
    {
        move_piece(from, to, &dts);

        // Absorption Xiangqi: move absorbed abilities from 'from' to 'to'
        absorbed[to] = absorbed[from];
        absorbed[from] = 0;
    }

    bool mirror_after[2] = {
      PSQFeatureSet::KingBuckets[king_square(us)][king_square(them)]
                                [PSQFeatureSet::requires_mid_mirror(*this, us)]
                                  .second,
      PSQFeatureSet::KingBuckets[king_square(them)][king_square(us)]
                                [PSQFeatureSet::requires_mid_mirror(*this, them)]
                                  .second};
    dp.requires_refresh[us] |= dts.requires_refresh[us]     = (mirror_before[0] != mirror_after[0]);
    dp.requires_refresh[them] |= dts.requires_refresh[them] = (mirror_before[1] != mirror_after[1]);

    if (history)
    {
        prefetch(&history->pawn_entry(*this)[pc][to]);
        prefetch(&history->pawn_correction_entry(*this));
        prefetch(&history->minor_piece_correction_entry(*this));
        prefetch(&history->nonpawn_correction_entry<WHITE>(*this));
        prefetch(&history->nonpawn_correction_entry<BLACK>(*this));
    }

    // Set capture piece
    st->capturedPiece = captured;

    // Calculate checkers bitboard
    // Absorption Xiangqi: always compute checkers since gives_check can't reliably
    // predict checks when pieces gain new abilities from captures
    st->checkersBB = checkers_to(us, king_square(them));
    (void)givesCheck;  // Suppress unused warning

    sideToMove = ~sideToMove;

    // Update king attacks used for fast check detection
    set_check_info();

    // Update the key with the final value
    st->key = k;

    assert(pos_is_ok());

    assert(dp.pc != NO_PIECE);
    assert(!bool(captured) ^ (dp.remove_sq != SQ_NONE));
    assert(dp.from != SQ_NONE && dp.to != SQ_NONE);
}


// Unmakes a move. When it returns, the position should
// be restored to exactly the same state as before the move was made.
void Position::undo_move(Move m) {

    assert(m.is_ok());

    sideToMove = ~sideToMove;

    Square from = m.from_sq();
    Square to   = m.to_sq();

    assert(empty(from));
    assert(type_of(st->capturedPiece) != KING);

    // Absorption Xiangqi: restore absorbed abilities before moving piece back
    absorbed[from] = (st->absorptionSquare != SQ_NONE) ? st->previousAbsorbed : absorbed[to];
    absorbed[to] = 0;

    move_piece(to, from);  // Put the piece back at the source square

    if (st->capturedPiece)
    {
        Square capsq = to;

        put_piece(st->capturedPiece, capsq);  // Restore the captured piece

        // Absorption Xiangqi: restore the captured piece's absorbed abilities
        absorbed[capsq] = st->capturedAbsorbed;
    }

    // Finally point our state pointer back to the previous state
    st = st->previous;
    --gamePly;

    // Update the bloom filter
    --filter[st->key];

    assert(pos_is_ok());
}


template<bool PutPiece>
inline void add_dirty_threat(
  DirtyThreats* const dts, Piece pc, Piece threatened, Square s, Square threatenedSq) {
    dts->list.push_back({pc, threatened, s, threatenedSq, PutPiece});
}


template<bool PutPiece, bool ComputeRay>
void Position::update_piece_threats(Piece pc, Square s, DirtyThreats* const dts) {
    Bitboard occupied = pieces();

    const Bitboard rAttacks = attacks_bb<ROOK>(s, occupied);
    const Bitboard cAttacks = attacks_bb<CANNON>(s, occupied);

    // Outgoing threats
    Bitboard threatened;

    switch (type_of(pc))
    {
    case PAWN :
        threatened = attacks_bb<PAWN>(s, color_of(pc));
        break;
    case ROOK :
        threatened = rAttacks;
        break;
    case CANNON :
        threatened = cAttacks;
        break;

    default :
        threatened = attacks_bb(type_of(pc), s, occupied);
    }

    threatened &= occupied;

    while (threatened)
    {
        Square threatenedSq = pop_lsb(threatened);
        Piece  threatenedPc = piece_on(threatenedSq);

        assert(threatenedSq != s);
        assert(threatenedPc);

        add_dirty_threat<PutPiece>(dts, pc, threatenedPc, s, threatenedSq);
    }

    // Incoming threats
    Bitboard incoming_threats = (attacks_bb<PAWN_TO>(s, WHITE) & pieces(WHITE, PAWN))
                              | (attacks_bb<PAWN_TO>(s, BLACK) & pieces(BLACK, PAWN))
                              | (attacks_bb<KNIGHT_TO>(s, occupied) & pieces(KNIGHT))
                              | (attacks_bb<BISHOP>(s, occupied) & pieces(BISHOP))
                              | (attacks_bb<ADVISOR>(s) & pieces(ADVISOR))
                              | (attacks_bb<KING>(s) & pieces(KING));

    // Discovered threats
    if constexpr (ComputeRay)
    {
        // Rooks threat pieces on the other side
        Bitboard sliders = rAttacks & pieces(ROOK);
        while (sliders)
        {
            Square sliderSq = pop_lsb(sliders);
            Piece  slider   = piece_on(sliderSq);

            const Bitboard discovered = ray_pass_bb(sliderSq, s) & rAttacks & occupied;

            assert(!more_than_one(discovered));
            if (discovered)
            {
                const Square threatenedSq = lsb(discovered);
                const Piece  threatenedPc = piece_on(threatenedSq);
                add_dirty_threat<!PutPiece>(dts, slider, threatenedPc, sliderSq, threatenedSq);
            }

            add_dirty_threat<PutPiece>(dts, slider, pc, sliderSq, s);
        }
        // Cannons threat pieces on the other side
        sliders = cAttacks & pieces(CANNON);
        while (sliders)
        {
            Square sliderSq = pop_lsb(sliders);
            Piece  slider   = piece_on(sliderSq);

            // Jumping over the first piece before 's'
            const Bitboard discovered = ray_pass_bb(sliderSq, s) & rAttacks & occupied;

            assert(!more_than_one(discovered));
            if (discovered)
            {
                const Square threatenedSq = lsb(discovered);
                const Piece  threatenedPc = piece_on(threatenedSq);
                add_dirty_threat<!PutPiece>(dts, slider, threatenedPc, sliderSq, threatenedSq);
            }

            add_dirty_threat<PutPiece>(dts, slider, pc, sliderSq, s);
        }
        sliders = rAttacks & pieces(CANNON);
        while (sliders)
        {
            Square sliderSq = pop_lsb(sliders);
            Piece  slider   = piece_on(sliderSq);

            // Jumping over 's'
            Bitboard discovered = ray_pass_bb(sliderSq, s) & rAttacks & occupied;

            assert(!more_than_one(discovered));
            if (discovered)
            {
                const Square threatenedSq = lsb(discovered);
                const Piece  threatenedPc = piece_on(threatenedSq);
                add_dirty_threat<PutPiece>(dts, slider, threatenedPc, sliderSq, threatenedSq);
            }

            // Jumping over the first piece after 's'
            discovered = ray_pass_bb(sliderSq, s) & cAttacks & occupied;

            assert(!more_than_one(discovered));
            if (discovered)
            {
                const Square threatenedSq = lsb(discovered);
                const Piece  threatenedPc = piece_on(threatenedSq);
                add_dirty_threat<!PutPiece>(dts, slider, threatenedPc, sliderSq, threatenedSq);
            }
        }

        // Knights with 's' in between threat pieces on the other side
        // Bishops with 's' in between threat pieces on the other side
        Bitboard leapers = (unconstrained_attacks_bb<KING>(s) & pieces(KNIGHT))
                         | (unconstrained_attacks_bb<ADVISOR>(s) & pieces(BISHOP));
        while (leapers)
        {
            Square leaperSq = pop_lsb(leapers);
            Piece  leaper   = piece_on(leaperSq);

            Bitboard discovered = leaper_pass_bb(leaperSq, s) & occupied;

            assert(type_of(leaper) == KNIGHT ? popcount(discovered) <= 2
                                             : !more_than_one(discovered));
            while (discovered)
            {
                const Square threatenedSq = pop_lsb(discovered);
                const Piece  threatenedPc = piece_on(threatenedSq);
                add_dirty_threat<!PutPiece>(dts, leaper, threatenedPc, leaperSq, threatenedSq);
            }
        }
    }
    else
        incoming_threats |= (rAttacks & pieces(ROOK)) | (cAttacks & pieces(CANNON));

    while (incoming_threats)
    {
        Square src_sq = pop_lsb(incoming_threats);
        Piece  src_pc = piece_on(src_sq);

        assert(src_sq != s);
        assert(src_pc != NO_PIECE);

        add_dirty_threat<PutPiece>(dts, src_pc, pc, src_sq, s);
    }
}


// Used to do a "null move": it flips
// the side to move without executing any move on the board.
void Position::do_null_move(StateInfo& newSt, const TranspositionTable& tt) {

    assert(!checkers());
    assert(&newSt != st);

    // Update the bloom filter
    ++filter[st->key];

    std::memcpy(&newSt, st, sizeof(StateInfo));

    newSt.previous = st;
    st             = &newSt;

    st->key ^= Zobrist::side;
    prefetch(tt.first_entry(key()));

    st->pliesFromNull = 0;

    sideToMove = ~sideToMove;

    set_check_info();

    assert(pos_is_ok());
}


// Must be used to undo a "null move"
void Position::undo_null_move() {

    assert(!checkers());

    st         = st->previous;
    sideToMove = ~sideToMove;

    // Update the bloom filter
    --filter[st->key];
}


// Tests if the SEE (Static Exchange Evaluation)
// value of move is greater or equal to the given threshold. We'll use an
// algorithm similar to alpha-beta pruning with a null window.
bool Position::see_ge(Move m, int threshold) const {

    assert(m.is_ok());

    Square from = m.from_sq(), to = m.to_sq();

    assert(piece_on(from) != NO_PIECE);

    int swap = PieceValue[piece_on(to)] - threshold;
    if (swap < 0)
        return false;

    swap = PieceValue[piece_on(from)] - swap;
    if (swap <= 0)
        return true;

    assert(color_of(piece_on(from)) == sideToMove);
    Bitboard occupied  = pieces() ^ from ^ to;  // xoring to is important for pinned piece logic
    Color    stm       = sideToMove;
    Bitboard attackers = attackers_to(to, occupied);

    // Flying general
    bool kingAttacks = attackers & pieces(KING);
    if (kingAttacks)
        attackers |= attacks_bb<ROOK>(to, occupied) & pieces(KING);

    Bitboard nonCannons = attackers & ~pieces(CANNON);
    Bitboard cannons    = attackers & pieces(CANNON);
    Bitboard stmAttackers, bb;
    int      res = 1;

    while (true)
    {
        stm = ~stm;
        attackers &= occupied;

        // If stm has no more attackers then give up: stm loses
        if (!(stmAttackers = attackers & pieces(stm)))
            break;

        // Don't allow pinned pieces to attack as long as there are
        // pinners on their original square.
        if (pinners(~stm) & occupied)
        {
            stmAttackers &= ~blockers_for_king(stm);

            if (!stmAttackers)
                break;
        }

        res ^= 1;

        // Locate and remove the next least valuable attacker, and add to the
        // bitboard 'attackers' any protential attackers when it is removed.
        if ((bb = stmAttackers & pieces(PAWN)))
        {
            if ((swap = PawnValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);

            nonCannons |=
              attacks_bb<ROOK>(to, occupied) & (kingAttacks ? pieces(KING, ROOK) : pieces(ROOK));
            cannons   = attacks_bb<CANNON>(to, occupied) & pieces(CANNON);
            attackers = nonCannons | cannons;
        }

        else if ((bb = stmAttackers & pieces(BISHOP)))
        {
            if ((swap = BishopValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);
        }

        else if ((bb = stmAttackers & pieces(ADVISOR)))
        {
            if ((swap = AdvisorValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);

            nonCannons |= attacks_bb<KNIGHT_TO>(to, occupied) & pieces(KNIGHT);
            attackers = nonCannons | cannons;
        }

        else if ((bb = stmAttackers & pieces(CANNON)))
        {
            if ((swap = CannonValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);

            cannons   = attacks_bb<CANNON>(to, occupied) & pieces(CANNON);
            attackers = nonCannons | cannons;
        }

        else if ((bb = stmAttackers & pieces(KNIGHT)))
        {
            if ((swap = KnightValue - swap) < res)
                break;
            occupied ^= least_significant_square_bb(bb);
        }

        else if ((bb = stmAttackers & pieces(ROOK)))
        {
            swap = RookValue - swap;
            occupied ^= least_significant_square_bb(bb);

            nonCannons |=
              attacks_bb<ROOK>(to, occupied) & (kingAttacks ? pieces(KING, ROOK) : pieces(ROOK));
            cannons   = attacks_bb<CANNON>(to, occupied) & pieces(CANNON);
            attackers = nonCannons | cannons;
        }

        else  // KING
              // If we "capture" with the king but the opponent still has attackers,
              // reverse the result.
            return (attackers & ~pieces(stm)) ? res ^ 1 : res;
    }

    return bool(res);
}


// A lighter version of do_move(), used in chasing detection
std::pair<Piece, int> Position::do_move(Move m) {

    assert(capture(m));

    Square from     = m.from_sq();
    Square to       = m.to_sq();
    Piece  captured = piece_on(to);
    int    id       = idBoard[to];

    // Update id board
    idBoard[to]   = idBoard[from];
    idBoard[from] = 0;

    // Update board and piece lists
    remove_piece(to);
    move_piece(from, to);

    sideToMove = ~sideToMove;

    return {captured, id};
}


// A lighter version of undo_move(), used in chasing detection
void Position::undo_move(Move m, Piece captured, int id) {

    sideToMove = ~sideToMove;

    Square from = m.from_sq();
    Square to   = m.to_sq();

    // Put back id board
    idBoard[from] = idBoard[to];
    idBoard[to]   = id;

    move_piece(to, from);  // Put the piece back at the source square

    if (captured)
        put_piece(captured, to);  // Restore the captured piece
}


// Tests whether a pseudo-legal move is chase legal
bool Position::chase_legal(Move m) const {

    assert(m.is_ok());

    Color    us       = sideToMove;
    Square   from     = m.from_sq();
    Square   to       = m.to_sq();
    Bitboard occupied = (pieces() ^ from) | to;

    assert(color_of(moved_piece(m)) == us);
    assert(piece_on(king_square(us)) == make_piece(us, KING));

    // If the moving piece is a king, check whether the destination
    // square is not under new attack after the move.
    if (type_of(piece_on(from)) == KING)
        return !(checkers_to(~us, to, occupied));

    // A non-king move is chase legal if the king is not under new attack after the move.
    return !(checkers_to(~us, king_square(us), occupied) & ~square_bb(to));
}


// Calculates the chase information for a given color.
uint16_t Position::chased(Color c) {

    uint16_t chase = 0;

    std::swap(c, sideToMove);

    // King and pawn can legally perpetual chase
    Bitboard attackers = pieces(sideToMove) ^ pieces(sideToMove, KING, PAWN);
    while (attackers)
    {
        Square    from         = pop_lsb(attackers);
        PieceType attackerType = type_of(piece_on(from));
        Bitboard  attacks      = attacks_bb(attackerType, from, pieces());

        // Restrict to pinners if pinned, otherwise exclude attacks on unpromoted pawns and checks
        if (blockers_for_king(sideToMove) & from)
            attacks &= pinners(~sideToMove) & ~pieces(KING);
        else
            attacks &= (pieces(~sideToMove) ^ pieces(~sideToMove, KING, PAWN))
                     | (pieces(~sideToMove, PAWN) & HalfBB[sideToMove]);

        while (attacks)
        {
            Square to = pop_lsb(attacks);
            Move   m  = Move(from, to);

            if (chase_legal(m))
            {
                // Attacks against stronger pieces
                if ((attackerType == KNIGHT || attackerType == CANNON)
                    && type_of(piece_on(to)) == ROOK)
                    chase |= (1 << idBoard[to]);
                if ((attackerType == ADVISOR || attackerType == BISHOP)
                    && type_of(piece_on(to)) & 1)
                    chase |= (1 << idBoard[to]);
                // Attacks against potentially unprotected pieces
                else
                {
                    bool trueChase             = true;
                    const auto& [captured, id] = do_move(m);
                    Bitboard recaptures        = attackers_to(to) & pieces(sideToMove);
                    while (recaptures)
                    {
                        Square s = pop_lsb(recaptures);
                        if (chase_legal(Move(s, to)))
                        {
                            trueChase = false;
                            break;
                        }
                    }
                    undo_move(m, captured, id);

                    if (trueChase)
                    {
                        // Exclude mutual/symmetric attacks except pins
                        if (attackerType == type_of(piece_on(to)))
                        {
                            sideToMove = ~sideToMove;
                            if ((attackerType == KNIGHT && ((between_bb(from, to) ^ to) & pieces()))
                                || !chase_legal(Move(to, from)))
                                chase |= (1 << idBoard[to]);
                            sideToMove = ~sideToMove;
                        }
                        else
                            chase |= (1 << idBoard[to]);
                    }
                }
            }
        }
    }

    std::swap(c, sideToMove);

    return chase;
}


// Detects chases from state st - d to state st
Value Position::detect_chases(int d, int ply) {

    // Grant each piece on board a unique id for each side
    int whiteId = 0;
    int blackId = 0;
    for (Square s = SQ_A0; s <= SQ_I9; ++s)
        if (board[s] != NO_PIECE)
            idBoard[s] = color_of(board[s]) == WHITE ? whiteId++ : blackId++;

    Color us = sideToMove, them = ~us;

    // Rollback until we reached st - d
    uint16_t chase[COLOR_NB] = {0xFFFF, 0xFFFF};
    for (int i = 0; i < d; ++i)
    {
        if (st->checkersBB)
            return VALUE_DRAW;
        else if (!chase[~sideToMove])
        {
            if (!chase[sideToMove])
                break;
            undo_move(st->move, st->capturedPiece);
            st = st->previous;
        }
        else
        {
            uint16_t after = chased(~sideToMove);
            undo_move(st->move, st->capturedPiece);
            st = st->previous;
            // Take the exact diff to detect the chase
            chase[sideToMove] &= after & ~chased(sideToMove);
        }
    }

    return bool(chase[us]) ^ bool(chase[them]) ? chase[us] ? mated_in(ply) : mate_in(ply)
                                               : VALUE_DRAW;
}


// Tests whether the position may end the game by rule 60, insufficient material, draw repetition,
// perpetual check repetition or perpetual chase repetition that allows a player to claim a game result.
bool Position::rule_judge(Value& result, int ply) {

    // Restore rule 60 by adding back the checks
    int end = std::min(st->rule60 + std::max(0, st->check10[WHITE] - 10)
                         + std::max(0, st->check10[BLACK] - 10),
                       st->pliesFromNull);

    if (end >= 4 && filter[st->key] >= 1)
    {
        int        cnt       = 0;
        StateInfo* stp       = st->previous->previous;
        bool       checkThem = st->checkersBB && stp->checkersBB;
        bool       checkUs   = st->previous->checkersBB && stp->previous->checkersBB;

        for (int i = 4; i <= end; i += 2)
        {
            stp = stp->previous->previous;
            checkThem &= bool(stp->checkersBB);

            // Return a score if a position repeats once earlier but strictly
            // after the root, or repeats twice before or at the root.
            if (stp->key == st->key && (++cnt == 2 || ply > i))
            {
                if (!checkThem && !checkUs)
                {
                    // Copy the current position to a rollback struct, so we don't need to do those moves again
                    Position rollback;
                    memcpy((void*) &rollback, (const void*) this, offsetof(Position, filter));

                    // Chasing detection
                    result = rollback.detect_chases(i, ply);
                }
                else
                    // Checking detection
                    result = !checkUs ? mate_in(ply) : !checkThem ? mated_in(ply) : VALUE_DRAW;

                // 3 folds and 2 fold draws can be judged immediately
                if (result == VALUE_DRAW || cnt == 2)
                    return true;

                // 2 fold mates need further investigations
                if (filter[st->key] <= 1)
                {
                    // Not exceeding rule 60 and have the same previous step
                    if (st->rule60 < 120 && st->previous->key == stp->previous->key)
                    {
                        // Even if we entering this loop again, it will not lead to a 3 fold repetition
                        StateInfo* prev = st->previous;
                        while ((prev = prev->previous) != stp)
                            if (filter[prev->key] > 1)
                                break;
                        if (prev == stp)
                            return true;
                    }
                    // We know there can't be another fold
                    break;
                }
            }

            if (i + 1 <= end)
                checkUs &= bool(stp->previous->checkersBB);
        }
    }

    // 60 move rule
    if (st->rule60 >= 120)
    {
        result = MoveList<LEGAL>(*this).size() ? VALUE_DRAW : mated_in(ply);
        return true;
    }

    // Draw by insufficient material
    if (count<PAWN>() == 0)
    {
        enum DrawLevel : int {
            NO_DRAW,      // There is no drawing situation exists
            DIRECT_DRAW,  // A draw can be directly yielded without any checks
            MATE_DRAW     // We need to check for mate before yielding a draw
        };

        int level = [&]() {
            // No cannons left on the board
            if (!major_material())
                return DIRECT_DRAW;

            // One cannon left on the board
            if (major_material() == CannonValue)
            {
                // See which side is holding this cannon, and this side must not possess any advisors
                Color cannonSide = major_material(WHITE) == CannonValue ? WHITE : BLACK;
                if (count<ADVISOR>(cannonSide) == 0)
                {
                    // No advisors left on the board
                    if (count<ADVISOR>(~cannonSide) == 0)
                        return DIRECT_DRAW;

                    // One advisor left on the board
                    if (count<ADVISOR>(~cannonSide) == 1)
                        return count<BISHOP>(cannonSide) == 0 ? DIRECT_DRAW : MATE_DRAW;

                    // Two advisors left on the board
                    if (count<BISHOP>(cannonSide) == 0)
                        return MATE_DRAW;
                }
            }

            // Two cannons left on the board, one for each side, and no advisors left on the board
            if (major_material(WHITE) == CannonValue && major_material(BLACK) == CannonValue
                && count<ADVISOR>() == 0)
                return count<BISHOP>() == 0 ? DIRECT_DRAW : MATE_DRAW;

            return NO_DRAW;
        }();

        if (level != NO_DRAW)
        {
            if (level == MATE_DRAW)
            {
                MoveList<LEGAL> moves(*this);
                if (moves.size() == 0)
                {
                    result = mated_in(ply);
                    return true;
                }
                for (const auto& move : moves)
                {
                    StateInfo tempSt;
                    do_move(move, tempSt);
                    bool mate = MoveList<LEGAL>(*this).size() == 0;
                    undo_move(move);
                    if (mate)
                        return false;
                }
            }
            result = VALUE_DRAW;
            return true;
        }
    }

    return false;
}


// Flips position with the white and black sides reversed. This
// is only useful for debugging e.g. for finding evaluation symmetry bugs.
void Position::flip() {

    string            f, token;
    std::stringstream ss(fen());

    for (Rank r = RANK_9;; --r)  // Piece placement
    {
        std::getline(ss, token, r > RANK_0 ? '/' : ' ');
        f.insert(0, token + (f.empty() ? " " : "/"));

        if (r == RANK_0)
            break;
    }

    ss >> token;                        // Active color
    f += (token == "w" ? "B " : "W ");  // Will be lowercased later

    ss >> token;
    f += token + " ";

    std::transform(f.begin(), f.end(), f.begin(),
                   [](char c) { return char(islower(c) ? toupper(c) : tolower(c)); });

    ss >> token;
    f += token;

    std::getline(ss, token);  // Half and full moves
    f += token;

    set(f, st);

    assert(pos_is_ok());
}


// Performs some consistency checks for the position object
// and raise an assert if something wrong is detected.
// This is meant to be helpful when debugging.
bool Position::pos_is_ok() const {

    constexpr bool Fast = true;  // Quick (default) or full check?

    if ((sideToMove != WHITE && sideToMove != BLACK) || piece_on(king_square(WHITE)) != W_KING
        || piece_on(king_square(BLACK)) != B_KING)
        assert(0 && "pos_is_ok: Default");

    if (Fast)
        return true;

    if (pieceCount[W_KING] != 1 || pieceCount[B_KING] != 1
        || checkers_to(sideToMove, king_square(~sideToMove)))
        assert(0 && "pos_is_ok: Kings");

    if ((pieces(WHITE, PAWN) & ~PawnBB[WHITE]) || (pieces(BLACK, PAWN) & ~PawnBB[BLACK])
        || pieceCount[W_PAWN] > 5 || pieceCount[B_PAWN] > 5)
        assert(0 && "pos_is_ok: Pawns");

    if ((pieces(WHITE) & pieces(BLACK)) || (pieces(WHITE) | pieces(BLACK)) != pieces()
        || popcount(pieces(WHITE)) > 16 || popcount(pieces(BLACK)) > 16)
        assert(0 && "pos_is_ok: Bitboards");

    for (PieceType p1 = PAWN; p1 <= KING; ++p1)
        for (PieceType p2 = PAWN; p2 <= KING; ++p2)
            if (p1 != p2 && (pieces(p1) & pieces(p2)))
                assert(0 && "pos_is_ok: Bitboards");

    for (Piece pc : Pieces)
        if (pieceCount[pc] != popcount(pieces(color_of(pc), type_of(pc)))
            || pieceCount[pc] != std::count(board.begin(), board.end(), pc))
            assert(0 && "pos_is_ok: Pieces");

    return true;
}

}  // namespace Stockfish
