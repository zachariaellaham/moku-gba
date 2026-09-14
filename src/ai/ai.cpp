// Thinker: the resumable front end shared by the five levels.
#include "ai/mcts.h"
#include "go/score.h"

namespace ai {

using namespace detail;
using go::Game;
using go::PASS;
using go::Point;

namespace {

struct State {
    const Game* game = nullptr;
    Rng rng;
    // Preparation, spread over frames: generating the moves and ranking them with a
    // ladder-verified prior is about eighty milliseconds on this machine, which is five frames.
    int prep_stage = 0;
    int prep_index = 0;
    int prep_count = 0;
    int prep_shortlist = 0;
    int prep_keep = 0;
    Point prep_moves[go::MAX_SIZE * go::MAX_SIZE];
    int prep_priors[go::MAX_SIZE * go::MAX_SIZE];
    // NORMAL
    Point candidates[go::MAX_SIZE * go::MAX_SIZE];
    int candidate_count = 0;
    int candidate_index = 0;
    int influence_before = 0;
    MoveEval best;
    MoveEval top[8];
    int top_count = 0;
    // HARD / MASTER
    Mcts mcts;
    bool searching = false;            // false while the NORMAL pass is still scoring candidates
    int deep_index = 0;                // two-ply refinement progress
    int deep_count = 0;
    Point deep_moves[16];
    int deep_scores[16];
    DeepSearch deep;
    bool deep_started = false;
    Point search_moves[MCTS_MAX_CHILDREN];
    int search_priors[MCTS_MAX_CHILDREN];
    int search_count = 0;
};

State g_state GO_EWRAM_BSS;

void push_top(State& s, const MoveEval& e)
{
    int pos = s.top_count < 8 ? s.top_count : 8;
    while(pos > 0 && s.top[pos - 1].score < e.score)
    {
        if(pos < 8) s.top[pos] = s.top[pos - 1];
        --pos;
    }
    if(pos < 8) s.top[pos] = e;
    if(s.top_count < 8) ++s.top_count;
}

// The evaluator works in centipoints, roughly -3000..+3000; the search wants 1..1000.
int clamp_prior(int centipoints)
{
    int v = 500 + centipoints / 8;
    if(v < 1) v = 1;
    if(v > 1000) v = 1000;
    return v;
}

int clamp_prior_raw(int v)
{
    if(v < 1) v = 1;
    if(v > 1000) v = 1000;
    return v;
}

enum PrepStage : int { PREP_GENERATE = 0, PREP_CHEAP, PREP_SHORTLIST, PREP_REFINE, PREP_FINISH, PREP_DONE };

int root_children_for(int board_size)
{
    if(board_size <= 9) return MCTS_MAX_CHILDREN;
    if(board_size <= 13) return 20;
    return 12;
}

}  // namespace

Thinker::Thinker() { state_ = &g_state; }

void Thinker::reset_history() { low_winrate_streak_ = 0; }

void Thinker::start(const Game& game, Level level, uint32_t seed, const Limits* limits)
{
    State& s = *static_cast<State*>(state_);
    level_ = level;
    limits_ = limits ? *limits : default_limits(level);
    seed_ = seed ? seed : 1u;
    done_ = false;
    resign_ = false;
    result_ = PASS;
    stats_ = Stats();
    s.game = &game;
    s.rng = Rng(seed_);
    s.candidate_count = 0;
    s.candidate_index = 0;
    s.top_count = 0;
    s.deep_count = 0;
    s.deep_index = 0;
    s.deep_started = false;
    s.searching = false;
    s.search_count = 0;
    s.best = MoveEval();
    s.best.score = -(1 << 30);

    s.prep_stage = 0;
    s.prep_index = 0;
    s.prep_count = 0;
    s.prep_keep = 0;
    s.prep_shortlist = 0;

    if(level == Level::VERY_EASY || level == Level::EASY)
    {
        s.prep_stage = PREP_DONE;          // these two look at the board themselves
    }
}

namespace {

// One slice of the shared preparation: generate the moves, rank them cheaply, then pay for the
// ladder-verified prior on the shortlist only. Returns true when the candidates are ready.
bool prepare(State& s, Level level, Stats& stats, int& work)
{
    const Game& g = *s.game;
    const go::Board& b = g.board();

    // One stage per call, whatever the caller offers. Preparation is a small fixed amount of work
    // and the stages cost wildly different amounts of time; letting a frame run several of them is
    // how the display ends up stuttering.
    int budget = 1;

    // A unit of work is not the same amount of time in every stage, and it is not the same on a
    // 9x9 board as on a 19x19 one. Charging each stage what it actually costs keeps the caller's
    // controller honest: one frame is one frame.
    const bool big = b.size() > 13;
    const int cheap_chunk = big ? 8 : 24;
    const int refine_chunk = big ? 1 : 3;

    while(budget > 0 && s.prep_stage != PREP_DONE)
    {
        stats.phase = uint8_t(1 + s.prep_stage);
        switch(s.prep_stage)
        {
        case PREP_GENERATE:
        {
            // Chunked: on a 19x19 board this walks 361 points and asks the rules about each one,
            // which is more than a frame if it is done in one go.
            const int end = s.prep_index + (big ? 24 : 48) < b.empty_count()
                ? s.prep_index + (big ? 24 : 48) : b.empty_count();

            for(; s.prep_index < end; ++s.prep_index)
            {
                const go::Point p = b.empty_at(s.prep_index);
                if(b.is_eye_like(p, g.to_move())) continue;
                if(is_bad_self_atari(b, p, g.to_move())) continue;
                if(!g.is_legal(p)) continue;
                s.prep_moves[s.prep_count++] = p;
            }

            if(s.prep_index >= b.empty_count())
            {
                s.prep_keep = (level == Level::NORMAL)
                    ? (b.size() <= 9 ? 20 : (b.size() <= 13 ? 16 : 12))
                    : root_children_for(b.size());
                s.prep_shortlist = s.prep_keep * 2 < s.prep_count ? s.prep_keep * 2 : s.prep_count;
                s.prep_index = 0;
                s.prep_stage = PREP_CHEAP;
            }

            --budget;
            break;
        }

        case PREP_CHEAP:
        {
            const int end = s.prep_index + cheap_chunk < s.prep_count ? s.prep_index + cheap_chunk : s.prep_count;
            for(; s.prep_index < end; ++s.prep_index)
                s.prep_priors[s.prep_index] = quick_prior(b, s.prep_moves[s.prep_index], g.to_move(), b.last_move());
            if(s.prep_index >= s.prep_count) { s.prep_index = 0; s.prep_stage = PREP_SHORTLIST; }
            --budget;
            break;
        }

        case PREP_SHORTLIST:
            for(int i = 0; i < s.prep_shortlist; ++i)
            {
                int best = i;
                for(int j = i + 1; j < s.prep_count; ++j) if(s.prep_priors[j] > s.prep_priors[best]) best = j;
                if(best != i)
                {
                    const Point mp = s.prep_moves[i]; s.prep_moves[i] = s.prep_moves[best]; s.prep_moves[best] = mp;
                    const int pp = s.prep_priors[i]; s.prep_priors[i] = s.prep_priors[best]; s.prep_priors[best] = pp;
                }
            }
            s.prep_index = 0;
            s.prep_stage = PREP_REFINE;
            --budget;
            break;

        case PREP_REFINE:
        {
            const int end = s.prep_index + refine_chunk < s.prep_shortlist
                ? s.prep_index + refine_chunk : s.prep_shortlist;
            for(; s.prep_index < end; ++s.prep_index)
                s.prep_priors[s.prep_index] = refined_prior(b, s.prep_moves[s.prep_index], g.to_move());
            if(s.prep_index >= s.prep_shortlist) { s.prep_index = 0; s.prep_stage = PREP_FINISH; }
            --budget;
            break;
        }

        case PREP_FINISH:
        {
            const int count = s.prep_shortlist < s.prep_keep ? s.prep_shortlist : s.prep_keep;
            for(int i = 0; i < count; ++i)
            {
                int best = i;
                for(int j = i + 1; j < s.prep_shortlist; ++j) if(s.prep_priors[j] > s.prep_priors[best]) best = j;
                if(best != i)
                {
                    const Point mp = s.prep_moves[i]; s.prep_moves[i] = s.prep_moves[best]; s.prep_moves[best] = mp;
                    const int pp = s.prep_priors[i]; s.prep_priors[i] = s.prep_priors[best]; s.prep_priors[best] = pp;
                }
                s.candidates[i] = s.prep_moves[i];
            }

            s.candidate_count = count;
            s.candidate_index = 0;
            stats.candidates = count;

            influence_map(b, shared_influence());
            int total = 0;
            for(int y = 0; y < b.size(); ++y)
                for(int x = 0; x < b.size(); ++x)
                {
                    const int v = shared_influence()[b.point(x, y)];
                    total += (v > 0) - (v < 0);
                }
            s.influence_before = total;
            s.prep_stage = PREP_DONE;
            --budget;
            break;
        }

        default:
            s.prep_stage = PREP_DONE;
            break;
        }
    }

    return s.prep_stage == PREP_DONE;
}

}  // namespace

bool Thinker::step(int work)
{
    if(done_) return true;
    State& s = *static_cast<State*>(state_);
    const Game& g = *s.game;
    const Tuning& t = tuning();

    if(work < 1) work = 1;

    if(!prepare(s, level_, stats_, work))
    {
        return false;                      // still getting the candidates in order
    }

    if(s.candidate_count == 0 && level_ != Level::VERY_EASY && level_ != Level::EASY)
    {
        result_ = PASS;
        done_ = true;
        return true;
    }

    switch(level_)
    {
    case Level::VERY_EASY:
        result_ = pick_very_easy(g, s.rng);
        done_ = true;
        break;

    case Level::EASY:
        result_ = pick_easy(g, shared_scratch(), s.rng);
        done_ = true;
        break;

    case Level::NORMAL:
    {
        if(work < 1) work = 1;
        stats_.phase = 10;
        for(int i = 0; i < work && s.candidate_index < s.candidate_count; ++i, ++s.candidate_index)
        {
            const MoveEval e = evaluate_move(g, shared_scratch(), s.candidates[s.candidate_index],
                                             s.influence_before, shared_influence());
            push_top(s, e);
            if(e.score > s.best.score) s.best = e;
        }
        if(s.candidate_index >= s.candidate_count) finish();
        break;
    }

    case Level::HARD:
    case Level::MASTER:
    {
        if(work < 1) work = 1;

        if(!s.searching)
        {
            // still scoring candidates with the NORMAL evaluator, one per unit of work
            for(int i = 0; i < work && s.candidate_index < s.candidate_count; ++i, ++s.candidate_index)
            {
                const MoveEval e = evaluate_move(g, shared_scratch(), s.candidates[s.candidate_index],
                                                 s.influence_before, shared_influence());
                push_top(s, e);
                if(e.score > s.best.score) s.best = e;
                s.search_moves[s.search_count] = e.move;
                // map the evaluator's centipoints onto the 1..1000 prior scale
                s.search_priors[s.search_count] = clamp_prior(e.score);
                ++s.search_count;
            }

            if(s.candidate_index < s.candidate_count)
            {
                break;
            }

            // The best handful are searched deeper: HARD looks two plies, MASTER three. One
            // candidate per unit of work, so the display never waits for the whole search.
            if(s.deep_count == 0)
            {
                const int want = (level_ == Level::MASTER) ? (g.board().size() <= 13 ? 10 : 6) : 3;
                s.deep_count = s.top_count < want ? s.top_count : want;
                s.deep_index = 0;
            }

            {
                // Even depths only: after an odd number of plies the line ends on our own move,
                // which flatters it. MASTER searches the same two plies as HARD but far wider.
                // Even depths only: after an odd number of plies the line ends on our own move,
                // which flatters it. HARD looks two plies ahead, MASTER four.
                // Even depths only: after an odd number of plies the line ends on our own move,
                // which flatters it. How far and how wide is what the board size can afford: a
                // 19x19 evaluation costs six times a 9x9 one.
                // Both levels look two plies ahead; what separates them is how much of the board
                // they look at. Measured: a deeper search does not help here - the evaluator, not
                // the depth, is the limit - but a wider one does.
                const int size = g.board().size();
                SearchWidths widths;
                const int depth = 2;

                if(level_ == Level::MASTER)
                {
                    widths.width[0] = 8;
                    widths.width[1] = size > 13 ? 4 : 6;
                }
                else
                {
                    widths.width[0] = 3;
                    widths.width[1] = 2;
                }

                int deep_budget = (g.board().size() > 13) ? 1 : (work < 3 ? work : 3);

                while(s.deep_index < s.deep_count && deep_budget > 0)
                {
                    stats_.phase = s.deep_started ? 12 : 11;

                    if(!s.deep_started)
                    {
                        deep_begin(s.deep, g.board(), g.to_move(), s.top[s.deep_index].move,
                                   s.influence_before, shared_influence(), level_ == Level::MASTER);
                        s.deep_started = true;
                        --deep_budget;
                        continue;
                    }

                    if(!s.deep.replies_listed)
                    {
                        deep_list_replies(s.deep, g.to_move(), widths);
                        --deep_budget;
                        continue;
                    }

                    if(deep_step(s.deep, g.to_move(), g.settings().komi_x2, depth, widths,
                                 shared_influence(), level_ == Level::MASTER))
                    {
                        s.deep_moves[s.deep_index] = s.deep.move;
                        s.deep_scores[s.deep_index] = deep_value(s.deep);
                        ++s.deep_index;
                        s.deep_started = false;
                    }

                    --deep_budget;
                }
            }

            if(s.deep_index < s.deep_count)
            {
                break;
            }

            // the deep verdict decides the move, and seeds the priors the search starts from
            {
                int best_deep = s.deep_scores[0], worst_deep = s.deep_scores[0], best_i = 0;

                for(int i = 1; i < s.deep_count; ++i)
                {
                    if(s.deep_scores[i] > best_deep) { best_deep = s.deep_scores[i]; best_i = i; }
                    if(s.deep_scores[i] < worst_deep) worst_deep = s.deep_scores[i];
                }

                const int span = best_deep - worst_deep;
                s.best.move = s.deep_moves[best_i];
                s.best.score = best_deep;

                for(int i = 0; i < s.deep_count; ++i)
                {
                    for(int k = 0; k < s.search_count; ++k)
                    {
                        if(s.search_moves[k] != s.deep_moves[i]) continue;
                        const int bonus = span > 0 ? (600 * (s.deep_scores[i] - worst_deep)) / span : 300;
                        s.search_priors[k] = clamp_prior_raw(200 + bonus);
                    }
                }
            }

            s.searching = true;
            s.mcts.start_with_candidates(g, seed_, limits_.max_nodes, s.search_moves,
                                         s.search_priors, s.search_count);
            break;
        }

        const int budget = limits_.max_playouts > 0 ? limits_.max_playouts : default_limits(level_).max_playouts;
        int todo = budget - s.mcts.playouts();
        if(todo > work) todo = work;
        stats_.phase = 13;

        // The search is measured in board moves, and a move costs what the board size says it
        // costs. These are sized so a frame's worth of playouts stays inside a frame.
        const int size = g.board().size();
        const int moves_per_frame = size <= 9 ? 24 : (size <= 13 ? 12 : 6);
        int budget_moves = todo * moves_per_frame;
        if(budget_moves > moves_per_frame * 4) budget_moves = moves_per_frame * 4;
        if(todo > 0) s.mcts.run(budget_moves);
        stats_.playouts = s.mcts.playouts();
        stats_.nodes = s.mcts.nodes_used();
        stats_.winrate_permille = s.mcts.winrate_permille();
        if(s.mcts.playouts() >= budget) finish();
        break;
    }
    }
    (void)t;
    return done_;
}

void Thinker::finish()
{
    if(done_) return;
    State& s = *static_cast<State*>(state_);
    const Tuning& t = tuning();

    switch(level_)
    {
    case Level::VERY_EASY:
    case Level::EASY:
        step(1);
        return;

    case Level::NORMAL:
    {
        const go::Board& b = s.game->board();
        if(s.candidate_count == 0 || s.best.move == PASS)
        {
            result_ = PASS;
        }
        else
        {
            const bool endgame = b.empty_count() * 4 < b.size() * b.size() || b.last_move() == PASS;
            result_ = (endgame && s.best.score < t.pass_threshold) ? PASS : s.best.move;
        }
        stats_.winrate_permille = 500 + (s.best.score > 0 ? 1 : -1) * (s.best.score > 4000 ? 200 : s.best.score / 20);
        if(stats_.winrate_permille < 0) stats_.winrate_permille = 0;
        if(stats_.winrate_permille > 1000) stats_.winrate_permille = 1000;
        break;
    }

    case Level::HARD:
    case Level::MASTER:
    {
        if(!s.searching)
        {
            // the think was cut short before the search began: the evaluator has the answer
            result_ = s.best.move != PASS ? s.best.move : PASS;
            done_ = true;
            return;
        }

        stats_.playouts = s.mcts.playouts();
        stats_.nodes = s.mcts.nodes_used();
        stats_.winrate_permille = s.mcts.winrate_permille();
        result_ = s.mcts.best_move();
        // Resigning on a win rate measured over a handful of playouts would be guessing.
        if(limits_.allow_resign && s.mcts.playouts() >= 200)
        {
            if(stats_.winrate_permille < t.resign_permille) ++low_winrate_streak_;
            else low_winrate_streak_ = 0;
            resign_ = low_winrate_streak_ >= t.resign_streak;
        }
        break;
    }
    }
    done_ = true;
}

int Thinker::ranked_moves(Point* out, int max_out) const
{
    const State& s = *static_cast<const State*>(state_);
    if(level_ == Level::HARD || level_ == Level::MASTER) return s.mcts.ranked(out, max_out);
    if(level_ == Level::NORMAL)
    {
        int n = s.top_count < max_out ? s.top_count : max_out;
        for(int i = 0; i < n; ++i) out[i] = s.top[i].move;
        return n;
    }
    if(max_out > 0 && result_ != PASS) { out[0] = result_; return 1; }
    return 0;
}

Point pick_move(const Game& game, Level level, uint32_t seed, const Limits* limits)
{
    Thinker th;
    th.start(game, level, seed, limits);
    while(!th.step(48)) {}
    return th.result();
}

}  // namespace ai
