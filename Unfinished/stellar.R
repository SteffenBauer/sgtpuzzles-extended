# -*- makefile -*-

STELLAR_EXTRA = latin maxflow tree234

stellar : [X] GTK COMMON stellar STELLAR_EXTRA stellar-icon|no-icon
stellar : [G] WINDOWS COMMON stellar stellar.res|noicon.res

ALL += stellar[COMBINED]

!begin am gtk
GAMES += stellar
!end

!begin >list.c
    A(stellar) \
!end

!begin >gamedesc.txt
stellar:stellar.exe:Stellar:Place stars and nebulae
!end

