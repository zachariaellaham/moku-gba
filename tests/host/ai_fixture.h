// Test helpers for the AI suite: build positions from ASCII diagrams.
#pragma once
#include <string>
#include <vector>
#include "go/game.h"
#include "ai/ai.h"

namespace fixture {

// Rows are given top to bottom, one character per point:
//   '.' empty   'X' black   'O' white   any other character = empty and reported as a marker.
// Returns the marker points by character.
struct Position {
    go::Game game;
    std::vector<std::pair<char, go::Point>> markers;
    go::Point marker(char c) const {
        for(const auto& m : markers) if(m.first == c) return m.second;
        return go::NO_POINT;
    }
    std::vector<go::Point> all_markers(char c) const {
        std::vector<go::Point> out;
        for(const auto& m : markers) if(m.first == c) out.push_back(m.second);
        return out;
    }
};

inline void build(Position& pos, const std::vector<std::string>& rows, go::Color to_move,
                  go::Ruleset rules = go::Ruleset::AREA, int komi_x2 = 15)
{
    go::GameSettings s;
    s.size = uint8_t(rows.size());
    s.rules = rules;
    s.komi_x2 = int16_t(komi_x2);
    pos.game.start(s);
    pos.markers.clear();
    for(int y = 0; y < int(rows.size()); ++y)
    {
        for(int x = 0; x < int(rows[y].size()); ++x)
        {
            const char c = rows[y][x];
            const go::Point p = pos.game.board().point(x, y);
            if(c == 'X') pos.game.add_setup_stone(p, go::BLACK);
            else if(c == 'O') pos.game.add_setup_stone(p, go::WHITE);
            else if(c != '.') pos.markers.push_back({c, p});
        }
    }
    pos.game.set_to_move(to_move);
}

inline std::string dump(const go::Board& b)
{
    std::string out;
    for(int y = 0; y < b.size(); ++y)
    {
        for(int x = 0; x < b.size(); ++x)
        {
            const go::Color c = b.at(x, y);
            out += (c == go::BLACK ? 'X' : c == go::WHITE ? 'O' : '.');
        }
        out += '\n';
    }
    return out;
}

}  // namespace fixture
