bin_PROGRAMS = pt1 pt2 ptj

AM_FFLAGS = @DEBUG_FLAG@ `torc_cflags` -fno-lto
AM_LDFLAGS = -fno-lto

LDADD = ../src/libpndl.a `torc_libs`

pt1_SOURCES = pt1.f
pt1_DEPENDENCIES = ../src/libpndl.a

pt2_SOURCES = pt2.f
pt2_DEPENDENCIES = ../src/libpndl.a

ptj_SOURCES = ptj.f
ptj_DEPENDENCIES = ../src/libpndl.a
