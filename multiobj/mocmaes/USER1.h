//===========================================================================
/*!
 * 
 *
 * \brief       Multi-objective optimization benchmark function ZDT1
 * 
 * The function is described in
 * 
 * Eckart Zitzler, Kalyanmoy Deb, and Lothar Thiele. Comparison of
 * Multiobjective Evolutionary Algorithms: Empirical
 * Results. Evolutionary Computation 8(2):173-195, 2000
 * 
 * 
 *
 * \author      -
 * \date        -
 *
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
//===========================================================================


#ifndef SHARK_OBJECTIVEFUNCTIONS_USER1_H
#define SHARK_OBJECTIVEFUNCTIONS_USER1_H

#include <shark/ObjectiveFunctions/AbstractObjectiveFunction.h>
#include <shark/ObjectiveFunctions/BoxConstraintHandler.h>

namespace shark {
/*! \brief Multi-objective optimization benchmark function USER1
*
*/

/* multi objective fitness function as used in nsga2, x is the input,
 * N is input dimension, winfo is info, result corresponds to value
 *
	void fitfun(double *x, int N, void *output, int *winfo, double *result)
*
*/

extern "C"
{
	extern void fitfun(double *x, int N, void *output, int *winfo, double *result);
}

#if 1
static void geval(double *x, int *p_nx, double *output, int *p_no, int info[4])
{
	int nx = *p_nx;
	fitfun(x, nx, NULL, info, output);
}
#else
static void geval(double *x, int *p_nx, double *value, int *p_ny, int *info)
{
        int nx = *p_nx;
        int ny = *p_ny;

//     	usleep(100*1000);
        value[0] = pow((x[0]+2.0),2.0)-10;
        value[1] = pow((x[0]-2.0),2.0)+20;
}
#endif


//void getBounds(RealVector& lower, RealVector& upper, std::size_t numberOfVariables)
void getBounds(double *lower, double *upper, std::size_t numberOfVariables)
{
//	printf("getBounds: numberOfVariables = %d\n", numberOfVariables);
	for (int i = 0; i < numberOfVariables; i++)
	{
		lower[i] = mocma_params.lowerbound[i];
		upper[i] = mocma_params.upperbound[i];

	}
}


void getMaxTries(unsigned long *val)
{
	*val = mocma_params.maxtries;
}


struct USER1  : public MultiObjectiveFunction
{
	
	USER1(std::size_t numVariables = 0) :  m_handler(numVariables,-10,10) {
		std::cout <<" numV = " << numVariables << std::endl;
		announceConstraintHandler(&m_handler);
	}

	/// \brief From INameable: return the class name.
	std::string name() const
	{ return "USER1"; }

	std::size_t numberOfObjectives()const{
		return 2;
	}
	
	std::size_t numberOfVariables()const{
		return m_handler.dimensions();
	}
	
	bool hasScalableDimensionality()const{
		return true;
	}

	SearchPointType proposeStartingPoint() const {
		int numVar = numberOfVariables();
		RealVector x(numVar);
		double lb[numVar], ub[numVar];
		getBounds(lb, ub, numVar);

#if 0	// TODO: shrink bounds for the initial samples
		double shrink_factor = mocma_params.shrink_factor; 
		double mean, width;
		double s_lb[numVar], s_ub[numVar];

		for (std::size_t i = 0; i < x.size(); i++) {
			mean = 0.5*(ub[i]-lb[i]);
			width = shrink_factor*(ub[i]-lb[i]);
			s_lb[i] = mean - 0.5*width;
			s_ub[i] = mean + 0.5*width;
		}
		for (std::size_t i = 0; i < x.size(); i++) {
			x(i) = Rng::uni(s_lb[i],s_ub[i]);
		}
#else

		for (std::size_t i = 0; i < x.size(); i++) {
			//x(i) = Rng::uni(lb[i],ub[i]);
			x(i) = shark::random::uni(shark::random::globalRng, lb[i], ub[i]);
		}
#endif
		return x;
	}


	/// \brief Adjusts the number of variables if the function is scalable.
	/// \param [in] numberOfVariables The new dimension.
	void setNumberOfVariables( std::size_t numberOfVariables ){
			printf("setting bounds..\n");
			SearchPointType lower;
			SearchPointType upper;
			lower.resize(numberOfVariables);
			upper.resize(numberOfVariables);

			double l[numberOfVariables], u[numberOfVariables];
			getBounds(l, u, numberOfVariables);
			
			for (int i = 0; i < numberOfVariables; i++)
			{
				lower(i) = l[i];
				upper(i) = u[i];
			}
			m_handler.setBounds(lower, upper);
	}


	bool isFeasible( const SearchPointType & input) const {
//		printf("calling isFeasible...\n");
		size_t numVar = numberOfVariables();
		double l[numVar], u[numVar];
		getBounds(l, u, numVar);

		SIZE_CHECK(input.size() == numberOfVariables());
		for(std::size_t i = 0; i != numberOfVariables(); ++i){
			if (input(i) < l[i] || input(i) > u[i])
				return false;
		}
		return true;
	}


	// constraints
	bool isConstrained() {
		return true;
	}

	bool hasConstraintHandler() {
		return true;
	}

#if 0
	ResultType eval( const SearchPointType & x ) const {
		m_evaluationCounter++;

		ResultType value( 2 );

		usleep(10*1000);

		value[0] = pow((x(0)+2.0),2.0)-10;
		value[1] = pow((x(0)-2.0),2.0)+20;

		return value;
	}
#endif


private:
	BoxConstraintHandler<SearchPointType> m_handler;
};

}
#endif
