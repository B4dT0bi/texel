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
 * move.hpp
 *
 *  Created on: Feb 25, 2012
 *      Author: petero
 */

#ifndef MOVE_HPP_
#define MOVE_HPP_

#include <iosfwd>

/** Represents a chess move. */
class Move {
public:
    /** Create empty move object. */
    Move() : from_(0), to_(0), promoteTo_(0), score_(0) { }

    /** Create a move object. */
    Move(int from, int to, int promoteTo, int score = 0);

    /** Copy constructor. */
    Move(const Move& m);

    /** Set move properties. */
    void setMove(int from, int to, int promoteTo, int score);

    void setScore(int score);

    class SortByScore {
    public:
        bool operator()(const Move& m1, const Move& m2) const {
            return m1.score_ > m2.score_;
        }
    };

    int from() const;
    int to() const;
    int promoteTo() const;
    int score() const;

    bool isEmpty() const { return (from_ == 0) && (to_ == 0); }

    /** Note that score is not included in the comparison. */
    bool equals(const Move& other) const;

    bool operator==(const Move& other) const;

    int hashCode() const;

    /** For debugging. */
    std::ostream& operator<<(std::ostream& os);

private:
    /** From square, 0-63. */
    int from_;

    /** To square, 0-63. */
    int to_;

    /** Promotion piece. */
    int promoteTo_;

    /** Score. */
    int score_;
};

inline
Move::Move(int from, int to, int promoteTo, int score) {
    from_ = from;
    to_ = to;
    promoteTo_ = promoteTo;
    score_ = score;
}

inline
Move::Move(const Move& m) {
    from_ = m.from_;
    to_ = m.to_;
    promoteTo_ = m.promoteTo_;
    score_ = m.score_;
}

inline void
Move::setMove(int from, int to, int promoteTo, int score)
{
    from_ = from;
    to_ = to;
    promoteTo_ = promoteTo;
    score_ = score;
}

inline void
Move::setScore(int score) {
    score_ = score;
}

inline int
Move::from() const {
    return from_;
}

inline int
Move::to() const {
    return to_;
}

inline int
Move::promoteTo() const {
    return promoteTo_;
}

inline int
Move::score() const {
    return score_;
}

inline bool
Move::equals(const Move& other) const {
    if (from_ != other.from_)
        return false;
    if (to_ != other.to_)
        return false;
    if (promoteTo_ != other.promoteTo_)
        return false;
    return true;
}

inline bool
Move::operator==(const Move& other) const {
    return (*this).equals(other);
}

inline int
Move::hashCode() const {
    return (from_ * 64 + to_) * 16 + promoteTo_;
}

#endif /* MOVE_HPP_ */
