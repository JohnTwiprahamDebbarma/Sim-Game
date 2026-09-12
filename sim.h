/*
 * The Game of Sim -- solver engine.
 *
 * Two players alternately color the 15 edges of K6 (the complete graph on six
 * vertices).  Red moves first.  A player who completes a triangle in their own
 * color loses immediately.
 *
 * Sim cannot be drawn: Ramsey's theorem gives R(3,3) = 6, so every 2-coloring
 * of K6 contains a monochromatic triangle.  Exhaustive search confirms the
 * classical result that Sim is a second-player win.
 *
 * This header is the engine only -- it performs no I/O.
 */
#ifndef SIM_H
#define SIM_H

#include <stdbool.h>

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

typedef struct {
    int line;
    int score;                        /* -1 loss, 0 draw, +1 win, for the
                                         player to move                      */
} move_t;

void     init_board(board_t board);
bool     is_full(board_t board);
player_t other_player(player_t player);

/* Index of the edge joining vertices u and v (0-based, u != v). */
int      edge_index(int u, int v);

/*
 * The three edges of each triangle, derived from the vertex triples by
 * sim_init() rather than written out by hand.
 */
extern int triangle[NTRIANGLE][3];

/* Build the triangle, edge and index tables.  Call once, before anything else. */
void     sim_init(void);

bool     has_lost(board_t board, player_t player);

/* Best move for `player`, who must be to move on an unfinished board. */
move_t   best_move(board_t board, player_t player);

/* Memo table introspection, for tests and benchmarks. */
long     memo_entries(void);
void     reset_memo(void);

#endif /* SIM_H */
