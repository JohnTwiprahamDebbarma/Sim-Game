/*
 * The Game of Sim -- interactive front end.  All I/O lives here; sim.c is the
 * pure engine.
 */
#include "sim.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * The board as an upper-triangular matrix over the six vertices.  A cell holds
 * the color of that edge once played, and until then the number to type to
 * play it, so no separate key is needed.
 */
static void print_board(board_t board)
{
    printf("\n       ");
    for (int v = 1; v < NVERT; ++v)
        printf("%4d ", v + 1);
    printf("\n\n");

    for (int u = 0; u < NVERT - 1; ++u) {
        printf("  %2d   ", u + 1);
        for (int v = 1; v < NVERT; ++v) {
            if (v <= u)
                printf("     ");
            else if (board[edge_index(u, v)] == EMPTY)
                printf("%4d ", edge_index(u, v));
            else
                printf("%4c ", board[edge_index(u, v)]);
        }
        printf("\n");
    }
    printf("\n");
}

/*
 * Read one line and parse it as a whole number.  Returns 1 and sets *out when
 * the line is exactly one integer that fits in an int, 0 when it is anything
 * else, and EOF at end of input.
 *
 * Not scanf("%d"): a number too large for an int is undefined behavior there,
 * and in practice it wraps -- 4294967301 would be read as 5, a real edge.
 * strtol reports overflow instead.
 */
static int read_number(int *out)
{
    char text[32];
    size_t len = 0;
    int ch, too_long = 0;
    char *end;
    long value;

    while ((ch = getchar()) != EOF && ch != '\n') {
        if (len + 1 < sizeof text)
            text[len++] = (char)ch;
        else
            too_long = 1;
    }
    if (ch == EOF && len == 0 && !too_long)
        return EOF;
    text[len] = '\0';
    if (too_long || memchr(text, '\0', len) != NULL)
        return 0;

    errno = 0;
    value = strtol(text, &end, 10);
    if (end == text || errno == ERANGE || value < INT_MIN || value > INT_MAX)
        return 0;
    while (isspace((unsigned char)*end))
        ++end;
    if (*end != '\0')
        return 0;                     /* trailing characters, as in "5abc"   */

    *out = (int)value;
    return 1;
}

/*
 * Read one move.  Returns false at end of input.  A rejected move sets *line
 * to -1, leaving the caller to prompt again.
 */
static bool read_move(board_t board, int *line)
{
    int move, rc;

    printf("Enter your move: ");
    rc = read_number(&move);
    if (rc == EOF) {
        printf("\nInput ended; exiting.\n");
        return false;
    }

    *line = -1;
    if (rc == 0) {
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

    printf("Welcome to Game of Sim\n\n"
           "Color edges of the complete graph on six dots.  Complete a triangle\n"
           "in your own color and you lose.  Red moves first.\n\n"
           "Enter 1 if you are the first (Red) player and 2 otherwise (Blue): ");
    if (read_number(&order) != 1) {
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
            if (current == human)
                printf("Sadly, You Have Lost\nComputer Has Won\n");
            else
                printf("Congratulations, You Have Won\nComputer Has Lost\n");
            break;
        }

        /* R(3,3) = 6 forbids the board filling with the game undecided. */
        if (is_full(board)) {
            fprintf(stderr, "internal error: full board with no loser\n");
            return 2;
        }
        current = other_player(current);
    }
    return 0;
}
