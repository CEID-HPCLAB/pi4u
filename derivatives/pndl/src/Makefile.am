pndllibdir = $(libdir)
pndllib_LIBRARIES = libpndl.a

# Disable LTO for the legacy Fortran task interface.
AM_FFLAGS = @DEBUG_FLAG@ `torc_cflags` -I. -fno-lto
AM_CFLAGS = @DEBUG_FLAG@ `torc_cflags` -I. -fno-lto

libpndl_a_SOURCES = pndl1f.f pndl1n.f pndl1q.f \
	pndl2ff.f pndl2fq.f pndl2gf.f pndl2gq.f \
	pndlhf.f pndlhga.f pndlhg.f \
	pndlga.f pndlg.f pndlhfa.f \
	pndlja.f pndljf.f pndljq.f pndlj.f \
	pndlmsg.f pndlarg.f pndlbnd.f pndlacc.f pndllib.f \
	pndlghfa.f pndl3ff.f pndl3fq.f pndlc.c

pndlincdir = $(includedir)
pndlinc_HEADERS = ./pndlc.h


.f$(OBJEXT):
	$(F77) $(AM_FFLAGS) $(FFLAGS) -fno-lto -c $< -o $@

# $(F77) $(AM_FFLAGS) -c $<


