// Campaign missions: 20 code-defined missions, objective checkers, ranks, unlocks, hints.
// Platform-agnostic (host-tested). Text lives in the string tables (strings_ids.h, mission_text_*.h).
#pragma once
#include "go/game.h"
#include "ai/ai.h"

namespace campaign {

constexpr int MISSION_COUNT = 20;

enum class Opponent : uint8_t { NONE = 0, VERY_EASY, EASY, NORMAL, HARD, MASTER };   // NONE = passes

enum class Objective : uint8_t {
    PLACE_ON_MARKED,     // 1  place stones on all marked points (param = count)
    REDUCE_TO_1_LIB,     // 2  marked stone/group reaches exactly 1 liberty
    CAPTURE_MARKED,      // 3, 7, 16 capture the marked stone/group
    SAVE_MARKED,         // 4  marked group reaches >= param liberties (3)
    CONNECT_MARKED,      // 5  the two marked groups become one group
    CAPTURE_LADDER,      // 6  capture the marked stone (AI extends; fails if it escapes: >= 3 libs)
    CAPTURE_NET,         // 8  capture marked stone; playing the ladder atari (marked bad point) fails
    KO_WIN,              // 9  capture at the marked ko point twice
    KILL_MARKED,         // 10, 12 marked group is captured or dead (no two eyes, opponent can't save)
    MAKE_TWO_EYES,       // 11 marked group pass-alive (Benson) within param moves
    SNAPBACK,            // 13 capture >= 2 stones with a snapback (a stone sacrificed then recaptured)
    SEKI_SURVIVE,        // 14 survive param moves without either marked group being captured
    WIN_BY_MARGIN,       // 15 full game, win by >= param points (x2 stored)
    LIVE_IN_CORNER,      // 17 make a pass-alive group inside the marked corner area within param moves
    TSUMEGO,             // 18 staged: each stage has its own objective (uses stages[])
    WIN_GAME,            // 19, 20 full game win
};

enum class MissionStatus : uint8_t { PLAYING = 0, CLEAR = 1, FAIL = 2 };
enum class Rank : uint8_t { NONE = 0, C = 1, B = 2, A = 3, S = 4 };

struct SetupStone { int8_t x, y; go::Color c; };
// A board point. In a solution list, {-1, -1} means "pass / play elsewhere": missions 9 and 14 need
// the player to give up the move without touching the position.
struct XY { int8_t x, y; };

struct MissionDef {
    uint8_t id;                      // 1..20
    uint8_t board;                   // 7, 9, 13, 19
    go::Color player;                // colour the player controls
    Opponent opponent;
    uint8_t handicap;                // full games only
    go::Ruleset rules;
    int16_t komi_x2;
    const SetupStone* stones; uint8_t stone_count;
    const XY* marked; uint8_t marked_count;      // marked points (meaning per objective)
    const XY* marked2; uint8_t marked2_count;    // second set (CONNECT: second group; NET: forbidden ladder point; LIVE_IN_CORNER: area corners)
    Objective objective;
    int16_t param;                   // objective parameter
    uint8_t move_limit;              // player moves before FAIL (0 = none)
    uint8_t rank_s, rank_a, rank_b;  // max player moves for S/A/B (C otherwise)
    const XY* solution; uint8_t solution_len;    // scripted winning player moves (for hints + tests)
    uint8_t unlock;                  // UNLOCK_* id granted on clear (0 = none)
    const MissionDef* stages; uint8_t stage_count;   // TSUMEGO sub-problems
    uint16_t name_id, objective_id, rank_hint_id;    // string ids
    uint16_t text_before, text_hint, text_win, text_fail;  // string ids (dialogue)
};

enum : uint8_t {
    UNLOCK_NONE = 0, UNLOCK_SANDBOX_9_EASY = 1, UNLOCK_NORMAL = 2, UNLOCK_13 = 3, UNLOCK_HARD = 4,
    UNLOCK_19_MASTER = 5
};

const MissionDef& mission(int index);          // 0-based
const MissionDef* all_missions();

struct MissionRun {
    const MissionDef* def = nullptr;
    const MissionDef* stage = nullptr;         // current stage (== def unless TSUMEGO)
    uint8_t stage_index = 0;
    go::Game game;
    MissionStatus status = MissionStatus::PLAYING;
    int player_moves = 0, undos = 0, hints = 0, captures = 0;
    int ko_captures = 0;                       // KO_WIN progress
    int survive_moves = 0;                     // SEKI progress
    bool snapback_armed = false;               // SNAPBACK progress
    uint16_t fail_reason_id = 0;               // string id explaining a FAIL
};

void mission_start(MissionRun& run, const MissionDef& def, int stage = 0);
// Called after every move (player or opponent) or undo. Updates progress and status.
MissionStatus mission_check(MissionRun& run);
// Player plays p (must be legal). Returns the play result; updates counters and checks the objective.
go::PlayResult mission_player_move(MissionRun& run, go::Point p);
// Opponent move for the run's Opponent (NONE = pass). Uses ai::pick_move on host; on GBA the scene
// drives an ai::Thinker itself and then calls mission_opponent_played().
void mission_opponent_played(MissionRun& run, go::Point p);
ai::Level opponent_level(Opponent o);          // NONE -> VERY_EASY (never called for NONE)
bool mission_undo(MissionRun& run);            // undoes the last player move (and the reply)
go::Point mission_hint(MissionRun& run);       // next solution move if on the solution line, else ai NORMAL
bool next_stage(MissionRun& run);              // TSUMEGO: advance; false when finished

Rank mission_rank(const MissionRun& run);      // from player moves / undos / hints
bool mission_stars(const MissionRun& run);     // rank >= A && hints == 0 && undos == 0
// Points the HUD should mark for this stage (marked set), for drawing.
int mission_marked_points(const MissionRun& run, go::Point* out, int max_out);

}  // namespace campaign
