#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#define BOARD_SIZE (15)
/*
 * The status of a line.
 */
enum {
    NO = 0, /* No line */
    RED = 1,
    BLUE = 2
};

/*
 * The board records the colors of the lines.
 * board[0] = color of 12
 * board[1] = color of 13
 * ...
 * board[14] = color of 56
 */
typedef char player_t;
typedef char board_t[BOARD_SIZE];

void init_board(board_t board)
{
    for (int line = 0; line < BOARD_SIZE; ++line) {
            board[line] = '.';
    }
}


void print_board(board_t board)
{
    for (int line = 0; line < BOARD_SIZE; ++line) {
            printf("%3c ", board[line]);}
    printf("\n");
}

int is_full(board_t board)
{
    for (int line = 0; line < BOARD_SIZE; ++line) {
            if (board[line] == '.')
                { return 0;}
    }
    return 1;
}

bool has_lost(board_t board, player_t player){
if ((board[0] == player && board[1] == player && board[5] == player) ||
        (board[0] == player && board[2] == player && board[6] == player) ||
        (board[0] == player && board[3] == player && board[7] == player) ||
        (board[0] == player && board[4] == player && board[8] == player) ||
        (board[1] == player && board[2] == player && board[9] == player) ||
        (board[1] == player && board[3] == player && board[10] == player) ||
        (board[1] == player && board[4] == player && board[11] == player) ||
        (board[2] == player && board[3] == player && board[12] == player) ||
        (board[2] == player && board[4] == player && board[13] == player) ||
        (board[3] == player && board[4] == player && board[14] == player) ||
        (board[5] == player && board[6] == player && board[9] == player) ||
        (board[5] == player && board[7] == player && board[10] == player) ||
        (board[5] == player && board[8] == player && board[11] == player) ||
        (board[6] == player && board[7] == player && board[12] == player) ||
        (board[6] == player && board[8] == player && board[13] == player) ||
        (board[7] == player && board[8] == player && board[14] == player) ||
        (board[9] == player && board[10] == player && board[12] == player) ||
        (board[9] == player && board[13] == player && board[11] == player) ||
        (board[10] == player && board[11] == player && board[14] == player) ||
        (board[12] == player && board[13] == player && board[14] == player)) {
        return true;
    }
    return false;
}

player_t other_player(player_t player)
{
    switch (player) {
    case 'R': return 'B';
    case 'B': return 'R';
    default: assert(0);
    }
}

bool has_won(board_t board, player_t player)
{
    // bool losable_edges=true;
    // player_t op = other_player(player);
    // for (int line = 0; line < BOARD_SIZE; ++line) {
    //     if (board[line] == '.'){
    //         board[line] = player;
    //         if ((!(has_lost(board, op))&&(!(has_lost(board, op))))){
    //             losable_edges=false;}
    //         board[line]='.';
    //         }
    // }
    // return losable_edges;
    player_t op = other_player(player);
    if (has_lost(board,op)){
        return true;
    }
    return false;
}

typedef struct {
    int line;
    /* -1 for a loss, 0 for a draw, 1 for a win. */
    int score;
} move_t;

#define MAX_ORD (43046720)

uint8_t computed_moves[MAX_ORD+1];

uint8_t encode_move(move_t m)
{
    uint8_t b = 0;

    assert(0 <= m.line && m.line <= 14);
    b |= m.line;
    switch (m.score) {
    case -1: b |= 1 << 6; break;
    case 0: b |= 1 << 5; break;
    case 1: b |= 1 << 4; break;
    }

    return b;
}

move_t decode_move(uint8_t b)
{
    move_t m;
    m.line = b & 0xf;
    if (b & 0x10) m.score = 1;
    if (b & 0x20) m.score = 0;
    if (b & 0x40) m.score = -1;
    return m;
}

int ord1(board_t board)
{
    int p = 1;
    int i = 0;
    int d;

    for (int line = 0; line < BOARD_SIZE; ++line) {
            switch (board[line]) {
            case 'R': d = 1; break;
            case 'B': d = 2; break;
            case '.': d = 0; break;
            }
            i += d * p;
            p *= 3;
    }

    return i;
}

/*
 * board should be an unfinished game.
 */
move_t best_move1(board_t board, player_t player)
{
    move_t response;
    move_t candidate;
    int no_candidate = 1;

    int o = ord1(board);

    if (computed_moves[o]) {
        return decode_move(computed_moves[o]);
    }

    for (int line = 0; line < BOARD_SIZE; ++line) {
            if (board[line] == '.') {
                board[line] = player;
                if (has_won(board, player)) {
                    board[line] = '.';
                    computed_moves[o] = encode_move(candidate = (move_t) {
                        .line = line,
                        .score = 1
                        });
                    return candidate;
                }
                board[line] = '.';
            }
    }

    for (int line = 0; line < BOARD_SIZE; ++line) {
            if (board[line] == '.') {
                board[line] = player;
                if (is_full(board)) {
                    board[line] = '.';
                    computed_moves[o] = encode_move(candidate = (move_t) {
                        .line = line,
                        .score = 0
                        });
                    return candidate;
                }
                response = best_move1(board, other_player(player));
                board[line] = '.';
                if (response.score == -1) {
                    computed_moves[o] = encode_move(candidate = (move_t) {
                        .line = line,
                        .score = 1
                        });
                    return candidate;
                } else if (response.score == 0) {
                    candidate = (move_t) {
                        .line = line,
                        .score = 0
                    };
                    no_candidate = 0;
                } else { /* response.score == +1 */
                    if (no_candidate) {
                        candidate = (move_t) {
                            .line = line,
                            .score = -1
                        };
                        no_candidate = 0;
                    }
                }
            }
    }
    computed_moves[o] = encode_move(candidate);
    return candidate;
}






int ord2(board_t board)
{
    int p = 1;
    int i = 0;
    int d;

    for (int line = 0; line < BOARD_SIZE; ++line) {
            switch (board[line]) {
            case 'B': d = 1; break;
            case 'R': d = 2; break;
            case '.': d = 0; break;
            }
            i += d * p;
            p *= 3;
    }

    return i;
}

/*
 * board should be an unfinished game.
 */
move_t best_move2(board_t board, player_t player)
{
    move_t response;
    move_t candidate;
    int no_candidate = 1;

    int o = ord2(board);

    if (computed_moves[o]) {
        return decode_move(computed_moves[o]);
    }

    for (int line = 0; line < BOARD_SIZE; ++line) {
            if (board[line] == '.') {
                board[line] = player;
                if (has_won(board, player)) {
                    board[line] = '.';
                    computed_moves[o] = encode_move(candidate = (move_t) {
                        .line = line,
                        .score = 1
                        });
                    return candidate;
                }
                board[line] = '.';
            }
    }

    for (int line = 0; line < BOARD_SIZE; ++line) {
            if (board[line] == '.') {
                board[line] = player;
                if (is_full(board)) {
                    board[line] = '.';
                    computed_moves[o] = encode_move(candidate = (move_t) {
                        .line = line,
                        .score = 0
                        });
                    return candidate;
                }
                response = best_move2(board, other_player(player));
                board[line] = '.';
                if (response.score == -1) {
                    computed_moves[o] = encode_move(candidate = (move_t) {
                        .line = line,
                        .score = 1
                        });
                    return candidate;
                } else if (response.score == 0) {
                    candidate = (move_t) {
                        .line = line,
                        .score = 0
                    };
                    no_candidate = 0;
                } else { /* response.score == +1 */
                    if (no_candidate) {
                        candidate = (move_t) {
                            .line = line,
                            .score = -1
                        };
                        no_candidate = 0;
                    }
                }
            }
    }
    computed_moves[o] = encode_move(candidate);
    return candidate;
}





void print_key()
{
    int i = 0;
    for (int line = 0; line < BOARD_SIZE; ++line) {
                printf("%3d ", i++);
    }
    printf("\n");
}


int main()
{
    int move, line;
    board_t board;
    move_t response;
    player_t current = 'R';
    printf("Welcome to Game of Sim\nEnter 1 if you are the first (Red) player and 2 otherwise (Blue):");
    int order;
    scanf("%d", &order);
    if(order==1){
    init_board(board);
    while (1) {
        print_board(board);
        print_key();
        printf("\n\n");
        if (current == 'R') {
            printf("Enter your move: ");
            scanf("%d", &move);
            line = move;
            if(board[line] != '.'){
                printf("Invalid Move\n");
                continue;
            };
            board[line] = current;
        } else {
            printf("Computer's Move.......\n");
            response = best_move1(board, current);
            board[response.line] = current;
        }
        if (has_won(board, current)) {
            print_board(board);
            print_key();
            printf("\n\n");
//            printf("Player %c has won!\n", current);
//            printf("Computer was B and you are R");
            if (current=='R'){
                printf("Congratulations, You Have Won\nComputer Has Lost");
            }
            else if(current=='B'){
                printf("Sadly, You Have Lost\nComputer Has Won");
            }
            else{
                printf("SOME ERROR IS THERE");
            }
            break;
        }
        else if (has_lost(board, current)){
            print_board(board);
            printf("\n\n");
//            printf("Player %c has lost!\n", current);
//            printf("Computer was B and you are R");
            if (current=='R'){
                printf("Sadly, You Have Lost\nComputer Has Won");
            }
            else if(current=='B'){
                printf("Congratulations, You Have Won\nComputer Has Lost");
            }
            else{
                printf("SOME ERROR IS THERE");
            }
            break;}
         else if (is_full(board)) {
            print_board(board);
            printf("\n\n");
            printf("Draw.\n");
            break;
        }
        current = other_player(current);
    }
    }
else{
    init_board(board);
    board[0]='R';
    current = 'B';
    while (1) {
        print_board(board);
        print_key();
        printf("\n\n");

        if (current == 'B') {
            printf("Enter your move: ");
            scanf("%d", &move);
            line = move;
            if(board[line] != '.'){
                printf("Invalid Move\n");
                continue;
            };
            board[line] = current;
        } else {
            printf("Computer's Move.......\n");
            response = best_move2(board, current);
            board[response.line] = current;
        }
        if (has_won(board, current)) {
            print_board(board);
            print_key();
            printf("\n\n");
//            printf("Player %c has won!\n", current);
//            printf("Computer was R and you are B");
            if (current=='R'){
                printf("Sadly, You Have Lost\nComputer Has Won");
            }
            else if(current=='B'){
                printf("Congartulations, You Have Won\nComputer Has Lost");
            }
            else{
                printf("SOME ERROR IS THERE");
            }
            break;}

        else if (has_lost(board, current)){
            print_board(board);
            print_key();
            printf("\n\n");
//            printf("Player %c has lost!\n", current);
//            printf("Computer was R and you are B");
            if (current=='R'){
                printf("Congratulations, You Have Won\nComputer Has Lost");
            }
            else if(current=='B'){
                printf("Sadly, You Have Lost\nComputer Has Won");
            }
            else{
                printf("SOME ERROR IS THERE");
            }
            break;}

        else if (is_full(board)) {
            print_board(board);
            printf("\n\n");
            printf("Draw.\n");
            break;
        }
        current = other_player(current);
    }
    return 0;
}}