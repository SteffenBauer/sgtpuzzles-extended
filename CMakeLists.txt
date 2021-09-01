
set(creek_crop 193x193 160x160+16+16)
set(unequal_plus_crop 208x208 135x135+61+13)
set(undead_plus_crop 416x480 192x192+16+80)
set(walls_crop 232x232 256x256+0+0)

puzzle(creek
  DISPLAYNAME "Creek"
  DESCRIPTION "Path-drawing puzzle"
  OBJECTIVE "Draw a connected path that matches the clues.")

puzzle(walls
  DISPLAYNAME "Walls"
  DESCRIPTION "Path-finding puzzle"
  OBJECTIVE "Find a path through a maze.")

puzzle(solo_plus
  DISPLAYNAME "Solo+"
  DESCRIPTION "Number placement puzzle"
  OBJECTIVE "Fill in the grid so that each row, column and square \
block contains one of every digit.")

puzzle(undead_plus
  DISPLAYNAME "Undead+"
  DESCRIPTION "Monster-placing puzzle"
  OBJECTIVE "Place ghosts, vampires and zombies so that the right \
numbers of them can be seen in mirrors.")

puzzle(unequal_plus
  DISPLAYNAME "Unequal+"
  DESCRIPTION "Latin square puzzle"
  OBJECTIVE "Complete the latin square in accordance with the > signs.")

add_subdirectory(Unfinished)

export_variables_to_parent_scope()

