/*
 * Tests for the Sim engine.  Built with assertions enabled, so the engine's
 * own internal invariants are exercised alongside these checks.
 */
#include "sim.h"

#include <stdio.h>
#include <stdlib.h>

static int checks, failures;

#define CHECK(cond, ...)                                                      \
    do {                                                                      \
        ++checks;                                                             \
        if (!(cond)) {                                                        \
            ++failures;                                                       \
            printf("    FAIL %s:%d: ", __FILE__, __LINE__);                   \
            printf(__VA_ARGS__);                                              \
            printf("\n");                                                     \
        }                                                                     \
    } while (0)

static void report(const char *name)
{
    printf("  %-46s %s\n", name, failures ? "FAIL" : "ok");
}

/* ------------------------------------------------------------------ tests */

static void test_edge_index_is_a_bijection(void)
{
    int seen[BOARD_SIZE] = { 0 };
    int n = 0;

    for (int u = 0; u < NVERT; ++u)
        for (int v = u + 1; v < NVERT; ++v) {
            int e = edge_index(u, v);
            CHECK(0 <= e && e < BOARD_SIZE, "edge (%d,%d) -> %d out of range", u, v, e);
            CHECK(!seen[e], "edge index %d produced twice", e);
            seen[e] = 1;
            ++n;
            CHECK(edge_index(v, u) == e, "edge_index not symmetric for (%d,%d)", u, v);
        }
    CHECK(n == BOARD_SIZE, "expected %d edges, got %d", BOARD_SIZE, n);
    report("edge_index is a bijection onto 0..14");
}

static void test_triangle_table_matches_reference(void)
{
    /* Transcribed from the hand-written clause chain this table replaced. */
    static const int reference[NTRIANGLE][3] = {
        {0,1,5},{0,2,6},{0,3,7},{0,4,8},{1,2,9},{1,3,10},{1,4,11},{2,3,12},
        {2,4,13},{3,4,14},{5,6,9},{5,7,10},{5,8,11},{6,7,12},{6,8,13},{7,8,14},
        {9,10,12},{9,13,11},{10,11,14},{12,13,14}
    };
    int matched[NTRIANGLE] = { 0 };

    for (int t = 0; t < NTRIANGLE; ++t) {
        int found = 0;
        for (int r = 0; r < NTRIANGLE && !found; ++r) {
            if (matched[r]) continue;
            int hits = 0;
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    if (triangle[t][i] == reference[r][j]) ++hits;
            if (hits == 3) { matched[r] = 1; found = 1; }
        }
        CHECK(found, "generated triangle %d {%d,%d,%d} is not in the reference table",
              t, triangle[t][0], triangle[t][1], triangle[t][2]);
    }
    report("generated triangles match the hand-written table");
}

/*
 * R(3,3) = 6: every 2-coloring of K6 contains a monochromatic triangle.  This
 * is why Sim cannot be drawn, and why filling the last edge always loses --
 * the property the search's terminal case depends on.
 */
static void test_no_coloring_avoids_a_monochromatic_triangle(void)
{
    long drawn = 0;

    for (int mask = 0; mask < (1 << BOARD_SIZE); ++mask) {
        board_t b;
        for (int e = 0; e < BOARD_SIZE; ++e)
            b[e] = (mask >> e) & 1 ? RED : BLUE;
        if (!has_lost(b, RED) && !has_lost(b, BLUE))
            ++drawn;
    }
    CHECK(drawn == 0, "%ld of %d full colorings had no monochromatic triangle",
          drawn, 1 << BOARD_SIZE);
    report("no full coloring of K6 avoids a mono triangle");
}

/*
 * The regression test for the bug this project started with: the search scored
 * a filled board as a draw, which made the opening evaluate to 0 instead of a
 * loss for the player to move.
 */
static void test_opening_is_a_second_player_win(void)
{
    board_t b;
    move_t m;

    init_board(b);
    m = best_move(b, RED);
    CHECK(m.score == -1, "opening scored %d, expected -1 (second player wins)", m.score);
    CHECK(0 <= m.line && m.line < BOARD_SIZE, "opening move %d is not a legal edge", m.line);
    report("opening evaluates to a second-player win");
}

/*
 * Filling the final edge always loses.  Enumerate every balanced 14-edge
 * position that is still undecided; Red is to move, and Red's only edge must
 * complete a red triangle.
 */
static void test_filling_the_last_edge_loses(void)
{
    long positions = 0, survived = 0;

    for (int empty = 0; empty < BOARD_SIZE; ++empty)
        for (int mask = 0; mask < (1 << (BOARD_SIZE - 1)); ++mask) {
            board_t b;
            int bit = 0, reds = 0;

            for (int e = 0; e < BOARD_SIZE; ++e) {
                if (e == empty) { b[e] = EMPTY; continue; }
                b[e] = (mask >> bit) & 1 ? RED : BLUE;
                if (b[e] == RED) ++reds;
                ++bit;
            }
            if (reds != 7) continue;                    /* not reachable      */
            if (has_lost(b, RED) || has_lost(b, BLUE))
                continue;                               /* already decided    */
            ++positions;
            b[empty] = RED;
            if (!has_lost(b, RED)) ++survived;
        }
    CHECK(positions > 0, "no undecided 14-edge positions were generated");
    CHECK(survived == 0, "%ld of %ld final moves did not lose", survived, positions);
    printf("      (%ld undecided 14-edge positions checked)\n", positions);
    report("filling the last edge always loses");
}

static void test_best_move_returns_a_legal_edge(void)
{
    board_t b;

    init_board(b);
    srand(7);
    for (int trial = 0; trial < 200; ++trial) {
        player_t cur = RED;
        init_board(b);
        for (;;) {
            move_t m = best_move(b, cur);
            CHECK(0 <= m.line && m.line < BOARD_SIZE, "illegal edge %d", m.line);
            CHECK(b[m.line] == EMPTY, "edge %d is already colored", m.line);
            b[m.line] = cur;
            if (has_lost(b, cur)) break;
            CHECK(!is_full(b), "board filled with no loser");
            cur = other_player(cur);
        }
    }
    report("best_move always returns an uncolored edge");
}

static void test_memo_hit_agrees_with_fresh_search(void)
{
    board_t b;
    move_t warm, cold;

    init_board(b);
    warm = best_move(b, RED);         /* served from the table by now         */
    reset_memo();
    cold = best_move(b, RED);         /* recomputed from scratch              */
    CHECK(warm.line == cold.line && warm.score == cold.score,
          "memo hit (%d,%d) disagrees with fresh search (%d,%d)",
          warm.line, warm.score, cold.line, cold.score);
    report("memoized result agrees with a fresh search");
}

/*
 * Sim is a second-player win, so a correct engine playing Blue must never lose
 * -- against any opponent, including a random one.
 */
static void test_engine_never_loses_as_second_player(void)
{
    int games = 2000, losses = 0, draws = 0;

    srand(12345);
    for (int g = 0; g < games; ++g) {
        board_t b;
        player_t cur = RED;

        init_board(b);
        for (;;) {
            if (cur == RED) {
                int empty[BOARD_SIZE], n = 0;
                for (int e = 0; e < BOARD_SIZE; ++e)
                    if (b[e] == EMPTY) empty[n++] = e;
                if (n == 0) { ++draws; break; }
                b[empty[rand() % n]] = RED;
            } else {
                b[best_move(b, BLUE).line] = BLUE;
            }
            if (has_lost(b, cur)) { if (cur == BLUE) ++losses; break; }
            if (is_full(b)) { ++draws; break; }
            cur = other_player(cur);
        }
    }
    CHECK(losses == 0, "engine lost %d of %d games as second player", losses, games);
    CHECK(draws == 0, "%d games were drawn, which Sim does not allow", draws);
    printf("      (%d games played)\n", games);
    report("engine is unbeatable as second player");
}

/* ------------------------------------------------------------------- main */

int main(void)
{
    struct { const char *name; void (*fn)(void); } tests[] = {
        { "edge_index",        test_edge_index_is_a_bijection },
        { "triangles",         test_triangle_table_matches_reference },
        { "ramsey",            test_no_coloring_avoids_a_monochromatic_triangle },
        { "opening",           test_opening_is_a_second_player_win },
        { "last edge",         test_filling_the_last_edge_loses },
        { "legal moves",       test_best_move_returns_a_legal_edge },
        { "memo",              test_memo_hit_agrees_with_fresh_search },
        { "unbeatable",        test_engine_never_loses_as_second_player },
    };
    int total_failures = 0, total_checks = 0;

    sim_init();
    printf("sim engine tests\n\n");
    for (size_t i = 0; i < sizeof tests / sizeof tests[0]; ++i) {
        checks = failures = 0;
        tests[i].fn();
        total_failures += failures;
        total_checks += checks;
    }
    printf("\n%d checks, %d failures\n", total_checks, total_failures);
    return total_failures != 0;
}
