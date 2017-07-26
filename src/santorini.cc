#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

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
class SmallVec {
    public:
        SmallVec() : length_(0) {}

        void push_back(const T &val) {
            values_[length_] = val;
            ++length_;
        }

        T &operator[](int n) { return values_[n]; }
        const T &operator[](int n) const { return values_[n]; }
        T *begin() { return values_; }
        T *end() { return values_ + length_; }
        const T *begin() const { return values_; }
        const T *end() const { return values_ + length_; }
        int size() const { return length_; }

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

struct State {
    int player;
    Position position[2][PAWN_COUNT];  // First index is player.
    int height[BOARD_WIDTH][BOARD_WIDTH];  // First index is y, second is x.

    bool operator==(const State &that) const {
        return 0 == memcmp(this, &that, sizeof(that));
    }

    bool is_pawn_at(const Position &p) const {
        for (int player = 0; player < 2; ++player) {
            for (int pawn = 0; pawn < PAWN_COUNT; ++pawn) {
                if (position[player][pawn] == p) {
                    return true;
                }
            }
        }
        return false;
    }

    bool is_blocked(const Position &p) const {
        return height[p.y][p.x] == MAX_HEIGHT || is_pawn_at(p);
    }
};

struct Play {
    int pawn;
    Position end;
    Position build;

    bool operator==(const Play &that) const {
        return pawn == that.pawn && end == that.end && build == that.build;
    }
};

using Plays = SmallVec<Play, MAX_LEGAL_MOVES>;

struct Counts {
    double wins;
    double plays;

    Counts(double wins, double plays) : wins(wins), plays(plays) {}
};

using Neighbors = SmallVec<Position, 8>;

namespace std {
    // We need this so we can use State as the key in an unordered_map.
    template <> struct hash<State> {
        size_t operator()(const State &state) const {
            hash<int> int_hash;
            size_t result = int_hash(state.player);
            for (int player = 0; player < 2; ++player) {
                for (int pawn = 0; pawn < PAWN_COUNT; ++pawn) {
                    result =
                        (result << 1) ^
                        int_hash(state.position[player][pawn].x);
                    result =
                        (result << 1) ^
                        int_hash(state.position[player][pawn].y);
                }
            }
            for (int x = 0; x < BOARD_WIDTH; ++x) {
                for (int y = 0; y < BOARD_WIDTH; ++y) {
                    result = (result << 1) ^ int_hash(state.height[y][x]);
                }
            }
            return result;
        }
    };
}

// Finds the neighboring positions of a given position.
Neighbors neighbors(const Position &p) {
    Neighbors results;
    for (int dy = -1; dy < 2; ++dy) {
        for (int dx = -1; dx < 2; ++dx) {
            int x = p.x + dx;
            int y = p.y + dy;
            if ((dx || dy) &&
                    x >= 0 && y >= 0 && x < BOARD_WIDTH && y < BOARD_WIDTH) {
                results.push_back(Position(x, y));
            }
        }
    }
    return results;
}

void print_state(const State &state) {
    cout << "Next player = " << state.player << "\n";
    char screen[11][26];
    for (int y = 0; y < 11; ++y) {
        for (int x = 0; x < 26; ++x) {
            screen[y][x] = ' ';
        }
    }
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 5; ++x) {
            screen[2*y][5*x] = '+';
            screen[2*y][5*x + 1] = '-';
            screen[2*y][5*x + 2] = '-';
            screen[2*y][5*x + 3] = '-';
            screen[2*y][5*x + 4] = '-';
            screen[2*y + 1][5*x] = '|';
            screen[2*y + 1][5*x + 1] = '0' + state.height[y][x];
        }
    }
    for (int player = 0; player < 2; ++player) {
        for (int pawn = 0; pawn < 2; ++pawn) {
            Position p = state.position[player][pawn];
            screen[2*p.y + 1][5*p.x + 2] = ':';
            screen[2*p.y + 1][5*p.x + 3] = player ? 'b' : 'a';
            screen[2*p.y + 1][5*p.x + 4] = '0' + pawn;
        }
    }
    for (int y = 0; y < 11; ++y) {
        for (int x = 0; x < 26; ++x) {
            cout << screen[y][x];
        }
        cout << "\n";
    }
}

State get_start_state() {
    State state;
    memset(&state, 0, sizeof(state));
    state.position[0][0] = Position(0, 0);
    state.position[0][1] = Position(4, 4);
    state.position[1][0] = Position(0, 4);
    state.position[1][1] = Position(4, 0);
    return state;
}

State get_next_state(const State &state, const Play &play) {
    State result(state);
    result.player = 1 - state.player;
    result.position[state.player][play.pawn] = play.end;
    result.height[play.build.y][play.build.x]++;
    return result;
}

Plays get_legal_plays(const State &state) {
    Plays plays;
    for (int pawn = 0; pawn < PAWN_COUNT; ++pawn) {
        Position start = state.position[state.player][pawn];
        for (Position end : neighbors(start)) {
            int height_change =
                state.height[end.y][end.x] - state.height[start.y][start.x];
            if (state.is_blocked(end) || height_change > 1) {
                continue;
            }
            Play play;
            play.pawn = pawn;
            play.end = end;
            play.build = start;
            plays.push_back(play);
            for (Position build : neighbors(end)) {
                if (state.is_blocked(build)) {
                    continue;
                }
                play.build = build;
                plays.push_back(play);
            }
        }
    }
    return plays;
}

int get_winner(const State &state) {
    for (int player = 0; player < 2; ++player) {
        for (int pawn = 0; pawn < PAWN_COUNT; ++pawn) {
            Position p = state.position[player][pawn];
            if (state.height[p.y][p.x] == MAX_HEIGHT - 1) {
                return player;
            }
        }
    }
    if (!get_legal_plays(state).size()) {
        return 1 - state.player;
    }
    return -1;
}

class MonteCarlo {
    public:
        MonteCarlo(chrono::milliseconds time_limit) : time_limit_(time_limit) {}

        int select_move(const State &state, const Plays &plays) {
            Play play = get_next_play(state);
            for (int i = 0; i < plays.size(); ++i) {
                if (play == plays[i]) {
                    return i;
                }
            }
            cout << "No valid move selected. Picking the first.\n";
            return -1;
        }

        Play get_next_play(const State &state) {
            max_depth_ = 0;
            Plays legal = get_legal_plays(state);

            if (!legal.size()) {
                Play play;
                play.pawn = -1;  // Negative pawn means error.
                return play;
            } else if (legal.size() == 1) {
                return legal[0];
            }

            int games = 0;
            const auto start_time = chrono::steady_clock::now();
            while (chrono::steady_clock::now() - start_time < time_limit_) {
                run_simulation(state);
                games++;
            }

            cout << "Game count = " << games << "\n";

            Play best_play;
            double best_win_percent = -1;
            for (const Play &play : legal) {
                State next_state = get_next_state(state, play);
                auto iter = state_counts_.find(next_state);
                double win_percent =
                    iter == state_counts_.end() ?
                    0.0 :
                    iter->second.wins / iter->second.plays;
                if (win_percent > best_win_percent) {
                    best_win_percent = win_percent;
                    best_play = play;
                }
            }
            cout << "max depth = " << max_depth_ << "\n";
            cout << "win percent = " << best_win_percent << "\n";
            return best_play;
        }

        void run_simulation(const State &state) {
            unordered_set<State> visited_states;

            bool expand = true;
            int winner = -1;
            State this_state = state;
            for (int t = 0;; ++t) {
                double total = 0.0;
                bool all_seen = true;
                Plays legal = get_legal_plays(this_state);
                State next_state;
                for (const Play &play : legal) {
                    next_state = get_next_state(this_state, play);
                    auto iter = state_counts_.find(next_state);
                    if (iter == state_counts_.end()) {
                        all_seen = false;
                        break;
                    }
                    total += iter->second.plays;
                }
                if (all_seen) {
                    double log_total = log(total);
                    double best_score = -1;
                    for (const Play &play : legal) {
                        State s = get_next_state(this_state, play);
                        auto iter = state_counts_.find(s);
                        // Should not happen if all_seen.
                        assert(iter != state_counts_.end());
                        double score =
                            iter->second.wins/iter->second.plays
                            + sqrt(2*log_total/iter->second.plays);
                        if (score > best_score) {
                            best_score = score;
                            next_state = s;
                        }
                    }
                }

                this_state = next_state;

                if (expand && !all_seen) {
                    expand = false;
                    Counts counts(0, 0);
                    state_counts_.emplace(this_state, counts);
                    if (t > max_depth_) {
                        max_depth_ = t;
                    }
                }

                visited_states.emplace(this_state);

                winner = get_winner(this_state);
                if (winner >= 0) {
                    break;
                }
            }

            for (const State &visited_state : visited_states) {
                auto iter = state_counts_.find(visited_state);
                if (iter == state_counts_.end()) {
                    continue;
                }
                iter->second.plays++;
                if (visited_state.player != winner) {
                    iter->second.wins++;
                }
            }
        }

    private:
        chrono::milliseconds time_limit_;
        int max_depth_;
        unordered_map<State, Counts> state_counts_;
};

// Plays the game with the state as the starting state and the scratch space.
//
// Returns the index of the winning player (either 0 or 1).
template <bool verbose=false, typename P0, typename P1>
int play_game(State *state, P0 *p0, P1 *p1) {
    for (int move_number = 0;; ++move_number) {
        if (verbose) { printf("Move %2d\n", move_number); }
        Plays plays = get_legal_plays(*state);
        if (!plays.size()) {
            // Next player loses because they have no legal moves.
            if (verbose) {
                printf(
                        "Player %d wins because player %d has no legal moves.\n",
                        1 - state->player, state->player);
            }
            return 1 - state->player;
        }
        int index = state->player ?
            p1->select_move(*state, plays) :
            p0->select_move(*state, plays);
        Play play = plays[index];
        if (state->height[play.end.y][play.end.x] == MAX_HEIGHT - 1) {
            // Next player wins because they stepped to the winning height.
            if (verbose) {
                printf(
                        "Player %d wins by stepping onto (%d,%d)\n",
                        state->player, play.end.x, play.end.y);
            }
            return state->player;
        }
        if (verbose) {
            printf(
                    "Player %d moves pawn %d to (%d,%d) and builds at (%d,%d)\n",
                    state->player,
                    play.pawn,
                    play.end.x,
                    play.end.y,
                    play.build.x,
                    play.build.y);
        }
        // Update board due to selected move.
        state->position[state->player][play.pawn] = play.end;
        ++state->height[play.build.y][play.build.x];
        state->player = 1 - state->player;
        if (verbose) {
            print_state(*state);
        }
    }
}

class HumanPlayer {
    public:
        int select_move(const State &state, const Plays &plays) {
            //print_state(state);
            string player_label = state.player ? "b" : "a";
            string expected_pawns[] =
            {player_label + "0", player_label + "1"};
            int pawn;
            Position end;
            Position build;
            while (true) {
                string input;

                // Get pawn.
                pawn = 0;
                while (true) {
                    cout << "Which pawn will you move ("
                        << expected_pawns[0]
                        << " or " << expected_pawns[1] << ")\n> ";
                    cin >> input;
                    if (input != expected_pawns[0] &&
                            input != expected_pawns[1]) {
                        cout << "Invalid pawn selection, please enter "
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
                for (int i = 0; i < plays.size(); ++i) {
                    if (plays[i].pawn == pawn) {
                        valid_pawn = true;
                        break;
                    }
                }
                if (!valid_pawn) {
                    cout << "Pawn " << expected_pawns[pawn]
                        << " has no valid moves, "
                        << "please select the other pawn.\n";
                    continue;
                }

                // Get end.
                Position start = state.position[state.player][pawn];
                while (true) {
                    cout << "Which direction will you move\n> ";
                    char direction;
                    cin >> direction;
                    end = get_new_position(start, direction);
                    if (end.x < 0) {
                        cout << "Invalid move direction\n";
                        continue;
                    }
                    bool valid_move = false;
                    for (int i = 0; i < plays.size(); ++i) {
                        if (plays[i].pawn == pawn && plays[i].end == end) {
                            valid_move = true;
                            break;
                        }
                    }
                    if (!valid_move) {
                        cout << "That move is not legal for that pawn. "
                            "Try again.\n";
                        continue;
                    }
                    break;
                }

                // Get build.
                while (true) {
                    cout << "Which direction will you build\n> ";
                    char direction;
                    cin >> direction;
                    build = get_new_position(end, direction);
                    if (build.x < 0) {
                        cout << "Invalid build direction\n";
                        continue;
                    }
                    for (int i = 0; i < plays.size(); ++i) {
                        if (plays[i].pawn == pawn &&
                                plays[i].end == end &&
                                plays[i].build == build) {
                            return i;
                        }
                    }
                    cout
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

int main(int argc, char *argv[]) {
    random_device random_device;
    unsigned int seed = argc > 1 ? stoul(argv[1]) : random_device();
    printf("Seed = %u\n", seed);
    mt19937 rng(seed);

    int counts[2] = {0, 0};
    MonteCarlo player0(chrono::minutes(5));
    HumanPlayer player1;
    for (int trial = 0; trial < 100; ++trial) {
        State state = get_start_state();
        print_state(state);
        printf("\n");
        int winner = play_game<true>(&state, &player0, &player1);
        ++counts[winner];
        printf("\n");
        print_state(state);
        printf("Trial %3d won by player %d (%d to %d).\n",
                trial,
                winner,
                counts[0],
                counts[1]);
    }
    printf(
            "\nPlayer 0 wins %d times, player 1 wins %d times.\n",
            counts[0],
            counts[1]);
}
