# Thread Snake

A simple auto-playing snake game for terminal written in C.

## Program description

### Command-line arguments

The program takes multiple command-line arguments: options modifying the default behaviour and at least one snake parameters. Default option values are given after equal sign:

./snake [-x xdim=20] [-y ydim=10] [-f file=$SNAKEFILE] c1:s1 [c2:s2 ...]

The snake parameters are in form ci:si where ci is the snake’s character (has to be uppercase and unique) and si is the snake’s speed in milliseconds. For instance, the user is able to create three snakes with different speed using the following invocation:

./snake A:500 B:1200 C:200

### Map

The main thread at the very beginning creates a 2-d array of characters with dimensions given via CLI (xdim, ydim) representing the game’s map and fills it with randomly placed food tiles. Empty tiles are just spaces. Food tiles are lowercase “o” characters. Tiles occupied by snake should contain the snake’s character – head in the uppercase, the rest in the lowercase. The number of food tiles and the number of snakes are always equal.

### Snake threads

For each snake parameter a snake thread is spawned, responsible for moving a single snake around the map. At the very beginning, the thread puts a 1-tile long snake at a random free tile. The thread always maintains a target tile – the one which the snake is moving towards. The target tile is always randomly chosen out of the set of the tiles containing food (at the moment of making choice). After reaching the target tile, a new target is chosen.
The snake thread moves the snake in a loop with delay interval defined by snake’s speed parameter. In each step the snake’s head moves one tile towards the target (in either the x or y axis and the tail follows. After stepping on a food tile the snake becomes longer, new food tile is generated in the snake’s thread and a new target is chosen. Snakes cannot move through the map boundary. They also cannot enter tiles occupied by other snakes.

### Runtime commands

The main thread after creating the snake threads reads commands from the standard input. It also reacts to some signals from the user.

When user types "spawn cn:sn" it spawns a new snake thread as if it was given by CLI arguments.

When user types "save" it shall save map to the file given by -f argument (in exactly the same textual format as described in show command).

When user types "exit" or sends SIGINT (C-c) via terminal the snake threads are cancelled and the program exits cleanly.

### How to build

Simply execute "make snake" command in the directory with the source files.



