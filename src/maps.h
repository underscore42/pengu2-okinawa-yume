/* maps.h - Pengu authored ice fields.
 *   #  ice block (pushable, crushable when jammed)
 *   T  palm trunk - immovable, uncrushable. Its canopy sits on scroll
 *      plane 2 and occludes VISUALLY ONLY; blocks and crabs pass under it.
 *   P  pineapple - walk over it for points, and the canopy lifts clear for
 *      a couple of seconds
 *   *  diamond block (pushable, never breaks)
 *   .  open ice
 *   @  penguin start
 *
 * 10 cells wide x 9 tall.  Edit freely; init_level() scans for '@'.
 *
 * Tuned to ~26% density - the arcade's ratio.  Denser and the longest slide
 * drops to 2-3 cells, which kills the shove.  Every field is validated by
 * tools/checkmaps.py: reachability, slide lengths, jammed count.
 */
#ifndef MAPS_H
#define MAPS_H

static const char ice_maps[NUM_LEVELS][CELLS_H][CELLS_W + 1] = {
    /* 1 - BEACH: open, one palm, learn the shove */
    {
        "..#....#..",
        ".#..P..#..",
        "..*..#....",
        ".#..T...#.",
        "...#...*#.",
        ".#....#..#",
        "...*..P...",
        ".#..#...#.",
        "@..#....#."
    },
    /* 2 - PALM GROVE: two palms, canopies overlap the lanes */
    {
        ".#.#..#.#.",
        "..P....P..",
        "#.#.T.#.#.",
        "..........",
        ".#.*..*.#.",
        "..........",
        "#.#.T.#.#.",
        "..P....P..",
        "@#.#..#.#*"
    },
    /* 3 - DUSK: three palms, dead ground across the middle */
    {
        "..........",
        ".#.#..#.#.",
        "..T....T..",
        "#.#.*#.#.#",
        "....##....",
        "#.#.*#.#.#",
        "..P..T.P..",
        ".#.#..#.#.",
        "@........*"
    },
    /* 4 - NIGHT BEACH: four palms, most cover in the game */
    {
        "#..#..#..#",
        "...T..T...",
        ".#..##..#.",
        "..*.P..*..",
        "#...##...#",
        "..*....*..",
        ".#..##..#.",
        "...T..T...",
        "@..#..#..#"
    }
};

/* Bonus round field - whack-a-orca in the rock pools.
 *   o  hole through the ice (a shoved block drops in)
 *   #  loose ice block, your ammunition
 *
 * Six holes, twelve blocks. Every hole is reachable by a block from at least
 * two directions - validated by tools/checkmaps.py, because a hole that can
 * only be hit one way makes the perfect clear impossible if you waste that
 * block. */
static const char bonus_map[CELLS_H][CELLS_W + 1] = {
    "..........",
    ".o#.o.#o..",
    "..........",
    ".#..#..#..",
    "...#..#...",
    ".#..#..#..",
    "..........",
    ".o#.o.#o..",
    "@........."
};

/* Per level: initial hatches, then how many mid-round hatches are allowed.
 * Level 1 starts with two Sno-Bees so the first round is learnable. */
static const u8 lvl_start_bees[NUM_LEVELS] = { 2, 3, 3, 4 };

#endif
