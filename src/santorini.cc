#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>
#include <tuple>

// The board is a 5x5 square of cells.
constexpr int BOARD_WIDTH = 5;

// Each player has 2 pawns.
constexpr int PAWN_COUNT = 2;

// Maximum height for each cell.
constexpr int MAX_HEIGHT = 4;

// Each pawn could have 8 places to move and then 8 places to build;
constexpr int MAX_LEGAL_MOVES = PAWN_COUNT * 8 * 8;

// Random number generator stuff shared by everyone.
std::random_device random_device;
std::mt19937 rng(random_device());

// A vector with a maximum length of N.
//
// It doesn't do checks for you though, so it's up to you not to overfill it.
template <typename T, int N>
class MaxlenVector {
    public:
        MaxlenVector() : length_(0) {}

        void push_back(const T &val) {
            values_[length_] = val;
            ++length_;
        }

        T &operator[](int n) {
            return values_[n];
        }

        const T &operator[](int n) const {
            return values_[n];
        }

        T *begin() {
            return values_;
        }

        T *end() {
            return values_ + length_;
        }

        const T *begin() const {
            return values_;
        }

        const T *end() const {
            return values_ + length_;
        }

        int size() const {
            return length_;
        }

    private:
        int length_;
        T values_[N];
};

struct Position {
    int x;
    int y;

    Position() {}
    Position(int x, int y) : x(x), y(y) {}

    bool operator==(const Position &that) const {
        return that.x == x && that.y == y;
    }

    bool operator!=(const Position &that) const {
        return !(*this == that);
    }
};

struct Board {
    // First index is player, second index is pawn number.
    Position position[2][PAWN_COUNT];

    // Each height is an integer from 0 to MAX_HEIGHT, inclusive.
    int height[BOARD_WIDTH][BOARD_WIDTH];
};

struct Move {
    int pawn;
    Position start;
    Position end;
    Position build;
};

using Moves = MaxlenVector<Move, MAX_LEGAL_MOVES>;

// Valid end positions from making a kings move.
//
// From a central cell, there will be eight valid end positions, but from edges
// and corners there will be fewer.
using KingMoveEnds = MaxlenVector<Position, 8>;

// Pretty-prints the board to stdout.
void print_board(const Board &board) {
    std::printf("Positions =\n");
    for (int player = 0; player < 2; ++player) {
        std::printf("  Player = %d\n", player);
        for (int pawn = 0; pawn < PAWN_COUNT; ++pawn) {
            Position p = board.position[player][pawn];
            std::printf("    (%d,%d)\n", p.x, p.y);
        }
    }
    std::printf("\nHeights =\n");
    for (int y = 0; y < BOARD_WIDTH; ++y) {
        std::printf("  ");
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            std::printf("%d", board.height[x][y]);
        }
        std::printf("\n");
    }
}

// Gets the ends of legal king's moves starting at the given position.
KingMoveEnds get_king_move_ends(Position start) {
    KingMoveEnds ends;
    for (int dx = -1; dx < 2; ++dx) {
        for (int dy = -1; dy < 2; ++dy) {
            if (!dx && !dy) {
                continue;
            }
            Position end(start.x + dx, start.y + dy);
            if (end.x < 0 || end.x >= BOARD_WIDTH ||
                    end.y < 0 || end.y >= BOARD_WIDTH) {
                continue;
            }
            ends.push_back(end);
        }
    }
    return ends;
}

// Returns whether the target position can be moved-to/built-upon.
bool is_legal_target(const Board &board, Position target) {
    bool legal = board.height[target.x][target.y] < MAX_HEIGHT;
    for (int player = 0; player < 2; ++player) {
        for (int pawn = 0; pawn < PAWN_COUNT; ++pawn) {
            legal = legal && (target != board.position[player][pawn]);
        }
    }
    return legal;
}

// Gets all the legal moves for the given player.
Moves get_legal_moves(const Board &board, int player) {
    Moves moves;
    // Add all moves that build on the just-vacated cell.
    for (int pawn = 0; pawn < PAWN_COUNT; ++pawn) {
        Position start = board.position[player][pawn];
        // Prototype move for starting at this position with this pawn.
        Move move;
        move.pawn = pawn;
        move.start = start;
        // All valid moves will be able to build on the cell they just vacated.
        move.build = start;
        for (Position end : get_king_move_ends(start)) {
            if (is_legal_target(board, end)) {
                move.end = end;
                moves.push_back(move);
            }
        }
    }
    // Add all the other builds for the valid moves.
    int length = moves.size();
    for (int i = 0; i < length; ++i) {
        Move move = moves[i];
        for (Position build : get_king_move_ends(move.end)) {
            if (is_legal_target(board, build)) {
                move.build = build;
                moves.push_back(move);
            }
        }
    }
    return moves;
}

// Return the index into Moves for the selected move.
//
// Simple AI that looks ahead to the opponent's next move.
int select_move(const Board &board, const Moves &moves, int player) {
    // First see if any of the moves wins the game. If so, select that move.
    for (int i = 0; i < moves.size(); ++i) {
        Move move = moves[i];
        if (board.height[move.end.x][move.end.y] == MAX_HEIGHT - 1) {
            return i;
        }
    }

    // Check if any move stops the other player from winning.
    for (int pawn = 0; pawn < PAWN_COUNT; ++pawn) {
        Position them = board.position[1 - player][pawn];
        if (board.height[them.x][them.y] == MAX_HEIGHT - 2) {
            // This pawn is at the right height to win on the next move.
            for (Position end : get_king_move_ends(them)) {
                if (board.height[end.x][end.y] == MAX_HEIGHT - 1) {
                    // This move will win the game for the opponent, so try to
                    // build here. We know we can't move here because we
                    // checked that above.
                    for (int i = 0; i < moves.size(); ++i) {
                        if (moves[i].build == end) {
                            // This stops this particular winning move for the
                            // opponent, but the opponent may have other
                            // winning moves. In any case, we can only stop one
                            // so do not bother checking for others.
                            return i;
                        }
                    }
                }
            }
        }
    }

    // Check if any move gives the other player an immediate winning move.
    MaxlenVector<int, MAX_LEGAL_MOVES> blunders;
    for (int i = 0; i < moves.size(); ++i) {
        Position build = moves[i].build;
        if (board.height[build.x][build.y] == MAX_HEIGHT - 2) {
            // This makes a tower of winning height,
            // make sure no opponent is nearby.
            for (int pawn = 0; pawn < 2; ++pawn) {
                Position them = board.position[1 - player][pawn];
                int dx = them.x - build.x;
                int dy = them.y - build.y;
                int d2 = dx*dx + dy*dy;  // Squared horizontal distance.
                int height = board.height[them.x][them.y];
                if (d2 <= 2 && height == MAX_HEIGHT - 2) {
                    // Opponent is close enough, vertically and horizontally,
                    // so do not choose this move.
                    blunders.push_back(i);
                    break;
                }
            }
        }
    }

    if (blunders.size() == moves.size()) {
        // All the moves are losers, so just pick the first one, you loser.
        return 0;
    }

    // Choose at random from among the non-losing moves.
    std::uniform_int_distribution<int> uni(
            0, moves.size() - blunders.size() - 1);
    int rand = uni(rng);
    int skip = rand;
    int base = 0;
    for (int blunder : blunders) {
        if (base + skip < blunder) {
            return base + skip;
        }
        skip -= blunder - base;
        base = blunder + 1;
    }

    if (base + skip >= moves.size()) {
        // It shouldn't be possible to get here.
        std::fprintf(
                stderr,
                "Couldn't find %d winners from %d moves minus %d losers\n",
                rand, moves.size(), blunders.size());
        std::exit(EXIT_FAILURE);
    }

    return base + skip;
}

int main() {
    Board board;
    std::memset(&board, 0, sizeof(board));
    int cell_indices[BOARD_WIDTH * BOARD_WIDTH];
    for (int i = 0; i < BOARD_WIDTH * BOARD_WIDTH; ++i) {
        cell_indices[i] = i;
    }
    std::shuffle(cell_indices, cell_indices + BOARD_WIDTH * BOARD_WIDTH, rng);
    for (int player = 0; player < 2; ++player) {
        for (int pawn = 0; pawn < PAWN_COUNT; ++pawn) {
            int index = cell_indices[player * PAWN_COUNT + pawn];
            board.position[player][pawn] =
                Position(index % BOARD_WIDTH, index / BOARD_WIDTH);
        }
    }

    print_board(board);
    std::printf("\n");

    for (int i = 0;; ++i) {
        // Move for player zero.
        std::printf("Move %2d\n", 2*i + 1);
        Moves moves = get_legal_moves(board, 0);
        if (!moves.size()) {
            std::printf("\nPlayer 1 wins because player 0 has no moves.\n");
            break;
        }
        int index = select_move(board, moves, 0);
        Move move = moves[index];
        if (board.height[move.end.x][move.end.y] == MAX_HEIGHT - 1) {
            std::printf(
                    "\nPlayer 0 wins by stepping onto (%d,%d)\n",
                    move.end.x, move.end.y);
            break;
        }
        std::printf(
                "Player 0 moves pawn %d from (%d,%d) to (%d,%d) "
                "and builds at (%d,%d)\n",
                move.pawn, move.start.x, move.start.y,
                move.end.x, move.end.y, move.build.x, move.build.y);
        board.position[0][move.pawn] = move.end;
        ++board.height[move.build.x][move.build.y];

        // Move for player one.
        std::printf("Move %2d\n", 2*i + 2);
        moves = get_legal_moves(board, 1);
        if (!moves.size()) {
            std::printf("\nPlayer 0 wins because player 1 has no moves.\n");
            break;
        }
        index = select_move(board, moves, 1);
        move = moves[index];
        if (board.height[move.end.x][move.end.y] == MAX_HEIGHT - 1) {
            std::printf(
                    "\nPlayer 1 wins by stepping onto (%d,%d)\n",
                    move.end.x, move.end.y);
            break;
        }
        std::printf(
                "Player 1 moves pawn %d from (%d,%d) to (%d,%d) "
                "and builds at (%d,%d)\n",
                move.pawn, move.start.x, move.start.y,
                move.end.x, move.end.y, move.build.x, move.build.y);
        board.position[1][move.pawn] = move.end;
        ++board.height[move.build.x][move.build.y];
    }

    std::printf("\n");
    print_board(board);
}
