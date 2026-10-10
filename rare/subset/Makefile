CPP = mpic++ 

CFLAGS = -O3 -Wall

CFLAGS += `torc_cflags` -D_XOPEN_SOURCE=700 -D_BSD_SOURCE `gsl-config --cflags`
LDFLAGS += `torc_libs` -lm  # -lpthread -lm 

# GSL
LDFLAGS += `gsl-config --libs` # -lgsl -lgslcblas

# PRIMARY

TARGETS = engine_ss_fixed engine_ss_adaptive 

all: $(TARGETS)

engine_ss_fixed: engine_ss_fixed.cpp ss_db.cpp auxil.o fitfun.c
	$(CPP) $(CFLAGS) -o engine_ss_fixed engine_ss_fixed.cpp ss_db.cpp auxil.o $(LDFLAGS)

engine_ss_adaptive: engine_ss_adaptive.cpp ss_db.cpp auxil.o fitfun.c
	$(CPP) $(CFLAGS) -o engine_ss_adaptive engine_ss_adaptive.cpp ss_db.cpp auxil.o $(LDFLAGS)

auxil.o: auxil.cpp
	$(CPP) $(CFLAGS) -c auxil.cpp

clean:
	rm -f engine_ss_fixed engine_ss_adaptive *.o

clear:
	rm -rf samples*.txt seeds*.txt Pc.txt

