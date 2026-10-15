/*==========================================================================
//  Implementation of Multiobjective Evolutionary Algorithm Based on 
//  Decomposition (MOEA/D) For Continuous Multiobjective Optimization Problems (2006)
//
//  See the details of MOEA/D in the following paper
//  Q. Zhang and H. Li, MOEA/D: A Multi-objective Evolutionary Algorithm Based on Decomposition, 
//  IEEE Trans. on Evolutionary Computation, in press, 2007
//
//  The source code of MOEA/D was implemented by Hui Li and Qingfu Zhang  
//
//  If you have any questions about the codes, please contact 
//  Qingfu Zhang at qzhang@essex.ac.uk  or Hui Li at hzl@cs.nott.ac.uk
===========================================================================*/


#include "global.h"
#include "dmoea.h"

#if 1
typedef struct moead_params_s
{
	char    strTestInstance[256];
	char    strFunctionType[256];

	int Nth;
	int Nobj;
	int MaxStages;
	int Niche;
	int PopSize;

	int id_cx;	//20;		// crossover (global variable)
	int id_mu;	//20;		// for mutation (global variable)

	double  *lowerbound;
	double  *upperbound;

	double lb, ub;  /*generic lower and upper bound*/

	long seed;
} moead_params_t;

moead_params_t moead_params;

void params_init()
{
	// default parameters

	// set the type of decomposition method
	// "_TCH1": Tchebycheff, "_TCH2": normalized Tchebycheff, "_PBI": Penalty-based BI
	strcpy(moead_params.strFunctionType,"_TCH1");
	strcpy(moead_params.strTestInstance,"fitfun");

	moead_params.Nth = 1;
	moead_params.Nobj = 2;
	moead_params.Niche = 20;
	moead_params.MaxStages = 30;
	moead_params.PopSize = 64;

	moead_params.id_cx = 20;
	moead_params.id_mu = 20;

	moead_params.lb = -10;   /* Default LB, same for all */
	moead_params.ub = +10;   /* Default UB, same for all */

	moead_params.lowerbound = (double *)malloc(moead_params.Nth*sizeof(double));
	moead_params.upperbound = (double *)malloc(moead_params.Nth*sizeof(double));
	for (int i = 0; i < moead_params.Nth; i++) {
		moead_params.lowerbound[i] = moead_params.lb;
		moead_params.upperbound[i] = moead_params.ub;
	}

	moead_params.seed = 1;

	/* USER-DEFINED VALUES */
	FILE *f = fopen("moead.par", "r");
	if (f == NULL) {
		return;
	}

	/*
	Nth		1
	Nobj		2
	Niche		20
	MaxStages	30
	PopSize		64
	id_cx		20
	id_mu		20
	Bdef	-10	10
	#B0	-6	6
	#B1	-6	6
	seed	1
	*/

	char line[256];

	int line_no = 0;
	while (fgets(line, 256, f)!= NULL) {
		line_no++;
		if ((line[0] == '#')||(strlen(line)==0)) {
			printf("ignoring line %d\n", line_no);
			continue;
		}

		if (strstr(line, "Nth")) {
			sscanf(line, "%*s %d", &moead_params.Nth);
		}
		else if (strstr(line, "Nobj")) {
			sscanf(line, "%*s %d", &moead_params.Nobj);
		}
		else if (strstr(line, "Niche")) {
			sscanf(line, "%*s %d", &moead_params.Niche);
		}
		else if (strstr(line, "MaxStages")) {
			sscanf(line, "%*s %d", &moead_params.MaxStages);
		}
		else if (strstr(line, "PopSize")) {
			sscanf(line, "%*s %d", &moead_params.PopSize);
		}
		else if (strstr(line, "id_cx")) {
			sscanf(line, "%*s %d", &moead_params.id_cx);
		}
		else if (strstr(line, "id_mu")) {
			sscanf(line, "%*s %d", &moead_params.id_mu);
		}
		else if (strstr(line, "Bdef")) {
			sscanf(line, "%*s %lf %lf", &moead_params.lb, &moead_params.ub);
		}
		else if (strstr(line, "seed")) {
			sscanf(line, "%*s %ld", &moead_params.seed);
		}
	}


	{
	rewind(f);
	line_no = 0;

	free(moead_params.lowerbound);
	free(moead_params.upperbound);
	moead_params.lowerbound = (double *)malloc(moead_params.Nth*sizeof(double));
	moead_params.upperbound = (double *)malloc(moead_params.Nth*sizeof(double));
	int found;
	for (int i = 0; i < moead_params.Nth; i++) {
		found = 0;
		while (fgets(line, 256, f)!= NULL) {
			line_no++;

			if ((line[0] == '#')||(strlen(line)==0)) continue;

			char bound[32];
			snprintf(bound, sizeof(bound), "B%d", i);

			if (strstr(line, bound) != NULL) {
				sscanf(line, "%*s %lf %lf", &moead_params.lowerbound[i], &moead_params.upperbound[i]);
				found = 1;
				break;
			}
		}

		if (!found) {
			moead_params.lowerbound[i] = moead_params.lb;   /* Bdef value or Default LB */
			moead_params.upperbound[i] = moead_params.ub;   /* Bdef value of Default UB */
		}

		rewind(f);
		line_no = 0;
	}
	}

	fclose(f);
}

#endif

int main(int argc, char *argv[])
{
	params_init();

	strcpy(strFunctionType, moead_params.strFunctionType);
	strcpy(strTestInstance, moead_params.strTestInstance);

	int  max_gen = moead_params.MaxStages;	//1000    // maximal number of generations
	int  niche   = moead_params.Niche;	// 20	//neighborhood size

	numVariables  = moead_params.Nth;	// number of variables  (global variable)
	numObjectives = moead_params.Nobj;	// number of objectives (global variable)

	id_cx = moead_params.id_cx;	//20;		// crossover (global variable)
	id_mu = moead_params.id_mu;	//20;		// for mutation (global variable)

	for (int i = 0; i < numVariables; i++)
	{
		lowBound[i] = moead_params.lowerbound[i];	//-10;
		uppBound[i] = moead_params.upperbound[i];	//+10;
	}

//	seed = (seed + 111)%1235;
	seed = moead_params.seed;

	rnd_uni_init = -(long)seed;

	int popsize = moead_params.PopSize;	//32;

	torc_register_task((void *)obj_eval_raw);
	torc_init(argc, argv, MODE_MW);

	TMOEAD MOEAD;
	int run = 1;

	int param;

	if(numObjectives==2)
	{
		param = popsize-1;
		MOEAD.run(param, niche, max_gen, run);  //99 -2  popsize 100	// 0..99 = 100
	}

	if(numObjectives==3)
	{
		//(n+1)(n+2)/2=x -> n = 0.5*(sqrt(8x+1)-3)
		//param = (int) (0.5*sqrt(8*popsize+1)-3);
		param = (int)floor((sqrt(8.0 * popsize + 1.0) - 3.0) / 2.0);

		MOEAD.run(param, niche, max_gen, run);  //23 -3  popsize 300 // (23+1)(24+1)/2=300
	}

	torc_finalize();
	return 0;
}
