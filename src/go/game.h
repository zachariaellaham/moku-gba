// Rules layer on top of Board: rulesets, komi, handicap, positional superko, passes, resign,
// undo (by replay), dead-stone marking and final scoring.
#pragma once
#include "board.h"
#include "score.h"

namespace go {

enum class Ruleset : uint8_t { AREA = 0, TERRITORY = 1 };
enum class Phase : uint8_t { PLAYING = 0, MARK_DEAD = 1, OVER = 2 };
enum class EndReason : uint8_t { NONE = 0, TWO_PASSES = 1, RESIGN = 2, MISSION = 3 };

struct GameSettings {
    uint8_t size = 9;
    Ruleset rules = Ruleset::AREA;
    int16_t komi_x2 = 15;        // komi in half points: 7.5 -> 15, 6.5 -> 13, 0 -> 0 (range 0..18)
    uint8_t handicap = 0;        // 0 or 2..5 (9/13/19 only). With handicap, White moves first.
    static int16_t default_komi_x2(Ruleset r) { return r == Ruleset::AREA ? 15 : 13; }
};

// Fixed hoshi handicap placement. Writes up to 5 points, returns the count (0 if unsupported).
int handicap_points(int size, int handicap, Point* out);

struct PlayResult {
    bool ok = false;             // false if illegal
    int captured = 0;
    bool atari_created = false;  // an opponent group is in atari after this move
    bool self_in_atari = false;  // the played stone's group is in atari
    bool game_ended = false;     // two passes -> MARK_DEAD, or a resign
};

class Game {
public:
    void start(const GameSettings& s);
    // Setup stones for missions: call after start(), before any play(). Not counted as moves.
    void add_setup_stone(Point p, Color c);
    void set_to_move(Color c);

    const GameSettings& settings() const { return settings_; }
    const Board& board() const { return board_; }
    Color to_move() const { return to_move_; }
    Phase phase() const { return phase_; }
    EndReason end_reason() const { return end_reason_; }
    Color winner() const { return winner_; }          // valid when phase() == OVER (EMPTY = draw)

    // Legality including positional superko. PASS is always legal while PLAYING.
    bool is_legal(Point p) const;
    bool would_repeat(Point p) const;                 // superko test only (p assumed otherwise legal)
    PlayResult play(Point p);                         // p or PASS, for to_move()
    void resign(Color c);
    bool can_undo() const { return move_count_ > 0; }
    bool undo();                                      // removes the last move (also from MARK_DEAD)

    int move_count() const { return move_count_; }
    const Move* moves() const { return moves_; }
    int consecutive_passes() const { return passes_; }
    int setup_count() const { return setup_count_; }
    const Move* setup_stones() const { return setup_; }

    // Dead-stone marking (phase MARK_DEAD). Marks apply to whole groups.
    void toggle_dead(Point p);
    bool is_dead(Point p) const { return dead_[p]; }
    void clear_dead_marks();
    void propose_dead_marks();                        // fills marks with score::propose_dead()
    void finish_marking();                            // -> OVER, computes final score
    void resume_play();                               // MARK_DEAD -> PLAYING (disputed marks)

    // Score with the current dead marks (valid in MARK_DEAD and OVER; in PLAYING = live estimate
    // with no dead stones).
    ScoreResult score() const;
    const ScoreResult& final_score() const { return final_score_; }  // valid when OVER

    // Hash history (positions after each move, index 0 = initial position).
    int history_count() const { return move_count_ + 1; }
    uint64_t history_hash(int i) const { return history_[i]; }

private:
    void rebuild();                                   // replay setup + moves into board_
    void apply_move(Point p, Color c);

    GameSettings settings_;
    Board board_;
    Color to_move_ = BLACK, initial_to_move_ = BLACK;
    Phase phase_ = Phase::PLAYING;
    EndReason end_reason_ = EndReason::NONE;
    Color winner_ = EMPTY;
    int passes_ = 0;
    int move_count_ = 0;
    int setup_count_ = 0;
    Move moves_[MAX_MOVES];
    Move setup_[MAX_SIZE * MAX_SIZE];
    uint64_t history_[MAX_MOVES + 1];
    bool dead_[MAX_POINTS];
    ScoreResult final_score_;
};

}  // namespace go
