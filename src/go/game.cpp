// MOKU rules engine - the rules layer: komi, handicap, positional superko, passes, resign,
// undo by replay, dead-stone marking, final score (see game.h).
#include "go/game.h"
#include <cstring>

namespace go {

namespace {
// Scratch board for the superko simulation. A Board is ~11 KB: far too big for the stack on the GBA,
// and too big for .bss (IWRAM) as well, hence GO_EWRAM_BSS. The check is never re-entered (single
// threaded, no callbacks), so one shared copy is enough.
GO_EWRAM_BSS Board g_superko_scratch;
}  // namespace

int handicap_points(int size, int handicap, Point* out)
{
    if (handicap < 2 || handicap > 5 || out == nullptr) return 0;
    int lo, hi, mid;
    switch (size) {
        case 9:  lo = 2; hi = 6;  mid = 4; break;
        case 13: lo = 3; hi = 9;  mid = 6; break;
        case 19: lo = 3; hi = 15; mid = 9; break;
        default: return 0;                       // 7x7 (and odd sizes) have no hoshi convention
    }
    // Standard order: upper right, lower left, lower right, upper left, centre (y grows downwards).
    const int xs[5] = {hi, lo, hi, lo, mid};
    const int ys[5] = {lo, hi, hi, lo, mid};
    const int stride = size + 2;
    for (int i = 0; i < handicap; ++i) out[i] = Point(point_index(stride, xs[i], ys[i]));
    return handicap;
}

void Game::start(const GameSettings& s)
{
    settings_ = s;
    if (settings_.size < MIN_SIZE) settings_.size = MIN_SIZE;
    if (settings_.size > MAX_SIZE) settings_.size = MAX_SIZE;
    if (settings_.komi_x2 < 0) settings_.komi_x2 = 0;

    board_.init(settings_.size);
    to_move_ = BLACK;
    initial_to_move_ = BLACK;
    phase_ = Phase::PLAYING;
    end_reason_ = EndReason::NONE;
    winner_ = EMPTY;
    passes_ = 0;
    move_count_ = 0;
    setup_count_ = 0;
    std::memset(dead_, 0, sizeof(dead_));
    final_score_ = ScoreResult{};

    Point hp[5];
    const int n = handicap_points(settings_.size, settings_.handicap, hp);
    if (n == 0) settings_.handicap = 0;          // unsupported size/handicap: play an even game
    for (int i = 0; i < n; ++i) {
        board_.set_stone(hp[i], BLACK);
        setup_[setup_count_++] = Move{hp[i], BLACK};
    }
    if (n > 0) {
        to_move_ = WHITE;
        initial_to_move_ = WHITE;
    }
    history_[0] = board_.hash();
}

void Game::add_setup_stone(Point p, Color c)
{
    if (move_count_ > 0 || setup_count_ >= MAX_SIZE * MAX_SIZE) return;
    if (!board_.on_board(p) || board_.at(p) != EMPTY) return;
    if (c != BLACK && c != WHITE) return;
    board_.set_stone(p, c);
    setup_[setup_count_++] = Move{p, c};
    history_[0] = board_.hash();
}

void Game::set_to_move(Color c)
{
    if (c != BLACK && c != WHITE) return;
    to_move_ = c;
    if (move_count_ == 0) initial_to_move_ = c;
}

bool Game::is_legal(Point p) const
{
    if (phase_ != Phase::PLAYING) return false;
    if (p == PASS) return true;
    if (!board_.is_legal(p, to_move_)) return false;
    return !would_repeat(p);
}

bool Game::would_repeat(Point p) const
{
    if (p == PASS || !board_.on_board(p) || board_.at(p) != EMPTY) return false;
    // Only a capture can recreate an earlier position: everything else adds a stone.
    if (!board_.is_capture(p, to_move_)) return false;
    g_superko_scratch.copy_from(board_);
    g_superko_scratch.play(p, to_move_);
    const uint64_t h = g_superko_scratch.hash();
    for (int i = 0; i <= move_count_; ++i)
        if (history_[i] == h) return true;
    return false;
}

void Game::apply_move(Point p, Color c)
{
    if (p == PASS) board_.play_pass(c);
    else board_.play(p, c);
}

PlayResult Game::play(Point p)
{
    PlayResult r;
    if (phase_ != Phase::PLAYING || move_count_ >= MAX_MOVES) return r;

    if (p == PASS) {
        const Color c = to_move_;
        apply_move(PASS, c);
        moves_[move_count_] = Move{PASS, c};
        ++move_count_;
        history_[move_count_] = board_.hash();
        ++passes_;
        to_move_ = opponent(c);
        r.ok = true;
        if (passes_ >= 2) {
            phase_ = Phase::MARK_DEAD;
            end_reason_ = EndReason::TWO_PASSES;
            r.game_ended = true;
        }
        return r;
    }

    if (!is_legal(p)) return r;
    const Color c = to_move_;
    const Color opp = opponent(c);
    r.captured = board_.play(p, c);
    moves_[move_count_] = Move{p, c};
    ++move_count_;
    history_[move_count_] = board_.hash();
    passes_ = 0;
    r.ok = true;
    r.self_in_atari = board_.in_atari(p);
    const int stride = board_.stride();
    const Point nb[4] = {Point(p - stride), Point(p - 1), Point(p + 1), Point(p + stride)};
    for (int i = 0; i < 4; ++i)
        if (board_.at(nb[i]) == opp && board_.in_atari(nb[i])) r.atari_created = true;
    to_move_ = opp;
    return r;
}

void Game::resign(Color c)
{
    if (c != BLACK && c != WHITE) return;
    phase_ = Phase::OVER;
    end_reason_ = EndReason::RESIGN;
    winner_ = opponent(c);
    // The point totals are informational after a resignation; the winner is the resignee's opponent.
    final_score_ = score_game(board_, dead_, settings_.komi_x2, settings_.rules);
    final_score_.winner = winner_;
}

bool Game::undo()
{
    if (move_count_ <= 0) return false;
    --move_count_;
    phase_ = Phase::PLAYING;
    end_reason_ = EndReason::NONE;
    winner_ = EMPTY;
    final_score_ = ScoreResult{};
    std::memset(dead_, 0, sizeof(dead_));
    rebuild();
    return true;
}

void Game::rebuild()
{
    board_.init(settings_.size);
    for (int i = 0; i < setup_count_; ++i) board_.set_stone(setup_[i].p, setup_[i].c);
    history_[0] = board_.hash();
    Color tm = initial_to_move_;
    for (int i = 0; i < move_count_; ++i) {
        apply_move(moves_[i].p, moves_[i].c);
        history_[i + 1] = board_.hash();
        tm = opponent(moves_[i].c);
    }
    to_move_ = tm;
    passes_ = 0;
    for (int i = move_count_ - 1; i >= 0 && moves_[i].p == PASS && passes_ < 2; --i) ++passes_;
}

void Game::toggle_dead(Point p)
{
    if (!board_.on_board(p)) return;
    const Color c = board_.at(p);
    if (c != BLACK && c != WHITE) return;
    const bool value = !dead_[p];
    Point q = p;
    do {
        dead_[q] = value;
        q = board_.next_stone(q);
    } while (q != p);
}

void Game::clear_dead_marks()
{
    std::memset(dead_, 0, sizeof(dead_));
}

void Game::propose_dead_marks()
{
    propose_dead(board_, dead_);
}

void Game::finish_marking()
{
    if (phase_ == Phase::OVER) return;
    final_score_ = score_game(board_, dead_, settings_.komi_x2, settings_.rules);
    winner_ = final_score_.winner;
    phase_ = Phase::OVER;
    if (end_reason_ == EndReason::NONE) end_reason_ = EndReason::TWO_PASSES;
}

void Game::resume_play()
{
    if (phase_ != Phase::MARK_DEAD) return;
    phase_ = Phase::PLAYING;
    end_reason_ = EndReason::NONE;
    winner_ = EMPTY;
    passes_ = 0;
    std::memset(dead_, 0, sizeof(dead_));   // marks would go stale as soon as play resumes
}

ScoreResult Game::score() const
{
    const bool* d = (phase_ == Phase::PLAYING) ? nullptr : dead_;
    return score_game(board_, d, settings_.komi_x2, settings_.rules);
}

}  // namespace go
