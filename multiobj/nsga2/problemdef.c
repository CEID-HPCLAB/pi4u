/* Test problem definitions */

# include <stdio.h>
# include <stdlib.h>
# include <math.h>
# include <unistd.h>
# include "global.h"
# include "rand.h"

void fitfun(double *x, int N, double *result, int ncon, double *constraints, int *winfo);

void test_problem (double *xreal, double *xbin, int **gene, double *obj, double *constr)
{
	obj[0] = pow((xreal[0]+2.0),2.0)-10;
	obj[1] = pow((xreal[0]-2.0),2.0)+20;
	return;
}

void test_problem_torc (double *x, int *pnx, double *obj, int *pno, double *constr, int *pncon, double *constr_violation, int info[4])
{
	int nx = *pnx;
	int no = *pno;
	int ncon = *pncon;

	//int popid = info[2];
	//int id = info[3];

	//usleep(10*1000);

	// Option #0 (original): everything here
	obj[0] = pow((x[0]+2.0),2.0)-10;
	obj[1] = pow((x[0]-2.0),2.0)+20;
	// constr[0] = obj[1] - 25;

	// Option #1: fitfun code moved to fitfun.c and constraints stay here
	// fitfun(x, nx, obj, ncon, constr, info);
	//constr[0] = obj[1] - 25;

	// Option #2: fitfun also handles the constraints
	//fitfun(x, nx, obj, ncon, constr, info);

	// the following stays as it was
	if (ncon==0)
	{
		*constr_violation = 0.0;
	}
	else
	{
		*constr_violation = 0.0;

		int j;
		for (j=0; j<ncon; j++)
		{
			if (constr[j]<0.0)
			{
				*constr_violation += constr[j];
			}
		}
	}

	return;
}
