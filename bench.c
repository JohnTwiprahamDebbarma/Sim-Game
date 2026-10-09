/*
 * Solve Sim from the empty board and report the cost.  Built with assertions
 * enabled, so the numbers are conservative.
 */
#include "sim.h"

#include <stdio.h>
#include <time.h>

int main(void)
{
    board_t board;
    clock_t start;
    move_t m;
    double secs;

    init_triangles();
    init_board(board);

    start = clock();
    m = best_move(board, RED);
    secs = (double)(clock() - start) / CLOCKS_PER_SEC;

    printf("opening value     : %+d  (%s)\n", m.score,
           m.score == -1 ? "second player wins" : "UNEXPECTED");
    printf("first move        : edge %d\n", m.line);
    printf("positions memoized: %ld\n", memo_entries());
    printf("solve time        : %.3f s\n", secs);
    return m.score != -1;
}
