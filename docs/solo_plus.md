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

<table style="background-color:#ffffff;">
<tr style="background-color:#ffffff;">
<td style="background-color:#ffffff;">Manual mode starts with an empty grid:</td>
<td style="background-color:#ffffff;"><img src="https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/solo_m1.png"></td>
</tr>
<tr style="background-color:#ffffff;">
<td style="background-color:#ffffff;">Enter numbers as usual (no pencils or solving during entry mode</td>
<td style="background-color:#ffffff;"><img src="https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/solo_m2.png"></td>
</tr>
<tr style="background-color:#ffffff;">
<td style="background-color:#ffffff;">When finished, click somewhere on the border outside the game grid, to fixate the numbers. Now the puzzle is ready to play. Restarting a game will return to this state with all fixed clues.</td>
<td style="background-color:#ffffff;"><img src="https://raw.githubusercontent.com/SteffenBauer/sgtpuzzles-extended/master/screenshots/solo_m3.png"></td>
</tr>
</table>
