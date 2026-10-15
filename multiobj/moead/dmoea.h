#ifndef __MOEAD_H_
#define __MOEAD_H_

#include "global.h"
#include "common.h"
#include "individual.h"
#include "scalarfunc.h"
#include "recombination.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#include <torc.h>

class TMOEAD
{
public:

	TMOEAD();
	virtual ~TMOEAD();

	void init_uniformweight(int sd);			// initialize the weights for subproblems
	void init_neighbourhood();				// calculate the neighbourhood of each subproblem
	void init_population();					// initialize the population


	void update_reference(TIndividual &ind);		// update the approximation of ideal point
	void update_problem(TIndividual &child, int id);	// compare and update the neighboring solutions
	void evolution(int gen);					// mating restriction, recombination, mutation, update
	void run(int sd, int nc, int mg, int rn);		// execute MOEAD
	void save_front(char savefilename[1024]);		// save the pareto front into files

	vector <TSOP>  population;				// current population  	   
	TIndividual *indivpoint;				// reference point
	int  niche;						// neighborhood size
	int  pops;						// population   size	

	void operator=(const TMOEAD &emo);
};

TMOEAD::TMOEAD()
{

	idealpoint = new double[numObjectives];
	indivpoint = new TIndividual[numObjectives];
	// initialize ideal point	

	for(int n=0; n<numObjectives; n++) 
	{
		idealpoint[n] = 1.0e+30;  
		indivpoint[n].rnd_init();
	}

	double t0 = torc_gettime();
#if 0
//#pragma omp parallel for
	for(int n=0; n<numObjectives; n++) 
	{
		//indivpoint[n].obj_eval();
		obj_eval_raw(indivpoint[n].x_var.data(), &numVariables, indivpoint[n].y_obj.data(), &numObjectives, NULL);
	}
#else
	for(int n=0; n<numObjectives; n++) 
	{
		continue;
		int info[4];
		info[0] = 0; info[1] = 0; info[2] = 0; info[3] = n;
		
		//indivpoint[n].obj_eval();
		//obj_eval_raw(indivpoint[n].x_var.data(), &numVariables, indivpoint[n].y_obj.data(), &numObjectives);
		torc_create(-1, (void (*)())obj_eval_raw, 5,
				numVariables, MPI_DOUBLE, CALL_BY_COP,
				1, MPI_INT, CALL_BY_COP,
				numObjectives, MPI_DOUBLE, CALL_BY_REF,
				1, MPI_INT, CALL_BY_COP,
				4, MPI_INT, CALL_BY_COP,
				indivpoint[n].x_var.data(), &numVariables, indivpoint[n].y_obj.data(), &numObjectives, info);

	}
	torc_waitall();
#endif

	double t1 = torc_gettime();
	printf("constuctor took %lf s\n", t1-t0);


}

TMOEAD::~TMOEAD()
{
	delete [] idealpoint;
	delete [] indivpoint;
}


void TMOEAD::init_population()
{
	for(int i=0; i<pops; i++)
	{
		population[i].indiv.rnd_init();
	}

	double t0 = torc_gettime();
	for(int i=0; i<pops; i++)
	{
		int info[4];
		info[0] = 0; info[1] = 0; info[2] = 1; info[3] = i;
		//population[i].indiv.obj_eval();
		//obj_eval_raw(population[i].indiv.x_var.data(), &numVariables, population[i].indiv.y_obj.data(), &numObjectives, info);

		torc_create(-1, (void (*)())obj_eval_raw, 5,
				numVariables, MPI_DOUBLE, CALL_BY_COP,
				1, MPI_INT, CALL_BY_COP,
				numObjectives, MPI_DOUBLE, CALL_BY_REF,
				1, MPI_INT, CALL_BY_COP,
				4, MPI_INT, CALL_BY_COP,
				population[i].indiv.x_var.data(), &numVariables, population[i].indiv.y_obj.data(), &numObjectives, info);
	}
	torc_waitall();
	double t1 = torc_gettime();
	printf("init pop took %lf s\n", t1-t0);

	for(int i=0; i<pops; i++)
	{
		update_reference(population[i].indiv);
	}
}


// initialize a set of evely-distributed weight vectors
void TMOEAD::init_uniformweight(int sd)
{   
	for(int i=0; i<=sd; i++)
	{
		if(numObjectives==2)
		{
			TSOP sop;		   
			sop.array.push_back(i);
			sop.array.push_back(sd-i);
			for(unsigned int j=0; j<sop.array.size(); j++)
				sop.namda.push_back(1.0*sop.array[j]/sd);
			population.push_back(sop); 
		}
		else
		{
			for(int j=0;j<=sd;j++)
			{
				if(i+j<=sd)
				{
					TSOP sop;		   
					sop.array.push_back(i);
					sop.array.push_back(j);
					sop.array.push_back(sd-i-j);
					for(unsigned int k=0; k<sop.array.size(); k++)
						sop.namda.push_back(1.0*sop.array[k]/sd);
					population.push_back(sop); 
				}
			}
		}
	}
	pops = population.size();
}

// initialize the neighborhood of subproblems based on the distances of weight vectors
void TMOEAD::init_neighbourhood()
{
	double *x   = new double[pops];
	int*idx = new int[pops];
	for(int i=0; i<pops; i++)
	{	
		for(int j=0; j<pops; j++)
		{
			x[j]= distanceVector(population[i].namda,population[j].namda);
			idx[j]  = j;			
		}
		minfastsort(x,idx,pops,niche);   
		for(int k=0; k<niche; k++)   
			population[i].table.push_back(idx[k]);

	}
	delete [] x;
	delete [] idx;
}

// update the best solutions of neighboring subproblems
void TMOEAD::update_problem(TIndividual &indiv, int id)
{
	for(int i=0; i<niche; i++)
	{
		int k = population[id].table[i];
		double f1, f2;
		f1 = scalar_func(population[k].indiv.y_obj, population[k].namda, indivpoint);
		f2 = scalar_func(indiv.y_obj, population[k].namda, indivpoint);
		if(f2<f1) population[k].indiv = indiv;
	}
}


// update the reference point
void TMOEAD::update_reference(TIndividual &ind)
{
	for(int n=0; n<numObjectives; n++)
	{
		if(ind.y_obj[n]<idealpoint[n])
		{
			idealpoint[n]  = ind.y_obj[n];
			indivpoint[n]  = ind;
		}		
	}
}


// recombination, mutation, update in MOEA/D
void TMOEAD::evolution(int gen)
{
#if 0
	for(unsigned int i=0; i<population.size(); i++)
	{
		int   n  =  i; 
		int   s  = population[n].table.size();
		int   r1 = int(s*rnd_uni(&rnd_uni_init));
		int   r2 = int(s*rnd_uni(&rnd_uni_init));
		int   p1 = population[n].table[r1];
		int   p2 = population[n].table[r2];
		TIndividual child, child2;
		realbinarycrossover(population[p1].indiv,population[p2].indiv,child, child2);  
		realmutation(child, 1.0/numVariables);
		child.obj_eval();
		update_reference(child);
		update_problem(child, n);
	}
#else
	unsigned int nc = population.size();
	//TIndividual child[nc], child2[nc];
	vector<TIndividual> child(nc), child2(nc);
	for(unsigned int i=0; i<population.size(); i++)
	{
		int   n  =  i; 
		int   s  = population[n].table.size();
		int   r1 = int(s*rnd_uni(&rnd_uni_init));
		int   r2 = int(s*rnd_uni(&rnd_uni_init));
		int   p1 = population[n].table[r1];
		int   p2 = population[n].table[r2];
		realbinarycrossover(population[p1].indiv,population[p2].indiv,child[n], child2[n]);  
		realmutation(child[n], 1.0/numVariables);
	}

	double t0 = torc_gettime();
	for(unsigned int i=0; i<population.size(); i++)
	{
		int info[4];
		info[0] = 0; info[1] = 0; info[2] = gen; info[3] = i;

		int   n  =  i;
		//child[n].obj_eval();
		//obj_eval_raw(child[n].x_var.data(), &numVariables, child[n].y_obj.data(), &numObjectives);

		torc_create(-1, (void (*)())obj_eval_raw, 5,
				numVariables, MPI_DOUBLE, CALL_BY_COP,
				1, MPI_INT, CALL_BY_COP,
				numObjectives, MPI_DOUBLE, CALL_BY_REF,
				1, MPI_INT, CALL_BY_COP,
				4, MPI_INT, CALL_BY_COP,
				child[n].x_var.data(), &numVariables, child[n].y_obj.data(), &numObjectives, info);
	}
	torc_waitall();
	double t1 = torc_gettime();
	printf("evolve pop took %lf s\n", t1-t0);	

	for(unsigned int i=0; i<population.size(); i++)
	{
		int   n  =  i; 
		update_reference(child[n]);
		update_problem(child[n], n);
	}
#endif
}



void TMOEAD::run(int sd, int nc, int mg, int rn)
{
	// sd: integer number for generating weight vectors
	// nc: size of neighborhood
	// mg: maximal number of generations 
	niche = nc;	
	init_uniformweight(sd);
	init_neighbourhood();
	init_population();	
	for(int gen=2; gen<=mg; gen++)   evolution(gen);  
	char savefilename[1024];
	snprintf(savefilename,1024,"ParetoFront/DMOEA_%s_R%d.dat",strTestInstance,rn);
	save_front(savefilename);
	population.clear();
}


void TMOEAD::save_front(char saveFilename[1024])
{
	std::fstream fout;
	fout.open(saveFilename,std::ios::out);
	for(unsigned int n=0; n<population.size(); n++)
	{
		for(int k=0;k<numVariables;k++)
			fout<<population[n].indiv.x_var[k]<<"  ";

		for(int k=0;k<numObjectives;k++)
			fout<<population[n].indiv.y_obj[k]<<"  ";
		fout<<"\n";
	}
	fout.close();
}


void TMOEAD::operator=(const TMOEAD &emo)
{
	pops= emo.pops;
	population  = emo.population;
	indivpoint  = emo.indivpoint;
	niche   = emo.niche;
} 


#endif
