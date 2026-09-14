// Plays random games and prints every position, so tools/difftest.py can replay them through an
// independent implementation of the rules. See that script for the format.
//   tool_difftest [--games N] [--size N] [--seed N]
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "go/game.h"

using namespace go;

int main(int argc, char** argv)
{
    int games = 100, size = 9;
    uint32_t seed = 1;
    for(int i = 1; i < argc; ++i)
    {
        if(!strcmp(argv[i], "--games") && i + 1 < argc) games = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--size") && i + 1 < argc) size = atoi(argv[++i]);
        else if(!strcmp(argv[i], "--seed") && i + 1 < argc) seed = uint32_t(atoi(argv[++i]));
    }
    Rng rng(seed);
    for(int g = 0; g < games; ++g)
    {
        const int board_size = size > 0 ? size : (7 + int(rng.below(3)) * 2);
        GameSettings s;
        s.size = uint8_t(board_size);
        Game game;
        game.start(s);
        printf("game %d\n", board_size);
        const int limit = board_size * board_size * 2;
        for(int m = 0; m < limit && game.phase() == Phase::PLAYING; ++m)
        {
            const Board& b = game.board();
            // pick a random empty point; report the engine's verdict either way
            Point p = PASS;
            for(int tries = 0; tries < 12 && p == PASS; ++tries)
            {
                if(b.empty_count() == 0) break;
                const Point q = b.empty_at(int(rng.below(uint32_t(b.empty_count()))));
                if(game.is_legal(q) && !b.is_eye_like(q, game.to_move())) p = q;
                else if(!game.is_legal(q)) printf("illegal %d %d %c\n", b.x_of(q), b.y_of(q), game.to_move() == BLACK ? 'B' : 'W');
            }
            const char colour = game.to_move() == BLACK ? 'B' : 'W';
            if(p == PASS) printf("pass %c\n", colour);
            else printf("move %d %d %c\n", b.x_of(p), b.y_of(p), colour);
            game.play(p);
            printf("board\n");
            for(int y = 0; y < b.size(); ++y)
            {
                for(int x = 0; x < b.size(); ++x)
                {
                    const Color c = b.at(x, y);
                    putchar(c == BLACK ? 'X' : c == WHITE ? 'O' : '.');
                }
                putchar('\n');
            }
            printf("captures %d %d\n", b.captures(BLACK), b.captures(WHITE));
        }
    }
    return 0;
}
