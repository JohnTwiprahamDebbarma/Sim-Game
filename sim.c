/*
 * The Game of Sim -- solver engine.  See sim.h.
 *
 * A position is two 15-bit masks, one per color, so a triangle test is a
 * mask-and-compare.  Only the four triangles through the edge just played can
 * have been closed by it, so a move costs four tests rather than twenty.
 */
#include "sim.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef uint16_t mask_t;              /* one bit per edge                    */

#define FULL_MASK ((mask_t)((1u << BOARD_SIZE) - 1))
#define BIT(e)    ((mask_t)(1u << (e)))

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

int edge_index(int u, int v)
{
    int lo = u < v ? u : v;
    int hi = u < v ? v : u;

    assert(0 <= lo && lo < hi && hi < NVERT);
    return (NVERT - 1) * lo - lo * (lo - 1) / 2 + (hi - lo - 1);
}

/* ------------------------------------------------------------------ tables */

int triangle[NTRIANGLE][3];

/* The two vertices of each edge, for inducing edge maps from vertex maps. */
static uint8_t edge_ends[BOARD_SIZE][2];

/* The 20 triangles as edge masks. */
static mask_t triangle_mask[NTRIANGLE];

/*
 * The triangles through each edge.  Fixing an edge {u,v}, a triangle on it is
 * named by its third vertex, so there are exactly NVERT - 2 of them.
 */
#define TRI_PER_EDGE (NVERT - 2)
static mask_t edge_triangle[BOARD_SIZE][TRI_PER_EDGE];

/*
 * ord_tab[m] is the sum of 3^e over the edges in m, so a position's index is
 * ord_tab[red] + 2 * ord_tab[blue] -- two lookups instead of a 15-step loop.
 */
static uint32_t ord_tab[1u << BOARD_SIZE];

#if SIM_SYMMETRY
static void build_symmetry(void);
#endif

void sim_init(void)
{
    int n = 0;
    int per_edge[BOARD_SIZE];
    uint32_t pow3[BOARD_SIZE];

    for (int a = 0; a < NVERT; ++a)
        for (int b = a + 1; b < NVERT; ++b)
            for (int c = b + 1; c < NVERT; ++c) {
                triangle[n][0] = edge_index(a, b);
                triangle[n][1] = edge_index(a, c);
                triangle[n][2] = edge_index(b, c);
                triangle_mask[n] = BIT(triangle[n][0]) | BIT(triangle[n][1]) |
                                   BIT(triangle[n][2]);
                ++n;
            }
    assert(n == NTRIANGLE);

    for (int e = 0; e < BOARD_SIZE; ++e)
        per_edge[e] = 0;
    for (int t = 0; t < NTRIANGLE; ++t)
        for (int i = 0; i < 3; ++i) {
            int e = triangle[t][i];
            assert(per_edge[e] < TRI_PER_EDGE);
            edge_triangle[e][per_edge[e]++] = triangle_mask[t];
        }
    for (int e = 0; e < BOARD_SIZE; ++e)
        assert(per_edge[e] == TRI_PER_EDGE);

    for (int u = 0; u < NVERT; ++u)
        for (int v = u + 1; v < NVERT; ++v) {
            edge_ends[edge_index(u, v)][0] = (uint8_t)u;
            edge_ends[edge_index(u, v)][1] = (uint8_t)v;
        }

    pow3[0] = 1;
    for (int e = 1; e < BOARD_SIZE; ++e)
        pow3[e] = pow3[e - 1] * 3;
    ord_tab[0] = 0;
    for (uint32_t m = 1; m < (1u << BOARD_SIZE); ++m) {
        uint32_t low = m & (~m + 1u);             /* lowest set bit          */
        int e = 0;
        while ((low >> e) != 1u) ++e;
        ord_tab[m] = pow3[e] + ord_tab[m & (m - 1)];
    }

#if SIM_SYMMETRY
    build_symmetry();
#endif
}

#if SIM_SYMMETRY
/*
 * Relabelling the six dots does not change the game, so positions related by a
 * vertex permutation share a value.  Reducing each position to the least
 * member of its orbit under S6 collapses the table by the size of that orbit,
 * at the cost of a scan over the group at every node.
 */
#define NPERM 720                     /* |S6| = 6!                           */

static uint8_t edge_map[NPERM][BOARD_SIZE];
static uint8_t edge_unmap[NPERM][BOARD_SIZE];

/* Split lookup, so permuting a mask is two loads instead of fifteen tests. */
static mask_t perm_lo[NPERM][1u << 8];
static mask_t perm_hi[NPERM][1u << (BOARD_SIZE - 8)];

static int perm_count;
static uint8_t vertex_map[NPERM][NVERT];

static void gen_perms(int *cur, bool *used, int depth)
{
    if (depth == NVERT) {
        for (int i = 0; i < NVERT; ++i)
            vertex_map[perm_count][i] = (uint8_t)cur[i];
        ++perm_count;
        return;
    }
    for (int v = 0; v < NVERT; ++v)
        if (!used[v]) {
            used[v] = true;
            cur[depth] = v;
            gen_perms(cur, used, depth + 1);
            used[v] = false;
        }
}

static void build_symmetry(void)
{
    int cur[NVERT];
    bool used[NVERT] = { false };

    perm_count = 0;
    gen_perms(cur, used, 0);
    assert(perm_count == NPERM);

    for (int p = 0; p < NPERM; ++p) {
        for (int e = 0; e < BOARD_SIZE; ++e) {
            int u = vertex_map[p][edge_ends[e][0]];
            int v = vertex_map[p][edge_ends[e][1]];
            edge_map[p][e] = (uint8_t)edge_index(u, v);
        }
        for (int e = 0; e < BOARD_SIZE; ++e)
            edge_unmap[p][edge_map[p][e]] = (uint8_t)e;

        for (unsigned lo = 0; lo < (1u << 8); ++lo) {
            mask_t m = 0;
            for (int b = 0; b < 8; ++b)
                if (lo & (1u << b)) m |= BIT(edge_map[p][b]);
            perm_lo[p][lo] = m;
        }
        for (unsigned hi = 0; hi < (1u << (BOARD_SIZE - 8)); ++hi) {
            mask_t m = 0;
            for (int b = 0; b < BOARD_SIZE - 8; ++b)
                if (hi & (1u << b)) m |= BIT(edge_map[p][8 + b]);
            perm_hi[p][hi] = m;
        }
    }
}

static mask_t permute(int p, mask_t m)
{
    return (mask_t)(perm_lo[p][m & 0xFFu] | perm_hi[p][m >> 8]);
}

/*
 * Reduce to the least (red, blue) pair in the orbit.  Returns the permutation
 * that gets there, so the stored move can be mapped back into this position's
 * own labelling.
 */
static int canonicalize(mask_t red, mask_t blue, mask_t *cred, mask_t *cblue)
{
    uint32_t best = 0xFFFFFFFFu;
    int bestp = 0;

    for (int p = 0; p < NPERM; ++p) {
        mask_t r = permute(p, red);
        uint32_t key;

        if ((uint32_t)r > (best & FULL_MASK))
            continue;                 /* cannot win on the red half          */
        key = (uint32_t)r | ((uint32_t)permute(p, blue) << BOARD_SIZE);
        if (key < best) {
            best = key;
            bestp = p;
        }
    }
    *cred  = (mask_t)(best & FULL_MASK);
    *cblue = (mask_t)(best >> BOARD_SIZE);
    return bestp;
}
#endif /* SIM_SYMMETRY */

static mask_t to_mask(board_t board, player_t player)
{
    mask_t m = 0;

    for (int e = 0; e < BOARD_SIZE; ++e)
        if (board[e] == player)
            m |= BIT(e);
    return m;
}

/* True if `mine` contains a triangle through edge `e`. */
static bool closes_triangle(mask_t mine, int e)
{
    for (int i = 0; i < TRI_PER_EDGE; ++i)
        if ((mine & edge_triangle[e][i]) == edge_triangle[e][i])
            return true;
    return false;
}

bool has_lost(board_t board, player_t player)
{
    mask_t mine = to_mask(board, player);

    for (int t = 0; t < NTRIANGLE; ++t)
        if ((mine & triangle_mask[t]) == triangle_mask[t])
            return true;
    return false;
}

/* ----------------------------------------------------------------- solver */

#if SIM_SYMMETRY

/*
 * Reduced to orbit representatives the reachable set is small, so a direct
 * table indexed by position would be almost entirely holes.  Open addressing
 * with linear probing over a few thousand slots holds it in a fraction of the
 * space.
 */
/*
 * Solving from the empty board reaches 3112 orbit representatives, and every
 * position the engine can be asked about lies in that tree, so 16384 slots
 * keep the load factor under a fifth.  memo_put() asserts the bound; probing
 * assumes the table never fills.
 */
#define MEMO_BITS  14
#define MEMO_SLOTS (1u << MEMO_BITS)
#define MEMO_EMPTY 0xFFFFFFFFu

static struct {
    uint32_t key;
    uint8_t  val;
} memo[MEMO_SLOTS];

static long memo_used;
static bool memo_ready;

static uint32_t memo_probe(uint32_t key)
{
    uint32_t i = (key * 2654435761u) >> (32 - MEMO_BITS);   /* Knuth */

    while (memo[i].key != key && memo[i].key != MEMO_EMPTY)
        i = (i + 1u) & (MEMO_SLOTS - 1u);
    return i;
}

void reset_memo(void)
{
    memset(memo, 0xFF, sizeof memo);  /* every key becomes MEMO_EMPTY        */
    memo_used = 0;
    memo_ready = true;
}

static bool memo_get(long o, uint8_t *out)
{
    uint32_t i;

    if (!memo_ready)
        reset_memo();
    i = memo_probe((uint32_t)o);
    if (memo[i].key == MEMO_EMPTY)
        return false;
    *out = memo[i].val;
    return true;
}

static void memo_put(long o, uint8_t v)
{
    uint32_t i = memo_probe((uint32_t)o);

    if (memo[i].key == MEMO_EMPTY) {
        memo[i].key = (uint32_t)o;
        ++memo_used;
        /* Probing degrades badly past about three quarters full. */
        assert((unsigned long)memo_used < MEMO_SLOTS - MEMO_SLOTS / 4);
    }
    memo[i].val = v;
}

long memo_entries(void)
{
    return memo_used;
}

#else /* direct table */

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

static bool memo_get(long o, uint8_t *out)
{
    if (!computed_moves[o])
        return false;
    *out = computed_moves[o];
    return true;
}

static void memo_put(long o, uint8_t v)
{
    computed_moves[o] = v;
}

#endif /* SIM_SYMMETRY */

static long ord(mask_t red, mask_t blue)
{
    return (long)ord_tab[red] + 2L * (long)ord_tab[blue];
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
static int popcount15(mask_t m)
{
    int n = 0;

    while (m) { m &= (mask_t)(m - 1); ++n; }
    return n;
}
#endif

/*
 * Negamax: the value of a position is the negation of the best the opponent
 * can do in reply.
 *
 * Positions are memoized by the two masks alone.  That is sound only because
 * Red moves first and the players strictly alternate, which makes the side to
 * move a function of how many edges each has taken.
 */
static move_t solve(mask_t red, mask_t blue, bool red_to_move)
{
    move_t best = { .line = -1, .score = -1 };
    mask_t occupied = (mask_t)(red | blue);
    uint8_t stored;
    long o;
#if SIM_SYMMETRY
    mask_t cred, cblue;
    int p = canonicalize(red, blue, &cred, &cblue);

    o = ord(cred, cblue);
#else
    o = ord(red, blue);
#endif

    assert(red_to_move == (popcount15(red) == popcount15(blue)));

    if (memo_get(o, &stored)) {
        move_t m = decode_move(stored);
#if SIM_SYMMETRY
        m.line = edge_unmap[p][m.line];   /* back into this position's frame */
#endif
        return m;
    }

    for (int e = 0; e < BOARD_SIZE; ++e) {
        mask_t mine, nred, nblue;
        int score;

        if (occupied & BIT(e))
            continue;

        mine  = (mask_t)((red_to_move ? red : blue) | BIT(e));
        nred  = red_to_move ? mine : red;
        nblue = red_to_move ? blue : mine;

        if (closes_triangle(mine, e)) {
            score = -1;               /* this edge closes our own triangle   */
        } else if ((nred | nblue) == FULL_MASK) {
            /* Unreachable: by R(3,3) = 6 the edge just played closed one. */
            assert(0);
            score = -1;
        } else {
            score = -solve(nred, nblue, !red_to_move).score;
        }

        if (best.line < 0 || score > best.score) {
            best.line = e;
            best.score = score;
            if (score == 1)
                break;                /* cannot do better than winning       */
        }
    }

    assert(best.line >= 0);
#if SIM_SYMMETRY
    {
        move_t canon = best;

        canon.line = edge_map[p][best.line];   /* store in canonical frame   */
        memo_put(o, encode_move(canon));
    }
#else
    memo_put(o, encode_move(best));
#endif
    return best;
}

move_t best_move(board_t board, player_t player)
{
    assert(player == RED || player == BLUE);
    return solve(to_mask(board, RED), to_mask(board, BLUE), player == RED);
}
