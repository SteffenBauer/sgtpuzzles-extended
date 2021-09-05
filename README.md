sgtpuzzles-extended
===================

Extensions/Modifications I wrote for the famous & excellent [Simon Tathams Portable Puzzle Collection](http://www.chiark.greenend.org.uk/~sgtatham/puzzles/)

## Extensions / New puzzles:

### Solo+

The original solo app is a little bit too tedious for my taste. When starting a new game, entering pencils marks takes quite some time without really adding fun.

I added some new keyboard controls, to support game solving like those used in other sudoku apps.

#### *Plus* key
The **+** key fills all remaining possible pencils marks. See this example:

| Initial game | After pressing + |
| :----------: | :--------------: |
![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/solo_1a.png)|![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/solo_1b.png)

#### *Minus* key
The **-** key removes all pencils marks that directly contradict already filled numbers. See this example:

| Initial game | After pressing - |
| :----------: | :--------------: |
![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/solo_2a.png)|![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/solo_2b.png)

#### Number highlighting
Pressing a number key when no grid cell is selected will highlight all grid cells in green that contain this number (filled or pencil marked).

Left-clicking into a highlighted cell will fill that number. Right-click into a highlighted cell will remove that pencil mark.

See this example:

| Board after pressing 1 | After left-click into cell at (4/2) |
| :--------------------: | :---------------------------------: |
![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/solo_3a.png)|![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/solo_3b.png)

#### Manual mode
With this mode it is possible to manually enter puzzles from newspapers or other sources. It works like this:

|         |         |
| :------ | ------: |
| Manual mode starts with an empty grid: | ![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/solo_m1.png) |
| Enter numbers as usual (no pencils or solving during entry mode) | ![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/solo_m2.png) |
| When finished, click somewhere on the border outside the game grid, to fixate the numbers. Now the puzzle is ready to play. Restarting a game will return to this state with all fixed clues. | ![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/solo_m3.png) |

### Creek
![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/creek.png)

*Status*: Fully implemented and playable  

A Path-finding puzzle, implementation of [Creek](http://www.janko.at/Raetsel/Creek/index.htm)  

Game objective: Color each grid cell either black or white, with these conditions:

* All white cells must form a connected area (horizontally / vertically connected)
* The clue number indicates the number of black cells around the clue

### Undead+
![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/undead_plus.png)

An extended version of my puzzle *Undead*

* More sophisticated puzzle generator
* Four difficulty levels "Easy", "Normal", "Tricky", "Hard"
* New option "Remove clues", similiar to the option in the 'Magnets' puzzle
* Show count of placed monsters

### Unequal+
![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/unequal_plus.png)

Contains a new puzzle variation: *Kropki*, implementation of [Kropki](http://wiki.logic-masters.de/index.php?title=Kropki/de)

Works similiar to the *Adjacent* mode, with these rules:

* A black circle indicates that one number is double to its neighbour.
* A white circle indicates that one number is adjacent (one higher / lower) to its neighbour.
* No circle means both numbers must not be double or adjacent.
* The numbers *1* and *2* are both double and adjacent to each other. In this case, the circle color is chosen randomly.

### Walls
![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/walls.png)

*Status*: Nearly finished; only path dragging is missing.

This is an implementation of the excellent puzzle *Alcazar*, which I found in the Android store quite some time ago. Unfortunately, the developers of it don't seem to have time to maintain it anymore and pulled it from the Android store.

Objective of this game is to find a path through the given maze of walls, following these rules:

* The path enters the grid from one open border, and leaves the grid through another open border.
* All grid cells are visited once.
* The path may not cross itself or form loops.
* The path cannot go through a wall.

Controls:

* Left-click on a border between two cells to place a path segment.
* Right-click on a border between two cells to place a wall.

### Stellar
![](https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/stellar.png)

*Status*: Unfinished. Playable, but needs lot of work.

An implementation of [Sternenhaufen](http://www.janko.at/Raetsel/Sternenhaufen/index.htm)

Game objective: Place stars (by pressing key 'S') and stellar clouds (by pressing key 'C') into the grid, following these rules:

* Every row and every column must contain exactly one star and one cloud.
* Clouds block sunlight.
* The stars must be placed in a way that the given planets are correctly illuminated.

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

Portions copyright Richard Boulton, James Harvey, Mike Pinna, Jonas
Kölker, Dariusz Olszewski, Michael Schierl, Lambros Lambrou, Bernd
Schmidt, Steffen Bauer, Lennard Sprong, Rogier Goossens, Michael
Quevillon, Asher Gordon and Didi Kohen.

Distributed under the MIT license, see LICENSE
