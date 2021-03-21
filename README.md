sgtpuzzles-extended
===================
![puzzles](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/puzzles.png)

Extensions/Modifications I wrote for the famous & excellent [Simon Tathams Portable Puzzle Collection](http://www.chiark.greenend.org.uk/~sgtatham/puzzles/)

## Extensions:

* Solo: Additional keyboard controls to support game solving like used in some other sudoku apps
    * Pressing '+' automatically sets all possible game clues
    * Pressing '-' removes all impossible game clues
    * Pressing a number while no grid cell is selected highlights all grid cells with that number. Left/Right click on a clue in this highlight mode sets the number / removes the clue.
* Creek: Path-finding puzzle, implementation of [Creek](http://www.janko.at/Raetsel/Creek/index.htm)
    * Fully implemented and playable
* Undead:
    * More sophisticated puzzle generator
    * Four difficulty levels "Easy", "Normal", "Tricky", "Hard"
    * New option "Remove clues", similiar to the option in the 'Magnets' puzzle
    * Show count of placed monsters
* Unequal:
    * New puzzle variation: "Kropki", implementation of [Kropki](http://wiki.logic-masters.de/index.php?title=Kropki/de)
* Walls: New puzzle "Walls", find a path through a maze of walls
    * Playable. Needs still lots of work.
* Unfinished/Stellar: New puzzle "Stellar", implementation of [Sternenhaufen](http://www.janko.at/Raetsel/Sternenhaufen/index.htm)
    * So far playable in difficulty "Normal".

## Usage:

You need a copy of the SGT Puzzle collection source code. Copy the source files from this repository into this build directory, and recompile.

For instructions how to compile the game collection, see the README accompanying the original puzzle collection.

## Utility examples

[Example code](https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/Examples) for the utility API.

## Android

Files for the [Android port by Chris Boyle](https://github.com/chrisboyle/sgtpuzzles)

Contains also the puzzles [boats, rome and salad](https://github.com/x-sheep/puzzles-unreleased) by Lennard Sprong

Updated to the current Android sgtpuzzles version (using Android Studio)

**Outdated**, won't compile with current Android version.

## LICENSE

Copyright (c) 2013-2021 Simon Tatham, Steffen Bauer

Portion copyright Richard Boulton, James Harvey, Mike Pinna, Jonas
Koelker, Dariusz Olszewski, Michael Schierl, Lambros Lambrou, Bernd
Schmidt, Lennard Sprong and Rogier Goossens.

Distributed under the MIT license, see LICENSE
