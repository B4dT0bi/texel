/*
    Texel - A UCI chess engine.
    Copyright (C) 2012-2013  Peter Österlund, peterosterlund2@gmail.com

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/*
 * book.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: petero
 */

#include "book.hpp"
#include "position.hpp"
#include "moveGen.hpp"
#include "textio.hpp"
#include "util/timeUtil.hpp"

#include <cassert>


Book::BookMap Book::bookMap;
Random Book::rndGen;

int Book::numBookMoves = -1;


void
Book::getBookMove(Position& pos, Move& out) {
    initBook();
    out = Move();
    BookMap::iterator it = bookMap.find(pos.zobristHash());
    if (it == bookMap.end())
        return;
    const std::vector<BookEntry>& bookMoves = it->second;

    MoveGen::MoveList legalMoves;
    MoveGen::pseudoLegalMoves(pos, legalMoves);
    MoveGen::removeIllegal(pos, legalMoves);
    int sum = 0;
    for (size_t i = 0; i < bookMoves.size(); i++) {
        const BookEntry& be = bookMoves[i];
        bool contains = false;
        for (int mi = 0; mi < legalMoves.size; mi++)
            if (legalMoves[mi].equals(be.move)) {
                contains = true;
                break;
            }
        if  (!contains) {
            // If an illegal move was found, it means there was a hash collision.
            return;
        }
        sum += getWeight(be.count);
    }
    if (sum <= 0)
        return;
    int rnd = rndGen.nextInt(sum);
    sum = 0;
    for (size_t i = 0; i < bookMoves.size(); i++) {
        sum += getWeight(bookMoves[i].count);
        if (rnd < sum) {
            out = bookMoves[i].move;
            return;
        }
    }
    // Should never get here
    assert(false);
}

/** Return a string describing all book moves. */
std::string
Book::getAllBookMoves(const Position& pos) {
    initBook();
    std::string ret;
    BookMap::iterator it = bookMap.find(pos.zobristHash());
    if (it != bookMap.end()) {
        std::vector<BookEntry>& bookMoves = it->second;
        for (size_t i = 0; i < bookMoves.size(); i++) {
            BookEntry& be = bookMoves[i];
            std::string moveStr = TextIO::moveToString(pos, be.move, false);
            ret += moveStr;
            ret += '(';
            ret += num2Str(be.count);
            ret += ") ";
        }
    }
    return ret;
}

void
Book::initBook() {
    if (numBookMoves >= 0)
        return;
    U64 t0 = currentTimeMillis();
    bookMap.clear();
    rndGen.setSeed(currentTimeMillis());
    numBookMoves = 0;
    std::vector<byte> buf;
    createBinBook(buf);

    Position startPos(TextIO::readFEN(TextIO::startPosFEN));
    Position pos(startPos);
    UndoInfo ui;
    for (size_t i = 0; i < buf.size(); i += 2) {
        int b0 = buf[i]; if (b0 < 0) b0 += 256;
        int b1 = buf[i+1]; if (b1 < 0) b1 += 256;
        int move = (b0 << 8) + b1;
        if (move == 0) {
            pos = startPos;
        } else {
            bool bad = ((move >> 15) & 1) != 0;
            int prom = (move >> 12) & 7;
            Move m(move & 63, (move >> 6) & 63,
                   promToPiece(prom, pos.getWhiteMove()));
            if (!bad)
                addToBook(pos, m);
            pos.makeMove(m, ui);
        }
    }
    if (verbose) {
        S64 t1 = currentTimeMillis();
        std::stringstream ss;
        ss.precision(3);
        ss << std::fixed << ((t1 - t0) / 1000.0);
        std::cout << "Book moves:" << numBookMoves << " (parse time:" << ss.str() << ')' << std::endl;
    }
}

/** Add a move to a position in the opening book. */
void
Book::addToBook(const Position& pos, const Move& moveToAdd) {
    BookMap::iterator it = bookMap.find(pos.zobristHash());
    if (it == bookMap.end())
        it = bookMap.insert(std::make_pair(pos.zobristHash(),
                                           std::vector<BookEntry>())).first;

    std::vector<BookEntry>& ent = it->second;
    for (size_t i = 0; i < ent.size(); i++) {
        BookEntry& be = ent[i];
        if (be.move.equals(moveToAdd)) {
            be.count++;
            return;
        }
    }
    BookEntry be(moveToAdd);
    ent.push_back(be);
    numBookMoves++;
}

int
Book::getWeight(int count) {
    double tmp = ::sqrt((double)count);
    return (int)(tmp * ::sqrt(tmp) * 100 + 1);
}

void
Book::createBinBook(std::vector<byte>& binBook) {
    for (size_t i = 0; bookLines[i]; i++) {
        const char* line = bookLines[i];
        if (!addBookLine(line, binBook)) {
            std::cout << "Book parse error, line:" << i << std::endl;
            assert(false);
        }
//        std::cout << "no:" << i << " line:" << line << std::endl;
    }
}

/** Add a sequence of moves, starting from the initial position, to the binary opening book. */
bool
Book::addBookLine(const std::string& line, std::vector<byte>& binBook) {
    Position pos(TextIO::readFEN(TextIO::startPosFEN));
    UndoInfo ui;
    std::vector<std::string> strMoves;
    splitString(line, strMoves);
    for (size_t i = 0; i < strMoves.size(); i++) {
        std::string strMove = strMoves[i];
//        std::cout << "Adding move:" << strMove << std::endl;
        int bad = 0;
        if ((strMove.length() > 0) && (strMove[strMove.length()-1] == '?')) {
            strMove = strMove.substr(0, strMove.length() - 1);
            bad = 1;
        }
        Move m(TextIO::stringToMove(pos, strMove));
        if (m.isEmpty())
            return false;
        int prom = pieceToProm(m.promoteTo());
        int val = m.from() + (m.to() << 6) + (prom << 12) + (bad << 15);
        binBook.push_back((byte)(val >> 8));
        binBook.push_back((byte)(val & 255));
        pos.makeMove(m, ui);
    }
    binBook.push_back((byte)0);
    binBook.push_back((byte)0);
    return true;
}

int
Book::pieceToProm(int p) {
    switch (p) {
    case Piece::WQUEEN:  case Piece::BQUEEN:  return 1;
    case Piece::WROOK:   case Piece::BROOK:   return 2;
    case Piece::WBISHOP: case Piece::BBISHOP: return 3;
    case Piece::WKNIGHT: case Piece::BKNIGHT: return 4;
    default:                                  return 0;
    }
}

int
Book::promToPiece(int prom, bool whiteMove) {
    switch (prom) {
    case 1: return whiteMove ? Piece::WQUEEN : Piece::BQUEEN;
    case 2: return whiteMove ? Piece::WROOK  : Piece::BROOK;
    case 3: return whiteMove ? Piece::WBISHOP : Piece::BBISHOP;
    case 4: return whiteMove ? Piece::WKNIGHT : Piece::BKNIGHT;
    default: return Piece::EMPTY;
    }
}



const char*
Book::bookLines[] = {
    // Philidors defense
    "e4 e5 Nf3 d6 d4 exd4 Nxd4 Nf6 Nc3 Be7 Bf4 O-O Qd2 d5 exd5 Nxd5 Nxd5 Qxd5 Nb5 Qe4+",
    "e4 e5 Nf3 d6? d4 exd4 Qxd4 Nf6 Nc3 Be7 Bg5 Nc6 Bb5 O-O",
    "e4 e5 Nf3 d6? d4 Nd7 Bc4 c6 Ng5 Nh6 O-O Nb6",

    // Nordic gambit
    "e4 e5 d4 exd4 c3 dxc3 Bc4 cxb2 Bxb2 d5 Bxd5 Nf6 Bxf7+ Kxf7 Qxd8 Bb4+",
    "e4 e5 d4 exd4 c3? d5 exd5 Qxd5 cxd4 Nc6 Nf3 Bg4 Be2 Bb4 Nc3 Bxf3 Bxf3 Qc4",
    "e4 e5 d4 exd4 c3? d5 exd5 Qxd5 Nf3 Nc6 cxd4",
    "e4 e5 d4? exd4 c3? d5 exd5 Qxd5 Nf3 Nc6 Be2 Nf6",
    "e4 e5 d4? exd4 c3? d5 exd5 Qxd5 Nf3 Nc6 Be2 Bg4 O-O O-O-O cxd4",
    "e4 e5 d4? exd4 Qxd4 Nc6 Qe3 Nf6 Nc3 Bb4 Bd2 O-O O-O-O Re8 Bc4 d6",

    //Scottish game
    "e4 e5 Nf3 Nc6 d4 exd4 Nxd4 Nf6 Nc3 Bb4 Nxc6 bxc6 Bd3 d5 exd5",
    "e4 e5 Nf3 Nc6 d4 exd4 Nxd4 Bc5 Be3 Qf6 c3 Nge7",
    "e4 e5 Nf3 Nc6 d4 exd4 Nxd4 Bc5 Nb3 Bb6 a4 a5 Nc3 Qf6 Qe2 Nge7",
    "e4 e5 d4 exd4 Nf3 Nc6 Nxd4 Nf6 Nxc6 bxc6 e5 Qe7 Qe2 Nd5 c4 Ba6 b3 g6 f4 f6 Ba3 Qf7 Qd2 Nb6",

    // Italian game
    "e4 e5 Nf3 Nc6 Bc4 Bc5 d3 d6 Nc3 Nf6 Bg5 h6",
    "e4 e5 Nf3 Nc6 Bc4 Bc5 c3 Nf6 d4 exd4 cxd4 Bb4+ Bd2 Bxd2+ Nbxd2 d5 exd5 Nxd5 Qb3 Nce7",
    "e4 e5 Nf3 Nc6 Bc4 Bc5 c3 Qe7 d4 Bb6 O-O d6 h3 Nf6 Re1 O-O Na3",
    "e4 e5 Nf3 Nc6 Bc4 Bc5 c3 Nf6 d3 d6 O-O Qe7 b4 Bb6 a4 a6",

    // Two knights defense
    "e4 e5 Nf3 Nc6 Bc4 Nf6 Nc3 Nxe4 Nxe4 d5 Bd3",
    "e4 e5 Nf3 Nc6 Bc4 Nf6 Ng5 d5 exd5 Na5 Bb5+ c6 dxc6 bxc6 Be2 h6 Nf3 e4 Ne5 Qd4",
    "e4 e5 Nf3 Nc6 Bc4 Nf6 Ng5 d5 exd5 Na5 Bb5+ c6 dxc6 bxc6 Be2 h6 Nf3 e4 Ne5 Qc7",
    "e4 e5 Nf3 Nc6 Bc4 Nf6 d3 Bc5 Nc3 d6 Bg5 h6",

    // Max Lange attack
    "e4 e5 Nf3 Nc6 Bc4 Nf6 d4 exd4 O-O Bc5 e5 Ng4 Bf4 d6 exd6 Bxd6 Re1+ Kf8 Bxd6+ Qxd6 c3 Qc5",
    "e4 e5 Nf3 Nc6 Bc4 Bc5 O-O Nf6 d4 exd4 e5 d5 exf6 dxc4 Re1+ Be6 Ng5 Qd5 Nc3 Qf5",
    "e4 e5 Nf3 Nc6 d4 exd4 Bc4 Nf6 O-O Bc5",
    "e4 e5 Nf3 Nc6 d4 exd4 Bc4 Bc5 O-O Nf6",

    // Scottish gambit
    "e4 e5 Nf3 Nc6 Bc4 Nf6 d4 exd4 O-O Nxe4 Re1 d5 Bxd5 Qxd5 Nc3 Qa5 Nxe4 Be6",
    "e4 e5 Nf3 Nc6 d4 exd4 Bc4 Nf6 e5 d5 Bb5 Ne4 Nxd4 Bd7 Bxc6 bxc6 O-O Be7 f3 Nc5 f4 Ne4",
    "e4 e5 Nf3 Nc6 d4 exd4 Bc4 Nf6 e5 d5 Bb5 Ne4 Nxd4 Bd7 Bxc6 bxc6 O-O Bc5 Be3 O-O f3 Ng5 Qd2 f6 Kh1 Ne6",
    "e4 e5 Nf3 Nc6 d4 exd4 Bc4 Bc5 O-O d6 c3 Bg4 Qb3 Bxf3 Bxf7 Kf8 gxf3 Ne5 cxd4 Bxd4",
    "e4 e5 Nf3 Nc6 d4 exd4 c3 d5 exd5 Qxd5 cxd4 Bg4 Be2 Bb4 Nc3 Bxf3 Bxf3 Qc4 Qb3",

    // Hungarian
    "e4 e5 Nf3 Nc6 Bc4 Be7 d4 d6 dxe5 dxe5 Qxd8+ Bxd8",

    // Three and four knights game
    "e4 e5 Nf3 Nc6 Nc3 Nf6 Bb5 Nd4 Nxe5 Qe7 Nf3 Nxb5 Nxb5 Qxe4+ Qe2 Qxe2+ Kxe2 Nd5",
    "e4 e5 Nf3 Nf6 Nc3? Nc6 d4 exd4 Nxd4",

    // Russian defense
    "e4 e5 Nf3 Nf6 Nxe5 d6 Nf3 Nxe4 Qe2 Qe7 d3 Nf6 Bg5 Nbd7 Nc3 Qxe2+ Bxe2 h6 Bh4 g6",
    "e4 e5 Nf3 Nf6 Nxe5 d6 Nf3 Nxe4 d4 d5 Bd3 Be7 O-O Nc6 Re1 Bg4 c4 Nf6 cxd5 Nxd5 Nc3 O-O Be4 Be6",
    "e4 e5 Nf3 Nf6? d4 Nxe4 dxe5 d5 Nbd2 Nc6",

    // Kings gambit
    "e4 e5 f4 exf4 Nf3 d5 exd5 Nf6 Nc3 Nxd5 Nxd5 Qxd5 d4 Be7 c4 Qe4+ Be2 Nc6",
    "e4 e5 f4 exf4 Nf3? Be7 Bc4 Nf6 e5 Ng4 O-O Nc6 d4 d5 exd6 Qxd6",
    "e4 e5 f4? exf4 Bc4 Nf6 Nc3 c6 d4 Bb4 Ne2 d5 exd5 f3",
    "e4 e5 f4? d5 exd5 exf4 Nf3 Nf6 Nc3 Nxd5 Bc4 Qe7 Qe2",
    "e4 e5 f4? d5? exd5 e4? d3 Nf6 dxe4 Nxe4 Nf3 Bc5 Qe2",

    // Spanish
    "e4 e5 Nf3 Nc6 Bb5 d6 d4 Bd7 Nc3 Nf6 O-O Be7 Re1 exd4 Nxd4 O-O",
    "e4 e5 Nf3 Nc6 Bb5 Nf6 O-O Nxe4 Re1 Nd6 Nxe5 Be7 Bd3 O-O",
    "e4 e5 Nf3 Nc6 Bb5 Nf6 O-O Nxe4 d4 Nd6",
    "e4 e5 Nf3 Nc6 Bb5 a6 Ba4 Nf6 O-O Be7 Re1 b5 Bb3 O-O c3 d6 h3 h6 d4 Re8 Nbd2 Bf8 Nf1 Bb7 Ng3 Na5 Bc2 Nc4 a4 d5",
    "e4 e5 Nf3 Nc6 Bb5 a6 Ba4 Nf6 O-O Be7 Re1 b5 Bb3 d6 c3 O-O d4 Bg4 d5 Na5 Bc2 c6 h3 Bc8 dxc6 Qc7 Nbd2 Qxc6 Nf1 Nc4",
    "e4 e5 Nf3 Nc6 Bb5 a6 Ba4 Nf6 O-O Be7 Re1 b5 Bb3 d6 c3 Na5 Bc2 c5 d4 Nc6 d5",
    "e4 e5 Nf3 Nc6 Bb5 a6 Ba4 Nf6 O-O Be7 Re1 b5 Bb3 d6 c3 Na5 Bc2 c5 d4 Nc6 h3 Qc7 d5",
    "e4 e5 Nf3 Nc6 Bb5 a6 Ba4 Nf6 O-O Be7 Re1 b5 Bb3 d6 c3 Na5 Bc2 c5 d4 cxd4 cxd4 Qc7",
    "e4 e5 Nf3 Nc6 Bb5 a6 Ba4 Nf6 O-O Be7 Re1 b5 Bb3 d6 c3 Na5 Bc2 c5 d3 Nc6 Nbd2 O-O Nf1 Re8 h3 h6 Ne3 Bf8",
    "Nf3? Nc6 e4 e5 Bb5 a6 Ba4 Nf6 O-O Nxe4 d4 b5 Bb3 d5 dxe5 Be6 c3 Bc5",
    "e4 e5 Nf3 Nc6 Bb5 a6 Ba4 d6 O-O Bd7 c3 g6 d4 Bg7 Re1 Nge7 Be3 O-O Nbd2 h6 dxe5 dxe5 Bb3 b6 a4",
    "e4 e5 Nf3 Nc6 Bb5 a6 Ba4 d6 c3 Bd7 d4 Nge7 Bb3 h6 Nbd2 Ng6 Nc4 Be7 Ne3 O-O",
    "e4 e5 Nf3 Nc6 Bb5 Nf6 O-O Bc5 c3 O-O d4 Bb6 Bg5 h6 Bh4 d6 a4 a5 Re1 exd4 Bxc6 bxc6 Nxd4",
    "e4 e5 Nf3 Nc6 Bb5 Nf6? O-O Bc5 Nxe5 Nxe5 d4 a6 Ba4 b5 Bb3 Bxd4 Qxd4 d6",
    "e4 e5 Nf3 Nc6 Bb5 Nf6? O-O Bc5? Nc3? O-O d3 d6",

    // Scandinavian
    "e4 d5 exd5 Qxd5 Nc3 Qa5 d4 Nf6 Nf3 Bf5 Bc4 e6 Bd2 c6 Qe2 Bb4 Ne5 Nbd7 Nxd7 Nxd7 a3",
    "e4 d5? exd5 Qxd5 Nc3 Qa5 d4 c6 Nf3 Nf6 Bc4 Bg4 h3 Bh5 g4 Bg6 Bd2 Qb6 Qe2",
    "e4 d5 exd5 Nf6 d4 Nxd5 c4 Nb6 Nf3 g6 Nc3 Bg7 Be3 O-O h3 Nc6 Qd2 e5 d5",
    "e4 d5? exd5 Nf6 d4 Nxd5 Nf3 g6 c4 Nb6",
    "e4 d5? exd5 Nf6 d4 Nxd5 Nf3 g6 Be2 Bg7 O-O O-O c4 Nb6 Nc3 Nc6 d5 Ne5",

    // Queens gambit accepted
    "d4 d5 c4 dxc4 Nf3 Nf6 e3 Bg4 Bxc4 e6 h3 Bh5 Nc3",
    "d4 d5 c4 dxc4? e3 Nf6 Nf3 Bg4",
    "d4 d5 c4 dxc4? e3? Nf6 Nf3 e6 Bxc4 c5 O-O a6",

    // Queens gambit declined
    "d4 d5 c4 e6 Nc3 Nf6 Bg5 Nbd7 e3 Be7 Nf3 O-O Rc1 c6",
    "c4 e6 d4 d5 Nf3 Be7 Nc3 Nf6 Bg5 O-O e3 h6",
    "c4 Nf6 Nc3 e6 Nf3 d5 d4 Be7 Bg5 O-O e3 Nbd7 Qc2 c5",
    "c4? Nf6 Nc3 e6 Nf3 d5 d4 Be7 e3 O-O Bd3 c5",
    "Nf3? d5 d4 Nf6 c4 e6 Nc3 Be7 Bf4 O-O e3 c5 dxc5 Bxc5 Qc2 Nc6",
    "d4 d5 c4 c6 Nf3 Nf6 Nc3 e6 e3 Nbd7 Bd3 dxc4 Bxc4 b5 Bd3 a6 O-O",
    "d4 d5 c4 c6 Nf3 Nf6 Nc3 e6 Bg5 h6 Bh4 dxc4 e4 g5 Bg3 b5 Be2 Bb7",
    "d4 d5 c4 c6? cxd5 cxd5 Nc3 Nf6 Bf4 Nc6 e3 a6",
    "d4 d5 c4 c6? Nc3 Nf6 e3 e6 Nf3",

    // Tarrasch defense
    "d4 d5 c4 e6 Nc3 c5 cxd5 exd5 Nf3 Nc6 g3 Nf6 Bg2 Be7 O-O O-O Bg5 cxd4 Nxd4 h6",

    // Budapest defense
    "d4 Nf6 c4 e5 dxe5 Ng4 Nf3 Bc5 e3 Nc6 Be2 Ngxe5 O-O d6",

    // Sicilian
    "e4 c5 Nf3 Nc6 d4 cxd4 Nxd4 e6 Nc3 Qc7",
    "e4 c5 Nf3 Nc6 d4 cxd4 Nxd4 Nf6 Nc3 e5 Ndb5 d6 Bg5 a6 Na3 b5",
    "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 a6 f4 e5 Nf3 Qc7 Bd3",
    "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 a6 f4 e6 Qf3 Qb6 Nb3 Qc7",
    "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 a6 f4 Nbd7? Be2",
    "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 a6 Be2 e5 Nb3 Be7 O-O O-O Be3",
    "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 a6 Be3 e5 Nb3 Be6 Qd2 Nbd7 f3 b5",
    "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 a6 Bg5 e6 f4 Be7 Qf3 Qc7 O-O-O Nbd7 g4 b5",
    "e4 c5 Nf3 e6 d4 cxd4 Nxd4 a6 Bd3 Nf6 O-O Qc7 Qe2 d6 c4 g6 Nc3 Bg7 Rd1 O-O",
    "e4 c5 Nf3 e6 d4 cxd4 Nxd4 Nf6 Nc3 d6 Be2 a6 O-O Be7 f4 O-O",
    "e4 c5 Nf3 e6 d4 cxd4 Nxd4 Nc6 Nc3 Qc7 Be3 a6 Bd3 Nf6 O-O Ne5 h3 Bc5 Qe2 d6",
    "e4 c5 Nf3 e6 d4 cxd4 Nxd4 Nc6 Nc3 Qc7 f4 a6 Be2 b5",
    "e4 c5 Nf3 e6? Nc3 Nc6 d4 cxd4 Nxd4 Qc7 Be3 a6 Qd2 Nf6 O-O-O Be7",
    "e4 c5 Nc3 Nc6 Nge2 g6 d4 cxd4 Nxd4 Bg7 Be3 Nf6 Bc4 O-O Bb3 d6",
    "e4 c5 Nc3 Nc6 f4 d6 Nf3 g6 Bb5 Bd7 O-O Bg7 d3 a6 Bc4 Na5 e5 Nxc4 dxc4 Be6",
    "e4 c5 Nc3? Nc6 g3 g6 Bg2 Bg7 d3 d6 f4 e6 Nf3 Nge7 O-O O-O",
    "e4 c5 Nc3? e6 Nf3 Nc6 d4 cxd4 Nxd4 Qc7 Be2 a6 O-O Nf6 Be3 Bb4",
    "e4 c5 Nc3? e6? g3? Nc6 Bg2 Nf6",
    "e4 c5 Nc3? a6 Nf3 d6 d4 cxd4 Nxd4 Nf6",
    "e4 c5 Nc3? d6 f4 g6 Nf3 Bg7 Bc4 Nc6 O-O Nf6",
    "Nc3? c5 Nf3 Nc6 d4 cxd4 Nxd4 Nf6 e4 d6 Bg5 e6 Qd2 a6 O-O-O Bd7 f4 b5",
    "e4 c5 d4 cxd4 c3 dxc3 Nxc3 Nc6 Nf3 d6 Bc4 e6 O-O Nf6 Qe2 Be7 Rd1 e5",
    "e4 c5 c3 d5 exd5 Qxd5 d4 Nf6 Nf3 Bg4 Be2 e6 O-O Nc6 Be3 cxd4 cxd4 Be7",
    "e4 c5 c3? Nf6 e5 Nd5 Nf3 Nc6 Bc4 Nb6 Bb3 c4 Bc2 Qc7 Qe2 g5",
    "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6 Nc3 g6 Be3 Bg7 f3 O-O Qd2 Nc6 Bc4 Bd7 O-O-O Rc8 Bb3 Ne5 h4 Nc4 Bxc4 Rxc4 g4 Qa5",

    // French defense
    "d4 e6 e4 d5 exd5 exd5 Nf3 Nf6 Bd3 Bd6 O-O O-O Bg5 Bg4 Nbd2 Nbd7 c3 c6 Qc2 Qc7",
    "e4 e6 d4 d5 exd5 exd5 Bd3 Bd6 Nf3 Nf6",
    "e4 e6 d4 d5 exd5 exd5 Bd3 Bd6 Nf3 Ne7 O-O O-O Bg5 f6 Bd2 Bf5",
    "e4 e6 d4 d5 e5 c5 c3 Nc6 Nf3 Qb6 Be2 cxd4 cxd4 Nge7 Nc3 Nf5 Na4 Qa5+ Bd2 Bb4 Bc3",
    "e4 e6 d4 d5 e5 c5 c3 Nc6 Nf3 Qb6 Bd3 cxd4 cxd4 Bd7",
    "e4 e6 d4 d5 e5 c5 c3 Nc6 Nf3 Qb6 a3 c4 Nbd2 Na5 Be2 Bd7",
    "e4 e6 d4 d5 Nc3 Nf6 Bg5 Be7 e5 Nfd7 Bxe7",
    "e4 e6 d4 d5 Nd2 Nf6 e5 Nfd7 Bd3 c5 c3 Nc6 Ne2 cxd4 cxd4 f6 exf6 Nxf6",
    "e4 e6? d4 d5 Nc3 Nf6 e5 Nfd7 f4 c5 Nf3 Nc6 Be3 cxd4 Nxd4 Bc5 Qd2 O-O O-O-O a6",
    "e4 e6? d4 d5 Nc3 Bb4 e5 c5 a3 Bxc3 bxc3 Ne7? Qg4 Qc7 Qxg7 Rg8 Qxh7 cxd4 Ne2 Nbc6 f4 Bd7",
    "e4 e6? d4 d5 Nc3 Bb4 e5 c5 a3 Bxc3 bxc3 Qc7 Qg4 f6 f4 f5 Qg3 cxd4 cxd4 Ne7 c3 b6",
    "e4 e6? d4 d5 Nc3 Bb4 e5 c5 Qg4",
    "e4 e6? d4 d5 Nc3 Bb4 e5 c5 Nf3 Ne7 a3",
    "e4 e6? d4 c5? d5 exd5 exd5 d6",
    "e4 e6 d3 d5 Nd2 c5 Ngf3 Nc6 g3 Nf6 Bg2 Be7 O-O O-O",

    // Caro Kann defense
    "e4 c6 d4 d5 Nc3 dxe4 Nxe4 Bf5 Ng3 Bg6 h4 h6 Nf3 Nd7",
    "e4 c6 d4 d5 Nd2 dxe4 Nxe4",
    "e4 c6 d4 d5 exd5 cxd5 c4 Nf6 Nc3 e6 Nf3 Be7",
    "e4 c6 d4 d5 e5 Bf5 Nf3 e6 Be2 c5 O-O Nc6 c3 cxd4 cxd4 Nge7 Nc3 Nc8 Be3 Nb6 Rc1 Be7",
    "e4 c6? d4 d5 e5 Bf5 Nf3 e6 Be2 c5 Be3 Qb6 Nc3 Nc6",
    "e4 c6? d3 d5 Nd2 e5 Ngf3 Bd6 g3 Nf6 Bg2 O-O O-O",
    "e4 c6? d4 d5 Nc3 dxe4 Nxe4 Bf5 Ng3 Bg6 Nf3 Nd7 h4 h6 h5 Bh7 Bd3 Bxd3 Qxd3 e6",

    // Aljechins defense
    "e4 Nf6 e5 Nd5 d4 d6 c4 Nb6 exd6 cxd6 Be3 g6",
    "e4 Nf6 e5 Nd5 c4 Nb6 d4 d6 exd6 cxd6 Nf3 g6 Be2 Bg7 O-O O-O Nc3 Nc6 Be3 Bg4 b3 d5",
    "e4 Nf6 e5 Nd5 d4 d6 Nf3 Bg4 Be2 e6 c4 Nb6 exd6 cxd6",
    "e4 Nf6? Nc3 d5 e5 Nfd7 d4 e6 f4 c5 Nf3 Nc6 Be3 a6 Qd2 b5 dxc5 Bxc5 Bxc5 Nxc5",
    "e4 Nf6? Nc3 d5 e5 Nfd7 d4 e6 f4 c5 Nf3 Nc6 Be3 cxd4 Nxd4 Bc5 Qd2 O-O O-O-O a6",
    "e4 Nf6? e5 Nd5 c4 Nb6 d4 d6 Nf3 Bg4 exd6 exd6 Be2 Be7 O-O O-O Nc3 Nc6 b3 Bf6 Be3 d5",

    // Kings indian
    "d4 Nf6 c4 g6 Nc3 Bg7 e4 d6 Nf3 O-O Be2 e5 O-O Nc6",
    "d4 Nf6 c4 g6 Nc3 Bg7 g3 O-O Bg2 d6 Nf3 Nbd7 O-O e5",
    "c4 Nf6 Nf3 g6 d4 Bg7 Nc3 O-O e4 d6 Be2",
    "c4 Nf6 Nc3 g6 d4 d5 cxd5 Nxd5 e4 Nxc3 bxc3 Bg7 Bc4 c5",
    "d4 Nf6 c4 g6 Nc3 d5 cxd5 Nxd5 e4 Nxc3 bxc3 Bg7 Nf3 c5",
    "Nf3? Nf6 c4 g6 Nc3 Bg7 d4 O-O",

    // Queen indian
    "d4 Nf6 c4 e6 Nf3 b6 g3 Bb7 Bg2 Be7",
    "c4 Nf6 d4 e6 Nf3 b6 Nc3 Bb7 a3 d5 cxd5 Nxd5 Qc2",
    "d4 e6 Nf3 Nf6 c4 b6 g3 Bb7 Bg2 Bb4 Bd2 Bxd2 Qxd2",

    // Nimzo indian
    "d4 e6 c4 Nf6 Nc3 Bb4 Bg5 h6 Bh4 c5 d5 d6",
    "c4 e6 d4 Nf6 Nc3 Bb4 a3 Bxc3+ bxc3 c5 f3 d5",
    "d4 Nf6 c4 e6 Nc3 Bb4 Qc2 d5 a3 Bxc3+ Qxc3 Ne4 Qc2 Nc6 e3 e5",
    "d4 Nf6 c4 e6 Nc3 Bb4 Qc2 O-O a3 Bxc3+ Qxc3 b6 Bg5 Bb7 e3 d6",
    "d4 Nf6 c4 e6 Nc3 Bb4 Nf3 O-O Bg5 c5 Rc1 cxd4",
    "d4 Nf6 c4 e6 Nc3 Bb4 f3 d5 a3 Bxc3+ bxc3 c5",
    "d4 Nf6 c4 e6 Nc3 Bb4 e3 c5 Bd3 d5 Nf3 O-O O-O Nc6",
    "d4 Nf6 c4 e6? Nc3 Bb4 e3 O-O Bd3 d5 Nf3 c5 O-O",
    "c4 Nf6 Nc3 e6 d4",

    // Benoni
    "d4 Nf6 Nf3 e6 c4 c5 d5 exd5 cxd5 d6 Nc3 g6 e4 Bg7",
    "d4 Nf6 c4 c5 d5 e6 Nc3 exd5 cxd5 d6 e4 g6 f4 Bg7 Bb5 Nfd7 a4 O-O Nf3 Na6 O-O Nc7",
    "d4 c5 d5 Nf6 c4 e6 Nc3 exd5 cxd5 d6 Nf3 g6 Bf4 a6 a4 Bg7 e4 O-O",
    "c4 Nf6 d4 g6 Nc3 Bg7 g3 O-O Bg2 c5 d5 e6 Nf3 exd5 cxd5 d6 O-O",
    "d4 c5? d5 Nf6 c4 e6 g3 exd5 cxd5 d6 Nc3 g6 Nf3 Bg7 Bg2 O-O",

    // Reti's opening
    "Nf3 d5 g3 g6 Bg2 Bg7 O-O e5 d3 Ne7 Nbd2 O-O c4 c6",
    "Nf3 Nf6 g3 g6 c4 Bg7 Bg2 O-O O-O c5 d4 d6 d5 Na6 Nc3 Nc7",
    "Nf3? Nf6 g3 d5 d4 c5 Bg2 Nc6 O-O g6",
    "Nf3? Nf6 g3 d5 Bg2 c6 d4 Bf5 O-O g6",
    "Nf3? d5 d4 Nf6 c4 e6 g3 dxc4 Bg2 Nc6 Qa4 Bb4 Bd2 Nd5 Bxb4 Nxb4 O-O Rb8",
    "Nf3? c5 c4 Nf6 Nc3 e6 g3 Be7 Bg2 O-O O-O a6 d4 cxd4 Nxd4 Qc7",
    "g3 g6 Bg2 Bg7 c4 Nf6 Nc3 O-O Nf3 d6 d4 Nbd7 O-O e5 e4",
    "g3? d5 Bg2 Nf6 Nf3 c6 O-O Bf5 d3 e6 Nbd2 h6 b3 Be7 Bb2 O-O",
    "g3? Nf6 Bg2 d5 d3 c6 Nd2 e5 e4 Bd6 Ngf3 O-O O-O",
    "g3? d5 Nf3 Nf6 Bg2 e6 O-O Be7 d3 O-O Nbd2 c5 e4 Nc6",
    "g3? e5 Bg2 d5 d3 Nf6 Nf3 Nc6 O-O Be7 c4 O-O cxd5 Nxd5 Nc3 Be6",

    // Dutch
    "d4 f5 g3 Nf6 Bg2 g6 Nf3 Bg7 O-O O-O c4 d6 Nc3 Qe8 d5 Na6",
    "c4 f5? d4 Nf6 Nc3 g6 Nf3 Bg7 e3 O-O Be2 d6 O-O Nc6",
    "d4 f5? Nf3 Nf6 g3 g6 Bg2 Bg7 c4 O-O Nc3 d6 O-O Nc6 d5 Ne5",

    // Less usual openings
    "Nc3? d5 e4 d4 Nce2 e5 Ng3 Be6 Nf3 Nd7 c3 c5 Bb5 Bd6 O-O a6",
    "Nc3? d5 d4 Nf6 Bg5 Nbd7 Nf3 h6 Bh4 c6 e3 e6 Bd3 Be7 O-O O-O",
    "Nc3? d5 e4 c6 Nf3 Bg4 h3 Bh5 d4",
    "Nc3? e5 e4 Nf6 Bc4 Nc6 d3 Bb4 Bg5 h6 Bxf6 Bxc3 bxc3 Qxf6 Ne2 d6",
    "c4 e5 Nc3 Nf6 Nf3 Nc6 g3 d5 cxd5 Nxd5 Bg2 Nb6 O-O Be7 d3 O-O a3 Be6 b4",
    "c4 e5 Nc3 Nf6 g3 d5 cxd5 Nxd5 Bg2 Nb6 Nf3 Nc6 O-O Be7 a3 O-O b4 Be6 d3",
    "f4 d5 Nf3 Nf6 e3 g6 Be2 Bg7 O-O O-O d3 c5 Qe1 Nc6 Nc3 Re8",
    "f4 d5 Nf3 g6 g3 Bg7 Bg2 Nf6 O-O O-O d3 c5 Nc3 d4 Ne4",
    "f4? d5 Nf3 g6 e3 Bg7 Be2 Nf6",
    "f4? d5 e3 Nf6 Nf3 Bg4 Be2 Nbd7 Ne5",
    "b3? e5 Bb2 Nc6 e3 d5 Bb5 Bd6 Nf3 Qe7 c4 Nf6",
    "e4 g6? d4 Bg7 Nc3 d6 f4 Nf6 Nf3 O-O Bd3 Nc6 O-O e5",
    "e4 d6 d4 Nf6 Nc3 g6 Nf3 Bg7",
    "e4 d6? d4 g6 Nc3 Bg7 Nf3 Nf6",
    "d4 d6? e4 Nf6 Nc3 g6 Be3 Bg7 Qd2 c6",
    "e4 d6? d4 g6 c4 Bg7",
    "d4 g6 e4 Bg7 Nf3 d6 Nc3 Nf6 Be2 O-O O-O c6 h3 Qc7 Bf4 Nbd7 e5 dxe5 Nxe5 Nxe5 Bxe5 Qb6",
    "d4 g6 c4 Bg7 Nc3 d6 e4 Nf6 Nf3 O-O Be2 e5 O-O",
    "d4 g6? e4 Bg7 c4 d6 Nc3 Nc6 Be3 e5 d5 Nce7",
    "c4 Nf6 g3 g6 Bg2 Bg7 Nc3 O-O e4 d6 Nge2 c5 O-O Nc6 d3 a6 h3 Rb8 a4",
    "c4 Nf6 g3? e6 d4 d5 Nf3 dxc4 Bg2 a6 O-O Nc6",
    "b3? e6 Bb2 Nf6 e3 c5 Nf3 Be7 d4 O-O Bd3 d5 O-O Nc6",

    NULL
};
