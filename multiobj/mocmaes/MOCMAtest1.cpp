/*!
 * 
 *
 * \brief       Example of an expriment using the MO-CMA-ES on several benchmark functions
 *
 * \author      O.Krause
 * \date        2014
 *
 * \par Copyright 1995-2015 Shark Development Team
 * 
 * <BR><HR>
 * This file is part of Shark.
 * <http://image.diku.dk/shark/>
 * 
 * Shark is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published 
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Shark is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with Shark.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
// Implementation of the MO-CMA-ES
//#include <shark/Algorithms/DirectSearch/MOCMA.h>

#include <stdio.h>
#include <torc.h>
#include <iostream>

int checkpoint_restart = 0;

// hardcoded max time (negative if no use)
#define JOBMAXTIME 24*60*60

typedef struct mocma_params_s
{
	int Nth;
	int MaxStages;
	int PopSize;

	double *lowerbound;
	double *upperbound;

	double lb, ub;          /*generic lower and upper bound*/

	long seed;

	double initialSigma;
	double shrink_factor;
	unsigned long maxtries;
} mocma_params_t;

mocma_params_t mocma_params;

#include "USER1.h"
#include "MOCMApeh.h"
//#include <shark/Rng/GlobalRng.h>
// Access to benchmark functions


void params_init()
{
	mocma_params.Nth = 1;
	mocma_params.MaxStages = 30;
	mocma_params.PopSize = 64;

	mocma_params.lb = -10;   /* Default LB, same for all */
	mocma_params.ub = +10;   /* Default UB, same for all */

	mocma_params.lowerbound = reinterpret_cast<double *>(malloc(mocma_params.Nth*sizeof(double)));
	mocma_params.upperbound = reinterpret_cast<double *>(malloc(mocma_params.Nth*sizeof(double)));
	for (int i = 0; i < mocma_params.Nth; i++) {
		mocma_params.lowerbound[i] = mocma_params.lb;
		mocma_params.upperbound[i] = mocma_params.ub;
	}

	mocma_params.seed = 1;
	mocma_params.initialSigma = 0.1;
	mocma_params.shrink_factor = 1.0;
	mocma_params.maxtries = 1e4;

	/* USER-DEFINED VALUES */
	FILE *f = fopen("mocma.par", "r");
	if (f == NULL) {
		return;
	}

	/*
	Nth             1
	MaxStages	30
	PopSize         64
	Bdef            -10	10
	#B0              -6     6
	#B1              -6     6
	seed		1
	initialSigma	1
	shrink_factor	1
	*/

	char line[256];

	int line_no = 0;
	while (fgets(line, 256, f)!= NULL) {
		line_no++;
		if ((line[0] == '#')||(strlen(line)==0)) {
			printf("ignoring line %d\n", line_no);
			continue;
		}

		if (strstr(line, "Nth"))
		{
			sscanf(line, "%*s %d", &mocma_params.Nth);
		}
		else if (strstr(line, "MaxStages"))
		{
			sscanf(line, "%*s %d", &mocma_params.MaxStages);
		}
		else if (strstr(line, "PopSize"))
		{
			sscanf(line, "%*s %d", &mocma_params.PopSize);
		}
		else if (strstr(line, "Bdef"))
		{
			sscanf(line, "%*s %lf %lf", &mocma_params.lb, &mocma_params.ub);
		}
		else if (strstr(line, "seed"))
		{
			sscanf(line, "%*s %ld", &mocma_params.seed);
		}
		else if (strstr(line, "initialSigma"))
		{
			sscanf(line, "%*s %lf", &mocma_params.initialSigma);
		}
		else if (strstr(line, "shrink_factor"))
		{
			sscanf(line, "%*s %lf", &mocma_params.shrink_factor);
		}
		else if (strstr(line, "maxtries"))
		{
			sscanf(line, "%*s %ld", &mocma_params.maxtries);
		}
	}


	{
	rewind(f);
	line_no = 0;

	free(mocma_params.lowerbound);
	free(mocma_params.upperbound);
	mocma_params.lowerbound = reinterpret_cast<double *>(malloc(mocma_params.Nth*sizeof(double)));
	mocma_params.upperbound = reinterpret_cast<double *>(malloc(mocma_params.Nth*sizeof(double)));

	int found;
	for (int i = 0; i < mocma_params.Nth; i++) {
		found = 0;
		while (fgets(line, 256, f)!= NULL) {
			line_no++;

			if ((line[0] == '#')||(strlen(line)==0)) continue;

			char bound[32];
			snprintf(bound, 32, "B%d", i);
			if (strstr(line, bound) != NULL) {
				sscanf(line, "%*s %lf %lf", &mocma_params.lowerbound[i], &mocma_params.upperbound[i]);
				found = 1;
				break;
			}
		}
		if (!found) {
			mocma_params.lowerbound[i] = mocma_params.lb;   /* Bdef value or Default LB */
			mocma_params.upperbound[i] = mocma_params.ub;   /* Bdef value of Default UB */
		}

		rewind(f);
		line_no = 0;
	}
	}

	fclose(f);
}

using namespace shark;

int main( int argc, char ** argv ) {
	params_init();
	torc_register_task((void *)geval);
	torc_init(argc, argv, MODE_MW);

	if (argc == 2)
	{
		if (strcmp(argv[1], "-cr") == 0)
			checkpoint_restart = 1;
	}

	// restart mechanism variables
        double gt0;
        gt0 = torc_gettime();

	std::size_t frontSize = mocma_params.PopSize; //number of points that approximate the front
	std::size_t numDimensions = mocma_params.Nth; //dimensions of the objective functions
	std::size_t numTrials = 1; // how often the optimization is repeated
	std::size_t recordingInterval = 1; //we want to record after some multiple of this
	std::size_t numIterations = mocma_params.MaxStages; //number of iterations to perform

	// seed the global random number generator
	if (mocma_params.seed == 0)
		shark::random::globalRng.seed(static_cast<shark::random::rng_type::result_type>(gt0));
	else
		shark::random::globalRng.seed(static_cast<shark::random::rng_type::result_type>(mocma_params.seed));

	USER1 user1(numDimensions);
	user1.setNumberOfVariables(numDimensions);

	//print progress
	//create and initialize the optimizer
	MOCMA mocma;
	mocma.mu() = frontSize;
	mocma.initialSigma() = mocma_params.initialSigma;

	printf("calling mocma.init....\n");
	mocma.init(user1);

	//optimize
	for(std::size_t i = 1; i <= numIterations; ++i){

		double t0 = torc_gettime();
		mocma.step(user1);
		double t1 = torc_gettime();
		printf("step %ld took %f s\n", i, t1-t0);

# if (JOBMAXTIME > 0)
                {
                double lastgen_time = t1 - t0;
                static long maxt = -1;

                long runt, remt;
                if (maxt == -1) {
                        maxt = JOBMAXTIME;
                        printf("job maxtime = %ld\n", maxt);
                }

                runt = torc_gettime() - gt0;
                remt = maxt - runt;
                printf("job runtime = %ld\n", runt);
                printf("job remtime = %ld\n", remt);

                if ((lastgen_time * 1.5) > remt) {
                        printf("no more available time, exiting... \n");
                        // do something
                        break;
                }

                }
#endif

#if 1
	{
	char pareto_file[256];

	snprintf(pareto_file, 256, "pareto_mocma_%03ld.txt", i);
	FILE *fp = fopen(pareto_file, "w");
	for( std::size_t i = 0; i < mocma.solution().size(); i++ ) {
		for( std::size_t j = 0; j < user1.numberOfVariables(); j++ ) {
			fprintf(fp, "%f ", mocma.solution()[i].point[j]);
		}
		for( std::size_t j = 0; j < user1.numberOfObjectives(); j++ ) {
			fprintf(fp, "%f ", mocma.solution()[i].value[j]);
		}
		fprintf(fp, "\n");
	}
	fclose(fp);
	}
#endif


	}

       // Print the optimal pareto front
	for( std::size_t i = 0; i < mocma.solution().size(); i++ ) {
		for( std::size_t j = 0; j < user1.numberOfVariables(); j++ ) {
			std::cout<< mocma.solution()[ i ].point[j]<<" ";
		}
		for( std::size_t j = 0; j < user1.numberOfObjectives(); j++ ) {
			std::cout<< mocma.solution()[ i ].value[j]<<" ";
		}
		std::cout << std::endl;
	}

	FILE *fp = fopen("pareto_mocma.txt", "w");
	for( std::size_t i = 0; i < mocma.solution().size(); i++ ) {
		for( std::size_t j = 0; j < user1.numberOfVariables(); j++ ) {
			fprintf(fp, "%f ", mocma.solution()[i].point[j]);
		}
		for( std::size_t j = 0; j < user1.numberOfObjectives(); j++ ) {
			fprintf(fp, "%f ", mocma.solution()[i].value[j]);
		}
		fprintf(fp, "\n");
	}
	fclose(fp);

	torc_finalize();
	return 0;
}
