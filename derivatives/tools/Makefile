CC = mpicc
MPIF90 = mpif90

CPPFLAGS += -D_XOPEN_SOURCE=700 -I../pndl/src
CFLAGS = -O3 -std=c99 -Wall -fno-lto
TORC_CFLAGS = `torc_cflags`

LDFLAGS += -fno-lto
LDLIBS = `torc_libs` -lpthread -lgsl -lgslcblas -lm

TARGETS = sa_deriv fd_deriv fd_grad

all: $(TARGETS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(TORC_CFLAGS) -c $< -o $@

sa_deriv.o fd_deriv.o fd_grad.o: fitfun.c

sa_deriv: sa_deriv.o auxil.o
	$(CC) $(LDFLAGS) -o $@ sa_deriv.o auxil.o $(LDLIBS)

fd_deriv: fd_deriv.o auxil.o ../pndl/src/libpndl.a
	$(MPIF90) $(LDFLAGS) -o $@ fd_deriv.o auxil.o ../pndl/src/libpndl.a $(LDLIBS)

fd_grad: fd_grad.o auxil.o ../pndl/src/libpndl.a
	$(MPIF90) $(LDFLAGS) -o $@ fd_grad.o auxil.o ../pndl/src/libpndl.a $(LDLIBS)

clean:
	rm -f $(TARGETS) *.o sHbarbar.txt

.PHONY: all clean
