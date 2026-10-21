use_torc?=1
use_omp?=0

CC = gcc
CFLAGS = -g -ggdb -O3 -std=c99 -D_XOPEN_SOURCE=700 -D_BSD_SOURCE
CFLAGS += -Wall
CFLAGS += -I/opt/homebrew/include
LDFLAGS += -L/opt/homebrew/lib
LDFLAGS += -lgsl -lgslcblas -lm

ifeq ($(use_torc),1)
CC = mpicc
CFLAGS += -D_USE_TORC_=1 `torc_cflags`
LDFLAGS += `torc_libs`
endif

ifeq ($(use_omp),1)
CFLAGS += -D_USE_OPENMP_=1 -fopenmp
LDFLAGS += -fopenmp
endif


TARGETS = engine_tmcmc

all: $(TARGETS)

engine_tmcmc: engine_tmcmc.c engine_tmcmc.h spawner.h tmcmc_stats.c tmcmc_db.c auxil.c fitfun.c fitfun.h
	$(CC) $(CFLAGS) -o engine_tmcmc engine_tmcmc.c tmcmc_stats.c tmcmc_db.c auxil.c spawner.c fitfun.c $(LDFLAGS)



clean:
	rm -f engine_tmcmc *.o
	rm -rf *.dSYM

clear:
	rm -rf *.txt
