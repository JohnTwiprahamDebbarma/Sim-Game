/*
 * The Game of Sim, solved.
 *
 * Two players alternately color the 15 edges of K6 (the complete graph on six
 * vertices).  Red moves first.  A player who completes a triangle in their own
 * color loses immediately.
 *
 * Sim cannot be drawn: Ramsey's theorem gives R(3,3) = 6, so every 2-coloring
 * of K6 contains a monochromatic triangle.  Exhaustive search here confirms the
 * classical result that Sim is a second-player win.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define NVERT      6                  /* vertices: K6                        */
#define BOARD_SIZE 15                 /* edges: C(6,2)                       */
#define NTRIANGLE  20                 /* triangles: C(6,3)                   */

#define EMPTY '.'
#define RED   'R'
#define BLUE  'B'

/*
 * A board records the color of each edge.  Edges are numbered lexicographically
 * by their endpoints:
 *
 *    0:12   1:13   2:14   3:15   4:16   5:23   6:24   7:25
 *    8:26   9:34  10:35  11:36  12:45  13:46  14:56
 */
typedef char player_t;
typedef char board_t[BOARD_SIZE];

/* ------------------------------------------------------------------ board */

void init_board(board_t board)
{
    for (int line = 0; line < BOARD_SIZE; ++line)
        board[line] = EMPTY;
}

void print_board(board_t board)
{
    for (int line = 0; line < BOARD_SIZE; ++line)
        printf("%3c ", board[line]);
    printf("\n");
}

void print_key(void)
{
    for (int line = 0; line < BOARD_SIZE; ++line)
        printf("%3d ", line);
    printf("\n");
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

/* Index of the edge joining vertices u and v, in the numbering above. */
static int edge_index(int u, int v)
{
    int lo = u < v ? u : v;
    int hi = u < v ? v : u;

    assert(0 <= lo && lo < hi && hi < NVERT);
    return (NVERT - 1) * lo - lo * (lo - 1) / 2 + (hi - lo - 1);
}

/*
 * The three edges of each triangle.  Derived from the vertex triples rather
 * than written out by hand, so it cannot silently disagree with the numbering.
 */
static int triangle[NTRIANGLE][3];

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

typedef struct {
    int line;
    int score;                        /* -1 loss, 0 draw, +1 win, for the
                                         player to move                      */
} move_t;

/*
 * Every position is one base-3 digit per edge, so 3^15 entries suffice.  A
 * stored byte is nonzero, which is what marks the slot as computed.
 */
#define POSITIONS 14348907L           /* 3^15 */

static uint8_t computed_moves[POSITIONS];

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
 * Best move for `player`, to move on an unfinished board.  Negamax: the value
 * of a position is the negation of the best the opponent can do in reply.
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

/* ------------------------------------------------------------------- game */

/*
 * Read one move from the player.  Returns false at end of input.  A rejected
 * move sets *line to -1, leaving the caller to prompt again.
 */
static bool read_move(board_t board, int *line)
{
    int move, rc;

    printf("Enter your move: ");
    rc = scanf("%d", &move);
    if (rc == EOF) {
        printf("\nInput ended; exiting.\n");
        return false;
    }

    *line = -1;
    if (rc != 1) {                    /* not a number: discard the token     */
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF)
            { }
        printf("Invalid Move: please enter a number.\n");
    } else if (move < 0 || move >= BOARD_SIZE) {
        printf("Invalid Move: choose a line from 0 to %d.\n", BOARD_SIZE - 1);
    } else if (board[move] != EMPTY) {
        printf("Invalid Move: line %d is already colored.\n", move);
    } else {
        *line = move;
    }
    return true;
}

int main(void)
{
    board_t board;
    player_t human, current = RED;
    int order, line;

    init_triangles();

    printf("Welcome to Game of Sim\n"
           "Enter 1 if you are the first (Red) player and 2 otherwise (Blue): ");
    if (scanf("%d", &order) != 1) {
        printf("\nInput ended or was not a number; exiting.\n");
        return 1;
    }
    if (order != 1 && order != 2) {
        printf("Please enter 1 (Red, first) or 2 (Blue, second).\n");
        return 1;
    }
    human = (order == 1) ? RED : BLUE;

    init_board(board);
    for (;;) {
        print_board(board);
        print_key();
        printf("\n\n");

        if (current == human) {
            if (!read_move(board, &line))
                return 1;
            if (line < 0)
                continue;             /* rejected; ask again                 */
            board[line] = current;
        } else {
            printf("Computer's Move.......\n");
            board[best_move(board, current).line] = current;
        }

        if (has_lost(board, current)) {
            print_board(board);
            print_key();
            printf("\n\n");
            if (current == human)
                printf("Sadly, You Have Lost\nComputer Has Won\n");
            else
                printf("Congratulations, You Have Won\nComputer Has Lost\n");
            break;
        }

        assert(!is_full(board));      /* R(3,3) = 6 forbids an undecided fill */
        current = other_player(current);
    }
    return 0;
}
