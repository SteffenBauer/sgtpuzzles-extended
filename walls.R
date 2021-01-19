# -*- makefile -*-

WALLS_EXTRA = dsf

walls : [X] GTK COMMON walls WALLS_EXTRA walls-icon|no-icon
walls : [G] WINDOWS COMMON walls walls.res|noicon.res

ALL += walls[COMBINED]

!begin am gtk
GAMES += walls
!end

!begin >list.c
    A(walls) \
!end

!begin >gamedesc.txt
walls:walls.exe:Walls:Find a path through a maze
!end

