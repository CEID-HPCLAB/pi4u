#include <math.h>
#include <unistd.h>

//#ifdef __cplusplus
//extern "C" {
//#endif

void fitfun(double *x, int nx, double *result, int *winfo)	// ny must be also available
{
	usleep(10*1000);

//	result[0] = pow((x[0]+2.0),2.0)-10;
//	result[1] = pow((x[0]-2.0),2.0)+20;

	double y = 0;

	int i;
	for (i = 0; i < nx; i++) y += x[i];

	result[0] = pow((y+2.0),2.0)-10;
	result[1] = pow((y-2.0),2.0)+20;
}

//#ifdef __cplusplus
//}
//#endif

