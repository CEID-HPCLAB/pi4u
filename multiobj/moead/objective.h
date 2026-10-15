#ifndef __OBJECTIVE_H_
#define __OBJECTIVE_H_

#include "global.h"
#include <unistd.h>
#define pi   3.1415926
#define SQR2  sqrt(2)

//extern "C" 
extern void fitfun(double *x, int nx, double *result, int *winfo);


void objectives(vector<double> x_var, vector <double> &y_obj, int info[4])
{
//	usleep(1*1000);
//	y_obj[0] = pow((x_var[0]+2.0),2.0)-10;
//	y_obj[1] = pow((x_var[0]-2.0),2.0)+20;

	fitfun(x_var.data(), numVariables, y_obj.data(), info);
}

#endif
