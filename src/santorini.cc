#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <tuple>
#include <vector>

// The board is a 5x5 square of cells.
constexpr int BOARD_WIDTH = 5;

// Each player has 2 pawns.
constexpr int PAWN_COUNT = 2;

// Maximum height for each cell.
constexpr int MAX_HEIGHT = 4;

// Each pawn could have 8 places to move and then 8 places to build;
constexpr int MAX_LEGAL_MOVES = PAWN_COUNT * 8 * 8;

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

    bool operator==(const Move &that) const {
        return that.pawn == pawn &&
            that.start == start &&
            that.end == end &&
            that.build == build;
    }

    bool operator!=(const Move &that) const {
        return !(*this == that);
    }
};

using Moves = MaxlenVector<Move, MAX_LEGAL_MOVES>;

// Valid end positions from making a kings move.
//
// From a central cell, there will be eight valid end positions, but from edges
// and corners there will be fewer.
using KingMoveEnds = MaxlenVector<Position, 8>;

struct MonteCarloTreeSearchNode {
    Board board;
    int wins;
    int visits;
    int visited_child_count;
    int value; // 1 if known win, -1 if known loss, 0 otherwise.
    MonteCarloTreeSearchNode *parent;
    std::vector<MonteCarloTreeSearchNode> children;

    MonteCarloTreeSearchNode(
            Board board, MonteCarloTreeSearchNode *parent = nullptr) :
        board(board),
        wins(0),
        visits(0),
        visited_child_count(0),
        value(0),
        parent(parent)
    {}
};

// Pretty-prints the board to stdout.
void print_board(const Board &board) {
    char chars[2*BOARD_WIDTH + 1][5*BOARD_WIDTH + 1];
    // Create cell divisions.
    for (int y = 0; y < BOARD_WIDTH + 1; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            chars[2*y][5*x] = '+';
            chars[2*y][5*x + 1] = '-';
            chars[2*y][5*x + 2] = '-';
            chars[2*y][5*x + 3] = '-';
            chars[2*y][5*x + 4] = '-';
        }
        chars[2*y][5*BOARD_WIDTH] = '+';
    }
    for (int y = 0; y < BOARD_WIDTH; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            chars[2*y + 1][5*x] = '|';
            chars[2*y + 1][5*x + 1] = ' ';
            chars[2*y + 1][5*x + 2] = ' ';
            chars[2*y + 1][5*x + 3] = ' ';
            chars[2*y + 1][5*x + 4] = ' ';
        }
        chars[2*y + 1][5*BOARD_WIDTH] = '|';
    }

    // Put in player positions.
    for (int player = 0; player < 2; ++player) {
        char c = player ? 'b' : 'a';
        for (int pawn = 0; pawn < PAWN_COUNT; ++pawn) {
            Position p = board.position[player][pawn];
            chars[2*p.y + 1][5*p.x + 2] = ':';
            chars[2*p.y + 1][5*p.x + 3] = c;
            chars[2*p.y + 1][5*p.x + 4] = '0' + pawn;
        }
    }

    // Put in the heights.
    for (int y = 0; y < BOARD_WIDTH; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            chars[2*y + 1][5*x + 1] = '0' + board.height[x][y];
        }
    }

    // Do the printing.
    std::printf("     ");
    for (int x = 0; x < BOARD_WIDTH; ++x) {
        std::printf("x=%d  ", x);
    }
    std::printf("\n");
    for (int y = 0; y < 2*BOARD_WIDTH + 1; ++y) {
        if (y % 2) {
            std::printf("y=%d ", (y - 1) / 2);
        }
        else {
            std::printf("    ");
        }
        for (int x = 0; x < 5*BOARD_WIDTH + 1; ++x) {
            std::putchar(chars[y][x]);
        }
        std::putchar('\n');
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
//
// Checks that the height for the target is less than the max height and that
// there is no pawn on the target.
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
        int start_height = board.height[start.x][start.y];
        // Prototype move for starting at this position with this pawn.
        Move move;
        move.pawn = pawn;
        move.start = start;
        // All valid moves will be able to build on the cell they just vacated.
        move.build = start;
        for (Position end : get_king_move_ends(start)) {
            int end_height = board.height[end.x][end.y];
            if (is_legal_target(board, end) &&
                    (start_height + 1 >= end_height)) {
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

// Plays the game with the board as the starting state and the scratch space.
//
// Next is the index of the player who has the next move from the given
// starting state (either 0 or 1).
//
// Returns the index of the winning player (either 0 or 1).
template <bool verbose=false, typename P0, typename P1>
int play_game(Board *board, int next, P0 *p0, P1 *p1) {
    for (int i = 0;; ++i) {
        if (verbose) { std::printf("Move %2d\n", i); }
        Moves moves = get_legal_moves(*board, next);
        if (!moves.size()) {
            // Next player loses because they have no legal moves.
            if (verbose) {
                std::printf(
                        "Player %d wins because player %d has no legal moves.\n",
                        1 - next, next);
            }
            return 1 - next;
        }
        int index = next ?
            p1->select_move(*board, moves, next) :
            p0->select_move(*board, moves, next);
        Move move = moves[index];
        if (board->height[move.end.x][move.end.y] == MAX_HEIGHT - 1) {
            // Next player wins because they stepped to the winning height.
            if (verbose) {
                std::printf(
                        "Player %d wins by stepping onto (%d,%d)\n",
                        next, move.end.x, move.end.y);
            }
            return next;
        }
        if (verbose) {
            std::printf(
                    "Player %d moves pawn %d from (%d,%d) to (%d,%d) and builds at (%d,%d)\n",
                    next,
                    move.pawn,
                    move.start.x,
                    move.start.y,
                    move.end.x,
                    move.end.y,
                    move.build.x,
                    move.build.y);
        }
        // Update board due to selected move.
        board->position[next][move.pawn] = move.end;
        ++board->height[move.build.x][move.build.y];
        // Go to next player.
        next = 1 - next;
    }
}

// Simple AI that looks ahead to the opponent's next move.
class SimplePlayer {
    public:
        SimplePlayer(std::mt19937 *rng) : rng_(rng) {}

        int select_move(const Board &board, const Moves &moves, int player) {
            int obvious = get_obvious_move(board, moves, player);
            if (obvious >= 0) {
                return obvious;
            }

            auto blunders = get_blunders(board, moves, player);
            if (blunders.size() == moves.size()) {
                // All the moves are losers, so just pick the first one,
                // you loser.
                return 0;
            }

            // Choose at random from among the non-losing moves.
            std::uniform_int_distribution<int> uni(
                    0, moves.size() - blunders.size() - 1);
            int rand = uni(*rng_);
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
                        "Couldn't find %d winners from "
                        "%d moves minus %d losers\n",
                        rand, moves.size(), blunders.size());
                std::exit(EXIT_FAILURE);
            }

            return base + skip;
        }

    protected:
        std::mt19937 *rng_;

        int get_obvious_move(
                const Board &board, const Moves &moves, int player) {
            // First see if any of the moves wins the game.
            // If so, select that move.
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
                            // This move will win the game for the opponent,
                            // so try to build here. We know we can't move here
                            // because we checked that above.
                            for (int i = 0; i < moves.size(); ++i) {
                                if (moves[i].build == end) {
                                    // This stops this particular winning move
                                    // for the opponent, but the opponent may
                                    // have other winning moves. In any case,
                                    // we can only stop one so do not bother
                                    // checking for others.
                                    return i;
                                }
                            }
                        }
                    }
                }
            }
            // No obvious move found, return -1.
            return -1;
        }

        MaxlenVector<int, MAX_LEGAL_MOVES> get_blunders(
                const Board &board, const Moves &moves, int player) {
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
                            // Opponent is close enough, vertically and
                            // horizontally, so do not choose this move.
                            blunders.push_back(i);
                            break;
                        }
                    }
                }
            }
            return blunders;
        }
};

class SimpleRolloutPlayer : public SimplePlayer {
    public:
        SimpleRolloutPlayer(int rollout_count, std::mt19937 *rng)
            : SimplePlayer(rng), rollout_count_(rollout_count) {}

        int select_move(const Board &board, const Moves &moves, int player) {
            int obvious = get_obvious_move(board, moves, player);
            if (obvious >= 0) {
                return obvious;
            }

            auto blunders = get_blunders(board, moves, player);
            if (blunders.size() == moves.size()) {
                // All the moves are losers, so just pick the first one,
                // you loser.
                return 0;
            }
            SimplePlayer player_object(rng_);
            int best_index = -1;
            double best_score = std::numeric_limits<double>::lowest();
            int blunder_index = 0;
            for (int i = 0; i < moves.size(); ++i) {
                if (blunders.size() && i == blunders[blunder_index]) {
                    ++blunder_index;
                    continue;
                }
                // Roll this move out several times using SimplePlayer for both
                // players.
                Board moved_board = board;
                Move move = moves[i];
                moved_board.position[player][move.pawn] = move.end;
                ++moved_board.height[move.build.x][move.build.y];
                double payoff = 0;
                for (int trial = 0; trial < rollout_count_; ++trial) {
                    Board rollout_board = moved_board;
                    int winner = play_game(
                            &rollout_board,
                            1 - player,
                            &player_object,
                            &player_object);
                    payoff += (winner == player) ? 1 : -1;
                }
                double average_payoff = payoff / rollout_count_;
                if (average_payoff > best_score) {
                    best_score = average_payoff;
                    best_index = i;
                }
            }
            return best_index;
        }

    private:
        int rollout_count_;
};

class HumanPlayer {
    public:
        int select_move(const Board &board, const Moves &moves, int player) {
            print_board(board);
            std::string player_label = player ? "b" : "a";
            std::string expected_pawns[] =
            {player_label + "0", player_label + "1"};
            int pawn;
            Position end;
            Position build;
            while (true) {
                std::string input;

                // Get pawn.
                pawn = 0;
                while (true) {
                    std::cout << "Which pawn will you move ("
                        << expected_pawns[0]
                        << " or " << expected_pawns[1] << ")\n> ";
                    std::cin >> input;
                    if (input != expected_pawns[0] &&
                            input != expected_pawns[1]) {
                        std::cout << "Invalid pawn selection, please enter "
                            << expected_pawns[0] << " or "
                            << expected_pawns[1] << ".\n";
                        continue;
                    }
                    if (input == expected_pawns[1]) {
                        pawn = 1;
                    }
                    break;
                }
                bool valid_pawn = false;
                for (int i = 0; i < moves.size(); ++i) {
                    if (moves[i].pawn == pawn) {
                        valid_pawn = true;
                        break;
                    }
                }
                if (!valid_pawn) {
                    std::cout << "Pawn " << expected_pawns[pawn]
                        << " has no valid moves, "
                        << "please select the other pawn.\n";
                    continue;
                }

                // Get end.
                Position start = board.position[player][pawn];
                while (true) {
                    std::cout << "Which direction will you move\n> ";
                    char direction;
                    std::cin >> direction;
                    end = get_new_position(start, direction);
                    if (end.x < 0) {
                        std::cout << "Invalid move direction\n";
                        continue;
                    }
                    bool valid_move = false;
                    for (int i = 0; i < moves.size(); ++i) {
                        if (moves[i].pawn == pawn && moves[i].end == end) {
                            valid_move = true;
                            break;
                        }
                    }
                    if (!valid_move) {
                        std::cout << "That move is not legal for that pawn. "
                            "Try again.\n";
                        continue;
                    }
                    break;
                }

                // Get build.
                while (true) {
                    std::cout << "Which direction will you build\n> ";
                    char direction;
                    std::cin >> direction;
                    build = get_new_position(end, direction);
                    if (build.x < 0) {
                        std::cout << "Invalid build direction\n";
                        continue;
                    }
                    for (int i = 0; i < moves.size(); ++i) {
                        if (moves[i].pawn == pawn &&
                                moves[i].end == end &&
                                moves[i].build == build) {
                            return i;
                        }
                    }
                    std::cout
                        << "That build is not legal for that pawn "
                        << "and that move. Try again.\n";
                }
            }
        }

    private:
        Position get_new_position(const Position &start, char entry) {
            Position end;
            switch (entry) {
                case '1': // fall-through.
                case 'z':
                    end.x = start.x - 1;
                    end.y = start.y + 1;
                    break;
                case '2': // fall-through.
                case 'x':
                    end.x = start.x;
                    end.y = start.y + 1;
                    break;
                case '3': // fall-through.
                case 'c':
                    end.x = start.x + 1;
                    end.y = start.y + 1;
                    break;
                case '4': // fall-through.
                case 'a':
                    end.x = start.x - 1;
                    end.y = start.y;
                    break;
                case '6': // fall-through.
                case 'd':
                    end.x = start.x + 1;
                    end.y = start.y;
                    break;
                case '7': // fall-through.
                case 'q':
                    end.x = start.x - 1;
                    end.y = start.y - 1;
                    break;
                case '8': // fall-through.
                case 'w':
                    end.x = start.x;
                    end.y = start.y - 1;
                    break;
                case '9': // fall-through.
                case 'e':
                    end.x = start.x + 1;
                    end.y = start.y - 1;
                    break;
                default:
                    end.x = -1;
                    end.y = -1;
            }
            return end;
        }
};

class MonteCarloTreeSearchPlayer: public SimplePlayer {
    public:
        MonteCarloTreeSearchPlayer(
                std::chrono::seconds time_limit, std::mt19937 *rng) :
            SimplePlayer(rng),
            time_limit_(time_limit) {}

        int select_move(const Board &board, const Moves &moves, int player) {
            int obvious = get_obvious_move(board, moves, player);
            if (obvious >= 0) {
                return obvious;
            }

            auto blunders = get_blunders(board, moves, player);
            if (blunders.size() == moves.size()) {
                // All the moves are losers, so just pick the first one,
                // you loser.
                return 0;
            }

            // Build base nodes for tree search.
            std::vector<std::pair<int, MonteCarloTreeSearchNode>> nodes;
            int blunder_index = blunders.size() ? 0 : -1;
            for (int i = 0; i < moves.size(); ++i) {
                if (blunder_index >= 0 &&
                        blunder_index < blunders.size() &&
                        blunders[blunder_index] == i) {
                    ++blunder_index;
                    continue;
                }
                nodes.emplace_back(i, board);
            }

            int unvisited_node_count = nodes.size();

            std::chrono::system_clock clock;
            const auto start_time = clock.now();
            int rollout_count = 0;
            while (true) {
                // Iterate over rollouts until time expires.
                if (clock.now() - start_time > time_limit_) {
                    break;
                }
                std::printf("rollout_count = %d\n", rollout_count);
                ++rollout_count;

                // Pick the node to start with.
                MonteCarloTreeSearchNode *node = nullptr;
                if (unvisited_node_count) {
                    --unvisited_node_count;
                    std::uniform_int_distribution<int> uni(
                            0, unvisited_node_count);
                    int rand = uni(*rng_);
                    for (int i = 0; i < nodes.size(); ++i) {
                        if (!nodes[i].second.visits) {
                            if (rand) {
                                --rand;
                            } else {
                                node = &nodes[i].second;
                                Move move = moves[nodes[i].first];
                                node->board.position[player][move.pawn] =
                                    move.end;
                                ++node->
                                    board.height[move.build.x][move.build.y];
                                break;
                            }
                        }
                    }
                } else {
                    double best_score = -1;
                    int total_visits = 0;
                    for (const auto &n : nodes) {
                        total_visits += n.second.visits;
                    }
                    for (auto &n : nodes) {
                        double score =
                            static_cast<double>(n.second.wins)/n.second.visits +
                            std::sqrt(2.0*std::log(total_visits)/n.second.visits);
                        if (score > best_score) {
                            best_score = score;
                            node = &n.second;
                        }
                    }
                }

                // Search down the chosen node.
                int current_player = player;
                while (true) {
                    current_player = 1 - current_player;
                    if (!node->visits) {
                        // Fill in the children of this node.
                        Moves moves =
                            get_legal_moves(node->board, current_player);
                        if (!moves.size()) {
                            node->value = -1;
                            for (; node; node = node->parent) {
                                ++node->visits;
                                if (current_player != player) {
                                    ++node->wins;
                                }
                            }
                            break;
                        }
                        int obvious =
                            get_obvious_move(node->board, moves, current_player);
                        if (obvious >= 0) {
                            Move move = moves[obvious];
                            // This is the only child.
                            if (node->board.height[move.end.x][move.end.y] ==
                                    MAX_HEIGHT - 1) {
                                // This is a win for current_player.
                                node->value = 1;
                                for (; node; node = node->parent) {
                                    ++node->visits;
                                    if (current_player == player) {
                                        ++node->wins;
                                    }
                                }
                                break;
                            } else {
                                // It's not a win, but it is obvious, so it's
                                // the only child we will consider.
                                node->children.emplace_back(node->board, node);
                                node->children.back()
                                    .board.position[current_player][move.pawn] =
                                    move.end;
                                ++node->children.back()
                                    .board.height[move.build.x][move.build.y];
                            }
                        } else {
                            auto blunders = get_blunders(board, moves, player);
                            if (blunders.size() == moves.size()) {
                                // All the moves are losers.
                                //
                                // This is a win for the other player.
                                node->value = -1;
                                for (; node; node = node->parent) {
                                    ++node->visits;
                                    if (current_player != player) {
                                        ++node->wins;
                                    }
                                }
                                break;
                            }
                            int blunder_index = blunders.size() ? 0 : -1;
                            for (int i = 0; i < moves.size(); ++i) {
                                if (blunder_index >= 0 &&
                                        blunder_index < blunders.size() &&
                                        blunders[blunder_index] == i) {
                                    ++blunder_index;
                                    continue;
                                }
                                Move move = moves[i];
                                node->children.emplace_back(node->board, node);
                                node->
                                    children.back()
                                    .board.position[current_player][move.pawn] =
                                    move.end;
                                ++node->
                                    children.back()
                                    .board.height[move.build.x][move.build.y];
                            }
                        }
                    }

                    if (node->value) {
                        // Value already known for this node.
                        // Do not search any deeper on this path.
                        for (; node; node = node->parent) {
                            ++node->visits;
                            bool is_win =
                                (node->value == 1 &&
                                 current_player == player) ||
                                (node->value == -1 &&
                                 current_player != player);
                            if (is_win) {
                                ++node->wins;
                            }
                        }
                        break;
                    }

                    // Select the next node for the rollout.
                    int unvisited_node_count =
                        node->children.size() - node->visited_child_count;
                    if (unvisited_node_count) {
                        ++node->visited_child_count;
                        std::uniform_int_distribution<int> uni(
                                0, unvisited_node_count - 1);
                        int rand = uni(*rng_);
                        for (int i = 0; i < node->children.size(); ++i) {
                            if (rand) {
                                --rand;
                            } else {
                                node = &node->children[i];
                                break;
                            }
                        }
                    } else {
                        double best_score = 0;
                        int total_visits = 0;
                        for (const auto &n : node->children) {
                            total_visits += n.visits;
                        }
                        for (auto &n : node->children) {
                            double score =
                                static_cast<double>(n.wins) / n.visits +
                                std::sqrt(2.0*std::log(total_visits)/n.visits);
                            if (score > best_score) {
                                best_score = score;
                                node = &n;
                            }
                        }
                    }
                }
            }
            // Pick the index with the most visits.
            int best_visit_count = 0;
            int best_index = 0;
            for (const auto &n : nodes) {
                if (n.second.visits > best_visit_count) {
                    best_visit_count = n.second.visits;
                    best_index = n.first;
                }
            }
            return best_index;
        }

    private:
        std::chrono::seconds time_limit_;
};

int main(int argc, char *argv[]) {
    std::random_device random_device;
    unsigned int seed = argc > 1 ? std::stoul(argv[1]) : random_device();
    std::printf("Seed = %u\n", seed);
    std::mt19937 rng(seed);

    int counts[2] = {0, 0};
    MonteCarloTreeSearchPlayer player0(std::chrono::seconds(1), &rng);
    HumanPlayer player1;
    for (int trial = 0; trial < 1; ++trial) {
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
        int winner = play_game<false>(&board, 0, &player0, &player1);
        ++counts[winner];
        std::printf("\n");
        print_board(board);
        std::printf("Trial %3d won by player %d.\n", trial, winner);
    }
    std::printf(
            "\nPlayer 0 wins %d times, player 1 wins %d times.\n",
            counts[0],
            counts[1]);
}
