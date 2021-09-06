sgtpuzzles-extended
===================

Extensions/Modifications I wrote for the famous & excellent [Simon Tathams Portable Puzzle Collection](http://www.chiark.greenend.org.uk/~sgtatham/puzzles/)

## Extensions / New puzzles:

Click on a puzzle for a description of individual game extensions

<table>
<tr>
<td align="center"><b><a href="https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/docs/solo_plus.md">Solo+</a></b><br/>Additional solving tools</td>
<td align="center"><b><a href="https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/docs/creek.md">Creek</a></b><br/>Find a path,<br/>matching the clues</td>
<td align="center"><b><a href="https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/docs/undead_plus.md">Undead+</a></b><br/>Extended game generator</td>
</tr>
<tr>
<td><a href="https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/docs/solo_plus.md"><img src="https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/solo_plus.png"></a></td>
<td><a href="https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/docs/creek.md"><img src="https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/creek.png"></a></td>
<td><a href="https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/docs/undead_plus.md"><img src="https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/undead_plus.png"></a></td>
</tr>
<tr>
<td align="center"><b><a href="https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/docs/unequal_plus.md">Unequal+</a></b><br/>Puzzle variation <i>Kropki</i></td>
<td align="center"><b><a href="https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/docs/walls.md">Walls</a></b><br/>Find a path through a maze of walls</td>
<td align="center"><b><a href="https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/docs/stellar.md">Stellar</a></b><br/>Place stars and nebulae<br/>so that the given planets<br/>are correctly illuminated.</td>
</tr>
<tr>
<td><a href="https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/docs/unequal_plus.md"><img src="https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/unequal_plus.png"></a></td>
<td><a href="https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/docs/walls.md"><img src="https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/walls.png"></a></td>
<td><a href="https://github.com/SteffenBauer/sgtpuzzles-extended/blob/master/docs/stellar.md"><img src="https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/stellar.png"></a></td>
</tr>
</table>

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
