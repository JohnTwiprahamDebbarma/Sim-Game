# Sim

A complete solver for the [Game of Sim](https://en.wikipedia.org/wiki/Sim_(game)),
in C.

[![CI](https://github.com/JohnTwiprahamDebbarma/Sim-Game/actions/workflows/ci.yml/badge.svg)](https://github.com/JohnTwiprahamDebbarma/Sim-Game/actions/workflows/ci.yml)

Six dots, and every pair joined by a line — the complete graph K&#8326;, fifteen
edges. Two players take turns coloring an uncolored line, one red, one blue.
Complete a triangle in **your own** color and you lose immediately. Only
triangles whose corners are dots count; lines crossing in the middle mean
nothing.

## The result

Sim is a **second-player win**. Red, moving first, loses against perfect play.
The solver proves this by exhaustive search:

```
$ make bench
--- plain ---
mode              : direct table
opening value     : -1  (second player wins)
first move        : edge 0
positions memoized: 112096
solve time        : 0.008 s
--- symmetry reduced ---
mode              : symmetry-reduced (720 relabellings of K6)
opening value     : -1  (second player wins)
first move        : edge 0
positions memoized: 3112
solve time        : 0.007 s
```

Both builds agree on the value; they differ only in how much of the state
space they have to store.

Sim also **cannot be drawn**. Ramsey's theorem gives R(3,3) = 6: every
2-coloring of K&#8326; contains a monochromatic triangle, so the board can never
fill with nobody having lost. `make test` checks this by brute force over all
2¹⁵ colorings.

## Build and play

```sh
make          # build ./sim
./sim
```

The board is an upper-triangular matrix over the six dots. A cell shows the
color once that line is taken, and until then the number you type to take it:

```
          2    3    4    5    6

   1      R    B    R    B    4
   2           R    6    7    8
   3                B   10   11
   4                    12   13
   5                         14
```

Here `1-2` is red and edge `4` — the line joining dots 1 and 6 — is still free.

## How it works

Negamax over the full game tree with a transposition table.

- **Representation.** A position is two 15-bit masks, one per color, so a
  triangle test is a mask-and-compare.
- **Terminal test.** A move loses if it closes a triangle in the mover's color.
  Only the four triangles *through the edge just played* can have been closed
  by it, so a move costs four tests rather than twenty. There is no draw case:
  by R(3,3) = 6, coloring the last edge always closes one.
- **Memo key.** The two masks alone, without whose turn it is. That is sound
  because Red moves first and play strictly alternates, so the side to move
  follows from how many edges each player has taken. The invariant is enforced
  by a debug-only assertion.
- **Position index.** Each edge is one base-3 digit, so a position is an
  integer below 3¹⁵, read out of a precomputed table in two lookups rather
  than a fifteen-step loop.
- **Triangles.** The twenty triples are derived from the six vertices at
  startup rather than written out, so the table cannot drift from the edge
  numbering. A test checks it against the hand-written table it replaced.

### Symmetry reduction

Relabelling the six dots does not change the game, so all 720 vertex
permutations of a position share its value. Building with `-DSIM_SYMMETRY=1`
reduces every position to the least member of its orbit before consulting the
table, and maps the stored move back into the position's own labelling on the
way out.

That collapses the reachable set from 112,096 positions to **3,112** — a factor
of 36 — which in turn lets a 16K-slot hash table replace the 13.7 MB direct
array. It is not free: scanning the group at every node costs about 1.8x in
search time. The reduction is in states and memory, not in wall clock.

The whole engine is `sim.c` and does no I/O; `main.c` is the front end.

## Performance

Solving from the empty board, `-O2`, Apple M-series:

| | positions | memory | time |
| --- | ---: | ---: | ---: |
| first working version | 1,918,464 | 41.1 MB | 0.34 s |
| correct terminal score | 216,673 | 41.1 MB | 0.031 s |
| negamax, table sized 3¹⁵ not 3¹⁶ | 112,096 | 13.7 MB | 0.023 s |
| bitboards, 4 triangle tests per move | 112,096 | 13.8 MB | 0.0042 s |
| symmetry-reduced, hashed | **3,112** | **0.81 MB** | 0.0078 s |

Overall that is 616x fewer positions and 51x less memory than the version this
project started from, and the first row was answering the wrong question.

`make bench` reproduces the last two rows.

## Tests

```sh
make check     # everything CI runs
make test      # engine unit tests
make cli       # malformed input against the front end
make asan      # both again under AddressSanitizer and UBSan
```

The engine tests assert the mathematical facts the search depends on, not just
that it runs:

| test | what it establishes |
| --- | --- |
| `edge_index` | the vertex-pair-to-edge map is a bijection onto 0..14 |
| `triangles` | the generated table matches the hand-written one exactly |
| `ramsey` | none of the 2¹⁵ full colorings avoids a mono triangle |
| `opening` | the empty board evaluates to a second-player win |
| `last edge` | in all 180 undecided 14-edge positions, the last move loses |
| `legal moves` | the engine never returns an occupied edge |
| `memo` | a memoized answer matches a search from a cleared table |
| `unbeatable` | the engine loses none of 2000 games as second player |

`make cli` feeds the front end out-of-range, negative, non-numeric, oversized
and truncated input under a watchdog. Every case must exit cleanly — no crash,
no hang, no sanitizer report.

## What the search taught me

The first working version reported the opening as a **draw**. That is not a
close call — Ramsey's theorem says a drawn game of Sim does not exist, so the
output was impossible before it was wrong.

The cause was the terminal case. When a player colored the final edge, the
search scored it 0 without asking whether that player had just closed their own
triangle — which, by R(3,3) = 6, they always had. Scoring it as a loss for the
mover is a two-character change.

The interesting part is what the bug did *not* do. It never picked a bad move:
across 8741 genuinely won positions, the buggy search's choice preserved the
win every time, and it still beat a random opponent 2000 out of 2000. Because
Sim has no draws, "not losing" and "winning" are the same set of moves, and the
*loss* labels were already correct — enough for perfect play even while the
win/draw labels were wrong. The bug corrupted the analysis and left the play
intact, which is exactly why testing by playing it would never have caught it.

Fixing it also cut the search from 1,918,464 positions to 216,673, since
phantom draws were blocking cutoffs. Rewriting the duplicated search as
negamax took it to 112,096.

## Reference

- Ramsey theory and R(3,3) = 6 — [Wikipedia](https://en.wikipedia.org/wiki/Ramsey%27s_theorem)
- Game of Sim — [Wikipedia](https://en.wikipedia.org/wiki/Sim_(game))
