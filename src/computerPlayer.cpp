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
 * computerPlayer.cpp
 *
 *  Created on: Feb 25, 2012
 *      Author: petero
 */

#include "computerPlayer.hpp"
#include "textio.hpp"

#include <iostream>

std::string ComputerPlayer::engineName;


static StaticInitializer<ComputerPlayer> cpInit;

void
ComputerPlayer::staticInitialize() {
    std::string name = "Texel 1.03";
    if (sizeof(char*) == 4)
        name += " 32-bit";
    if (sizeof(char*) == 8)
        name += " 64-bit";
    engineName = name;
}

ComputerPlayer::ComputerPlayer()
    : tt(15), pd(tt),
      book(verbose)
{
    Parameters::instance();
    et = Evaluate::getEvalHashTables();
    minTimeMillis = 10000;
    maxTimeMillis = 10000;
    maxDepth = 100;
    maxNodes = -1;
    verbose = true;
    bookEnabled = true;
    currentSearch = NULL;
}

std::string
ComputerPlayer::getCommand(const Position& posIn, bool drawOffer, const std::vector<Position>& history) {
    // Create a search object
    std::vector<U64> posHashList(200 + history.size());
    int posHashListSize = 0;
    for (size_t i = 0; i < history.size(); i++)
        posHashList[posHashListSize++] = history[i].zobristHash();
    tt.nextGeneration();
    Position pos(posIn);
    KillerTable kt;
    History ht;
    Search::SearchTables st(tt, kt, ht, *et);
    TreeLogger treeLog;
    Search sc(pos, posHashList, posHashListSize, st, pd, nullptr, treeLog);

    // Determine all legal moves
    MoveGen::MoveList moves;
    MoveGen::pseudoLegalMoves(pos, moves);
    MoveGen::removeIllegal(pos, moves);
    sc.scoreMoveList(moves, 0);

    // Test for "game over"
    if (moves.size == 0) {
        // Switch sides so that the human can decide what to do next.
        return "swap";
    }

    if (bookEnabled) {
        Move bookMove;
        book.getBookMove(pos, bookMove);
        if (!bookMove.isEmpty()) {
            std::cout << "Book moves: " << book.getAllBookMoves(pos) << std::endl;
            return TextIO::moveToString(pos, bookMove, false);
        }
    }

    // Find best move using iterative deepening
    currentSearch = &sc;
    sc.setListener(listener);
    Move bestM;
    if ((moves.size == 1) && (canClaimDraw(pos, posHashList, posHashListSize, moves[0]) == "")) {
        bestM = moves[0];
        bestM.setScore(0);
    } else {
        sc.timeLimit(minTimeMillis, maxTimeMillis);
        bestM = sc.iterativeDeepening(moves, maxDepth, maxNodes, verbose);
    }
    currentSearch = NULL;
    //        tt.printStats();
    std::string strMove = TextIO::moveToString(pos, bestM, false);

    // Claim draw if appropriate
    if (bestM.score() <= 0) {
        std::string drawClaim = canClaimDraw(pos, posHashList, posHashListSize, bestM);
        if (drawClaim != "")
            strMove = drawClaim;
    }
    return strMove;
}

std::string
ComputerPlayer::canClaimDraw(Position& pos, std::vector<U64>& posHashList,
                             int posHashListSize, const Move& move) {
    std::string drawStr;
    if (Search::canClaimDraw50(pos)) {
        drawStr = "draw 50";
    } else if (Search::canClaimDrawRep(pos, posHashList, posHashListSize, posHashListSize)) {
        drawStr = "draw rep";
    } else {
        std::string strMove = TextIO::moveToString(pos, move, false);
        posHashList[posHashListSize++] = pos.zobristHash();
        UndoInfo ui;
        pos.makeMove(move, ui);
        if (Search::canClaimDraw50(pos)) {
            drawStr = "draw 50 " + strMove;
        } else if (Search::canClaimDrawRep(pos, posHashList, posHashListSize, posHashListSize)) {
            drawStr = "draw rep " + strMove;
        }
        pos.unMakeMove(move, ui);
    }
    return drawStr;
}

void
ComputerPlayer::timeLimit(int minTimeLimit, int maxTimeLimit) {
    minTimeMillis = minTimeLimit;
    maxTimeMillis = maxTimeLimit;
    if (currentSearch != NULL)
        currentSearch->timeLimit(minTimeLimit, maxTimeLimit);
}

std::pair<Move, std::string>
ComputerPlayer::searchPosition(Position& pos, int maxTimeMillis) {
    // Create a search object
    std::vector<U64> posHashList(200);
    tt.nextGeneration();
    KillerTable kt;
    History ht;
    Search::SearchTables st(tt, kt, ht, *et);
    TreeLogger treeLog;
    Search sc(pos, posHashList, 0, st, pd, nullptr, treeLog);

    // Determine all legal moves
    MoveGen::MoveList moves;
    MoveGen::pseudoLegalMoves(pos, moves);
    MoveGen::removeIllegal(pos, moves);
    sc.scoreMoveList(moves, 0);

    // Find best move using iterative deepening
    sc.timeLimit(maxTimeMillis, maxTimeMillis);
    Move bestM = sc.iterativeDeepening(moves, -1, -1, false);

    // Extract PV
    std::string PV = TextIO::moveToString(pos, bestM, false) + " ";
    UndoInfo ui;
    pos.makeMove(bestM, ui);
    PV += tt.extractPV(pos);
    pos.unMakeMove(bestM, ui);

//    tt.printStats();

    // Return best move and PV
    return std::pair<Move, std::string>(bestM, PV);
}
