#CCPLUS=CC -std=c++11
MPI_CXX=g++-16
CCPLUS=mpic++ -std=c++14
CC=gcc
# SHARK_DIR=./Shark-3.1.0/include
SHARK_DIR=./Shark

#SHARK_DIR=/Users/phadjido/pi4u/multiobj/mocmaes_torc/Shark
# SHARK_DIR=/home/phadjido/pi4u/multiobj/mocmaes_torc/Shark/build/include

CFLAGS=-g # -fopenmp
CFLAGS+=-I$(SHARK_DIR)/include -I$(SHARK_DIR)/build/include
CFLAGS+=`torc_cflags`
LIBS=-L$(SHARK_DIR)/build/lib -lshark
LIBS+=-lopenblas  -lboost_thread -lboost_serialization `torc_libs`


#BOOST_PREFIX := $(shell brew --prefix boost)
#CFLAGS += -I$(BOOST_PREFIX)/include

all:  MOCMAtest1 ssMOCMAtest1 #MOCMAtest1

MOCMAtest1: MOCMAtest1.o fitfun.o
	$(CCPLUS) $(CFLAGS) -o MOCMAtest1 MOCMAtest1.o fitfun.o $(LIBS)

MOCMAtest1.o: MOCMAtest1.cpp USER1.h MOCMApeh.h
	$(CCPLUS) $(CFLAGS) -c -o MOCMAtest1.o MOCMAtest1.cpp

ssMOCMAtest1: ssMOCMAtest1.o fitfun.o
	$(CCPLUS) $(CFLAGS) -o ssMOCMAtest1 ssMOCMAtest1.o fitfun.o $(LIBS)

ssMOCMAtest1.o: ssMOCMAtest1.cpp USER1.h ssMOCMApeh.h
	$(CCPLUS) $(CFLAGS) -c -o ssMOCMAtest1.o ssMOCMAtest1.cpp

fitfun.o:  fitfun.c
	$(CC) $(CFLAGS) -c fitfun.c

clean:
	rm -f MOCMAtest1 MOCMAtest1.o fitfun.o
	rm -f ssMOCMAtest1 ssMOCMAtest1.o

clear:
	rm -f curgen_db_*.txt pareto_mocma_*.txt
