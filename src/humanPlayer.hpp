/*
    Texel - A UCI chess engine.
    Copyright (C) 2012  Peter Österlund, peterosterlund2@gmail.com

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
 * humanPlayer.hpp
 *
 *  Created on: Feb 25, 2012
 *      Author: petero
 */

#ifndef HUMANPLAYER_HPP_
#define HUMANPLAYER_HPP_

#include "player.hpp"

#include <string>


/**
 * A player that reads input from the keyboard.
 */
class HumanPlayer : public Player {
private:
    std::string lastCmd;

public:
    HumanPlayer() {
    }

    std::string getCommand(const Position& pos, bool drawOffer, const std::vector<Position>& history);

    bool isHumanPlayer() {
        return true;
    }

    void useBook(bool bookOn) {
    }

    void timeLimit(int minTimeLimit, int maxTimeLimit) {
    }

    void clearTT() {
    }
};


#endif /* HUMANPLAYER_HPP_ */
