sgtpuzzles-extended
===================

Extensions/Modifications I wrote for the famous & excellent [Simon Tathams Portable Puzzle Collection](http://www.chiark.greenend.org.uk/~sgtatham/puzzles/)

## Extensions:

* Solo: Additional keyboard controls to support game solving
    * Pressing '+' automatically sets all possible game clues
    * Pressing '-' removes all impossible game clues
    * Pressing a number while no grid cell is selected highlights all grid cells with that number. Left/Right click on a clue in this highlight mode sets the number / removes the clue.
* Slant:
    * Extended the generator to three difficulty levels "Easy", "Tricky", "Hard"
    * New puzzle variation: "Creek", implementation of [Creek](http://www.janko.at/Raetsel/Creek/index.htm)
* Undead:
    * More sophisticated puzzle generator
    * Four difficulty levels "Easy", "Normal", "Tricky", "Hard"
    * New option "Remove clues", similiar to the option in the 'Magnets' puzzle
* Unfinished/Stellar: New puzzle "Stellar", implementation of [Sternenhaufen](http://www.janko.at/Raetsel/Sternenhaufen/index.htm)

## Usage:

You need a copy of the SGT Puzzle collection source code. Copy the source files from this repository into this build directory, and recompile.

For instructions how to compile the game collection, see the README accompanying the original puzzle collection.

## Soon

Android port

## LICENSE

Copyright (c) 2013-2014 Simon Tatham, Steffen Bauer

Portion copyright Richard Boulton, James Harvey, Mike Pinna, Jonas
K�lker, Dariusz Olszewski, Michael Schierl, Lambros Lambrou, Bernd
Schmidt, Lennard Sprong and Rogier Goossens.

Distributed under the MIT license, see LICENSE
