sgtpuzzles-extended
===================
![puzzles](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/puzzles.png)

Extensions/Modifications I wrote for the famous & excellent [Simon Tathams Portable Puzzle Collection](http://www.chiark.greenend.org.uk/~sgtatham/puzzles/)

## Extensions:

* Solo+: Additional keyboard controls to support game solving like used in some other sudoku apps
    * Pressing '+' automatically sets all possible game clues
    * Pressing '-' removes all impossible game clues
    * Pressing a number while no grid cell is selected highlights all grid cells with that number. Left/Right click on a clue in this highlight mode sets the number / removes the clue.
* Creek: Path-finding puzzle, implementation of [Creek](http://www.janko.at/Raetsel/Creek/index.htm)
    * Fully implemented and playable
* Undead+:
    * More sophisticated puzzle generator
    * Four difficulty levels "Easy", "Normal", "Tricky", "Hard"
    * New option "Remove clues", similiar to the option in the 'Magnets' puzzle
    * Show count of placed monsters
* Unequal+:
    * New puzzle variation: "Kropki", implementation of [Kropki](http://wiki.logic-masters.de/index.php?title=Kropki/de)
* Walls: New puzzle "Walls", find a path through a maze of walls
    * Nearly finished; only path dragging is missing.
* Unfinished/Stellar: New puzzle "Stellar", implementation of [Sternenhaufen](http://www.janko.at/Raetsel/Sternenhaufen/index.htm)
    * So far playable in difficulty "Normal".

## Usage:

* Get the source code for the SGT Portable Puzzle Collection from [the official site](https://www.chiark.greenend.org.uk/~sgtatham/puzzles/).
* Copy this folder into the above source folder as a subdirectory. Alternatively, add this repository as a submodule of the main repository.
* In the main repository's `CMakeLists.txt`, go to the line with `add_subdirectory(unfinished)` and add the following line below it:
```cmake
add_subdirectory(sgtpuzzles-extended) # or whatever this folder is called
```
* Copy all save files into the main `/icons` folder:
```sh
cp sgtpuzzles-extended/savefiles/*.sav icons/
```
* Run CMake in the main folder.

## Utility examples

[Example code](https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/Examples) for the utility API.

## Android

Files for the [Android port by Chris Boyle](https://github.com/chrisboyle/sgtpuzzles)

Contains also the puzzles [boats, rome and salad](https://github.com/x-sheep/puzzles-unreleased) by Lennard Sprong

**Outdated**, won't compile with current Android version.

## LICENSE

Copyright (c) 2013-2021 Simon Tatham, Steffen Bauer

Portion copyright Richard Boulton, James Harvey, Mike Pinna, Jonas
Koelker, Dariusz Olszewski, Michael Schierl, Lambros Lambrou, Bernd
Schmidt, Lennard Sprong and Rogier Goossens.

Distributed under the MIT license, see LICENSE
