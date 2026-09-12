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
opening value     : -1  (second player wins)
first move        : edge 0
positions memoized: 112096
solve time        : 0.034 s
```

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

- **Position encoding.** Each of the 15 edges is one base-3 digit (empty, red,
  blue), so a position is an integer below 3¹⁵ and the table is a flat 13.7 MB
  array indexed directly by it. No hashing, no collisions.
- **Memo key.** The board alone, without whose turn it is. That is sound
  because Red moves first and play strictly alternates, so the side to move
  follows from the number of colored edges. The invariant is enforced by a
  debug-only assertion.
- **Terminal test.** A move loses if it closes a triangle in the mover's color.
  There is no draw case: by R(3,3) = 6, coloring the last edge always closes
  one.
- **Triangles.** The twenty triples are derived from the six vertices at
  startup rather than written out, so the table cannot drift from the edge
  numbering. A test checks it against the hand-written table it replaced.

The whole engine is `sim.c` and does no I/O; `main.c` is the front end.

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
