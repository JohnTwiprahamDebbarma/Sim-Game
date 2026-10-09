/*
 * The Game of Sim -- solver engine.  See sim.h.
 */
#include "sim.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

/* ------------------------------------------------------------------ board */

void init_board(board_t board)
{
    for (int line = 0; line < BOARD_SIZE; ++line)
        board[line] = EMPTY;
}

bool is_full(board_t board)
{
    for (int line = 0; line < BOARD_SIZE; ++line)
        if (board[line] == EMPTY)
            return false;
    return true;
}

player_t other_player(player_t player)
{
    assert(player == RED || player == BLUE);
    return player == RED ? BLUE : RED;
}

/* -------------------------------------------------------------- triangles */

int edge_index(int u, int v)
{
    int lo = u < v ? u : v;
    int hi = u < v ? v : u;

    assert(0 <= lo && lo < hi && hi < NVERT);
    return (NVERT - 1) * lo - lo * (lo - 1) / 2 + (hi - lo - 1);
}

int triangle[NTRIANGLE][3];

void init_triangles(void)
{
    int n = 0;

    for (int a = 0; a < NVERT; ++a)
        for (int b = a + 1; b < NVERT; ++b)
            for (int c = b + 1; c < NVERT; ++c) {
                triangle[n][0] = edge_index(a, b);
                triangle[n][1] = edge_index(a, c);
                triangle[n][2] = edge_index(b, c);
                ++n;
            }
    assert(n == NTRIANGLE);
}

bool has_lost(board_t board, player_t player)
{
    for (int t = 0; t < NTRIANGLE; ++t)
        if (board[triangle[t][0]] == player &&
            board[triangle[t][1]] == player &&
            board[triangle[t][2]] == player)
            return true;
    return false;
}

/* ----------------------------------------------------------------- solver */

/*
 * Every position is one base-3 digit per edge, so 3^15 entries suffice.  A
 * stored byte is nonzero, which is what marks the slot as computed.
 */
#define POSITIONS 14348907L           /* 3^15 */

static uint8_t computed_moves[POSITIONS];

long memo_entries(void)
{
    long used = 0;

    for (long i = 0; i < POSITIONS; ++i)
        if (computed_moves[i])
            ++used;
    return used;
}

void reset_memo(void)
{
    memset(computed_moves, 0, sizeof computed_moves);
}

static long ord(board_t board)
{
    long i = 0;

    for (int line = BOARD_SIZE - 1; line >= 0; --line) {
        int d;
        switch (board[line]) {
        case RED:  d = 1; break;
        case BLUE: d = 2; break;
        default:   d = 0; break;
        }
        i = i * 3 + d;
    }
    return i;
}

static uint8_t encode_move(move_t m)
{
    assert(0 <= m.line && m.line < BOARD_SIZE);
    assert(-1 <= m.score && m.score <= 1);
    return (uint8_t)(m.line | (1 << (5 + m.score)));
}

static move_t decode_move(uint8_t b)
{
    move_t m;

    m.line = b & 0x0f;
    if (b & 0x10)      m.score = -1;
    else if (b & 0x20) m.score = 0;
    else               { assert(b & 0x40); m.score = 1; }
    return m;
}

#ifndef NDEBUG
/*
 * Positions are memoized by board alone.  That is sound only because Red moves
 * first and the players strictly alternate, which makes the side to move a
 * function of the number of colored edges rather than extra key material.
 */
static player_t side_to_move(board_t board)
{
    int red = 0, blue = 0;

    for (int line = 0; line < BOARD_SIZE; ++line) {
        if (board[line] == RED) ++red;
        else if (board[line] == BLUE) ++blue;
    }
    assert(red == blue || red == blue + 1);
    return red == blue ? RED : BLUE;
}
#endif

/*
 * Negamax: the value of a position is the negation of the best the opponent
 * can do in reply.
 */
move_t best_move(board_t board, player_t player)
{
    move_t best = { .line = -1, .score = -1 };
    long o = ord(board);

    assert(player == side_to_move(board));

    if (computed_moves[o])
        return decode_move(computed_moves[o]);

    for (int line = 0; line < BOARD_SIZE; ++line) {
        int score;

        if (board[line] != EMPTY)
            continue;

        board[line] = player;
        if (has_lost(board, player)) {
            score = -1;               /* this edge closes our own triangle   */
        } else if (is_full(board)) {
            /* Unreachable: by R(3,3) = 6 the edge just played closed one. */
            assert(0);
            score = -1;
        } else {
            score = -best_move(board, other_player(player)).score;
        }
        board[line] = EMPTY;

        if (best.line < 0 || score > best.score) {
            best.line = line;
            best.score = score;
            if (score == 1)
                break;                /* cannot do better than winning       */
        }
    }

    assert(best.line >= 0);
    computed_moves[o] = encode_move(best);
    return best;
}
