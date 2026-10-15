#CC=g++
CC=mpic++
CFLAGS=-O3 -Wall `torc_cflags` 
# CFLAGS+=-fopenmp
LIBS=`torc_libs`


all: main_moea

main_moea: main_moea.cpp common.h dmoea.h global.h individual.h objective.h random.h recombination.h fitfun.cpp # scalarfunc.h
	$(CC) $(CFLAGS) -o main_moea main_moea.cpp fitfun.cpp $(LIBS)

clean:
	rm -f main_moea
	rm -rf *.dSYM

