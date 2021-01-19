# -*- makefile -*-

CREEK_EXTRA = dsf findloop

creek    : [X] GTK COMMON creek CREEK_EXTRA creek-icon|no-icon

creek    : [G] WINDOWS COMMON creek CREEK_EXTRA creek.res|noicon.res

ALL += creek[COMBINED] CREEK_EXTRA

!begin am gtk
GAMES += creek
!end

!begin >list.c
    A(creek) \
!end

!begin >gamedesc.txt
creek:creek.exe:Creek:Path-drawing puzzle:Draw a connected path that matches the clues.
!end
