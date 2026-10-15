/*!
 * 
 *
 * \brief       Implements the generational Multi-objective Covariance Matrix Adapation ES.
 * 
 * 
 *
 * \author      T.Voss
 * \date        2010
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
#ifndef SHARK_ALGORITHMS_DIRECT_SEARCH_MOCMA
#define SHARK_ALGORITHMS_DIRECT_SEARCH_MOCMA

// MOO specific stuff
#include <shark/Algorithms/DirectSearch/Operators/Indicators/HypervolumeIndicator.h>
#include <shark/Algorithms/DirectSearch/Operators/Indicators/AdditiveEpsilonIndicator.h>
#include <shark/Algorithms/DirectSearch/Operators/Selection/IndicatorBasedSelection.h>
#include <shark/Algorithms/DirectSearch/Operators/Evaluation/PenalizingEvaluator.h>
#include <shark/Algorithms/DirectSearch/CMA/CMAIndividual.h>


#include <shark/Algorithms/AbstractMultiObjectiveOptimizer.h>
#include <boost/foreach.hpp>
namespace shark {
/**
 * \brief Implements the generational MO-CMA-ES.
 *
 * Please see the following papers for further reference:
 *	- Igel, Suttorp and Hansen. Steady-state Selection and Efficient Covariance Matrix Update in the Multi-Objective CMA-ES.
 *	- Voﬂ, Hansen and Igel. Improved Step Size Adaptation for the MO-CMA-ES.
 */
template<typename Indicator=HypervolumeIndicator>
class IndicatorBasedMOCMA : public AbstractMultiObjectiveOptimizer<RealVector >{
public:
	enum NotionOfSuccess{
		IndividualBased,
		PopulationBased
	};
public:	

	IndicatorBasedMOCMA(random::rng_type& rng= random::globalRng):mpe_rng(&rng) {
		m_individualSuccessThreshold = 0.44;
		initialSigma() = 1.0;
		mu() = 100;
		notionOfSuccess() = PopulationBased;
		this->m_features |= AbstractMultiObjectiveOptimizer<RealVector >::CAN_SOLVE_CONSTRAINED;
	}

	/**
	 * \brief Returns the name of the algorithm.
	 */
	std::string name() const {
		return "MOCMA";
	}
	
	std::size_t mu()const{
		return m_mu;
	}
	std::size_t& mu(){
		return m_mu;
	}
	
        /// \brief The number of points used for initialization of the algorithm.
        std::size_t numInitPoints() const{
                return m_mu;
        }

	double initialSigma()const{
		return m_initialSigma;
	}
	double& initialSigma(){
		return m_initialSigma;
	}
	
	NotionOfSuccess notionOfSuccess()const{
		return m_notionOfSuccess;
	}
	NotionOfSuccess& notionOfSuccess(){
		return m_notionOfSuccess;
	}
	void read( InArchive & archive ){
		archive >> BOOST_SERIALIZATION_NVP(m_parents);
		archive >> BOOST_SERIALIZATION_NVP(m_mu);
		archive >> BOOST_SERIALIZATION_NVP(m_best);
		
		archive >> BOOST_SERIALIZATION_NVP(m_selection);
		archive >> BOOST_SERIALIZATION_NVP(m_notionOfSuccess);
		archive >> BOOST_SERIALIZATION_NVP(m_individualSuccessThreshold);
		archive >> BOOST_SERIALIZATION_NVP(m_initialSigma);
	}
	void write( OutArchive & archive ) const{
		archive << BOOST_SERIALIZATION_NVP(m_parents);
		archive << BOOST_SERIALIZATION_NVP(m_mu);
		archive << BOOST_SERIALIZATION_NVP(m_best);
		
		archive << BOOST_SERIALIZATION_NVP(m_selection);
		archive << BOOST_SERIALIZATION_NVP(m_notionOfSuccess);
		archive << BOOST_SERIALIZATION_NVP(m_individualSuccessThreshold);
		archive << BOOST_SERIALIZATION_NVP(m_initialSigma);
	}
	
	using AbstractMultiObjectiveOptimizer<RealVector >::init;
	/**
	 * \brief Initializes the algorithm for the supplied objective function.
	 * 
	 * \param [in] function The objective function.
	 * \param [in] initialSearchPoints A set of intiial search points.
	 */
	void init(
		ObjectiveFunctionType const& function,
		std::vector<SearchPointType> const& initialSearchPoints
	){
		double t0 = torc_gettime();
		checkFeatures(function);
		std::vector<RealVector> values(initialSearchPoints.size());

		std::cout << "#objectives = " << function.numberOfObjectives() << std::endl;
		std::cout << "#variables = " << function.numberOfVariables() << std::endl;


#if 0
		std::vector<RealVector> searchPoints = initialSearchPoints;

		char user_initial_pop[256];
		strcpy(user_initial_pop, "user_initial_pop.txt");
		FILE *fp = fopen(user_initial_pop, "r");
		if (fp == NULL)
		{
			// nothing to do 
		}
		else
		{
			printf("Initializing first population from user file...\n");
			if (checkpoint_restart == 1)
			{
				printf("Setting checkpoint_restart flag to 0!\n");
				checkpoint_restart = 0;
			}

			//initialize_pop_fp (parent_pop, fp);
			int nx = function.numberOfVariables();
			int np = initialSearchPoints.size();

			for (int i=0; i<np; i++)
			{
				for (int j=0; j<nx; j++) {
					//fscanf(fp, "%lf", &initialSearchPoints[i][j]);
					fscanf(fp, "%lf", &searchPoints[i][j]);
				}
			}

			fclose(fp);
		}
#endif

		for(std::size_t i = 0; i != initialSearchPoints.size(); ++i){
			if(!function.isFeasible(initialSearchPoints[i]))
				throw SHARKEXCEPTION("[MOCMA::init] starting point(s) not feasible");
		}


		// peh: ok, we start 
		numVar = function.numberOfVariables();
		getBounds(lower, upper, numVar);
		getMaxTries(&maxtries);
		

		int nx = function.numberOfVariables();
		int ny = function.numberOfObjectives();
		int np = initialSearchPoints.size();

		double input[np][nx];	// peh: explicit memory allocation for safety
		double output[np][ny];

		if (checkpoint_restart)
		{
			int cmastep = 1;	// was 0
                        char filename[256];
                        sprintf(filename, "curgen_db_%03d.txt", cmastep);
                        FILE *fp = fopen(filename, "r");
                        if (fp != NULL)
                        {
                                for (int i = 0; i < np; i++) {
                                        for (int j = 0; j < nx; j++) {
                                                int r = fscanf(fp, "%le", &input[i][j]);
                                                printf("[%d] input[%d][%d]=%f\n", r, i, j, input[i][j]);
                                                if (r < 0) exit(1);
                                        }
                                        for (int j = 0; j < ny; j++) {
                                                int r = fscanf(fp, "%le", &output[i][j]);
                                                printf("[%d] output[%d][%d]=%f\n", r, i, j, output[i][j]);
                                                if (r < 0) exit(1);
                                        }
                                }
                                fclose(fp);

				for (int i=0; i < np; i++)
				{
					for (int j = 0; j < nx; j++) {
						*((double *)&initialSearchPoints[i][j]) = input[i][j];
					}
				}


                        }
                        else
                        {
				printf("XXXX: checkpoint_restart becomes 0\n");
                                checkpoint_restart = 0;
                        }
                }

		if (!checkpoint_restart)
		{
//#pragma omp parallel for
		int info[4];
		for(std::size_t i = 0; i < np; ++i){
//		for(std::size_t i = 0; i != initialSearchPoints.size(); ++i){
			//values[i] = function.eval(initialSearchPoints[i]);
			info[0] = 0; info[1] = 0; info[2] = 0; info[3] = i;
			
			for (int j=0; j<nx; j++) {
				input[i][j]=initialSearchPoints[i][j];
			}

			torc_create(-1, (void (*)())geval, 5,
				nx, MPI_DOUBLE, CALL_BY_VAL,
				1, MPI_INT, CALL_BY_COP,
				ny, MPI_DOUBLE, CALL_BY_RES,
				1, MPI_INT, CALL_BY_COP,
				4, MPI_INT, CALL_BY_COP,
				input[i], &nx, output[i], &ny, info);
		}
		torc_waitall();

		{
		int cmastep = 1;
		char filename[256];
		FILE *fp;

		sprintf(filename, "curgen_db_%03d.txt", cmastep); 
		fp = fopen(filename, "w");
		for(std::size_t i = 0; i < np; ++i){
			for (int j = 0; j < nx; j++)
				fprintf(fp, "%f ", input[i][j]);
			for (int j = 0; j < ny; j++)
				fprintf(fp, "%f ", output[i][j]);
			fprintf(fp, "\n");
		}
		fclose(fp);

		}

		}

		for(std::size_t i = 0; i != initialSearchPoints.size(); ++i){
			ResultType v(ny);
			for (int j = 0; j < ny; j++) v(j) = output[i][j];
			//v(0) = output[i][0]; v(1) = output[i][1];
			values[i] = v;
		}

		this->doInit(initialSearchPoints,values,mu(),initialSigma() );
		double t1 = torc_gettime();
		printf("XXX init took %f s\n", t1-t0);
	}
	
	/**
	 * \brief Executes one iteration of the algorithm.
	 * 
	 * \param [in] function The function to iterate upon.
	 */
	void step( ObjectiveFunctionType const& function ) {
		static int cmastep = 1;	// was 0
		std::vector<IndividualType> offspring = generateOffspring(cmastep);
		PenalizingEvaluator penalizingEvaluator;

		double t0 = torc_gettime();
#if 0
		penalizingEvaluator( function, offspring.begin(), offspring.end() );
#else
		int nx = function.numberOfVariables();
		int ny = function.numberOfObjectives();
		int np = offspring.size();

		double input[np][nx];
		double output[np][ny];

		std::vector<RealVector> values(offspring.size());

		int info[4];
		cmastep++;

		if (checkpoint_restart)
		{
                        char filename[256];
                        sprintf(filename, "curgen_db_%03d.txt", cmastep);
                        FILE *fp = fopen(filename, "r");
                        if (fp != NULL)
                        {
                                for (int i = 0; i < np; i++) {
#if 1	// this allows us to repeat the sequence of random numbers - useful for checking the restarts
					input[i][0]=offspring[i].searchPoint()[0];
#endif

                                        for (int j = 0; j < nx; j++) {
                                                int r = fscanf(fp, "%le", &input[i][j]);
                                                printf("[%d] input[%d][%d]=%f\n", r, i, j, input[i][j]);
                                                if (r < 0) exit(1);
                                        }
                                        for (int j = 0; j < ny; j++) {
                                                int r = fscanf(fp, "%le", &output[i][j]);
                                                printf("[%d] output[%d][%d]=%f\n", r, i, j, output[i][j]);
                                                if (r < 0) exit(1);
                                        }
                                }
                                fclose(fp);

                                for (int i = 0; i < np; i++) {
					for (int j=0; j<nx; j++) {
						offspring[i].searchPoint()[j] = input[i][j];
					}
				}

                        }
                        else
                        {
				printf("YYYY: checkpoint_restart becomes 0\n");
                                checkpoint_restart = 0;
                        }
                }

		if (!checkpoint_restart)
		{
//#pragma omp parallel for
		for(std::size_t i = 0; i < np; ++i){
//		for(std::size_t i = 0; i != offspring.size(); ++i){
			//if(!function.isFeasible(offspring[i].searchPoint() ))
			//{
			//	std::cout << offspring[i].searchPoint() << std::endl;
			//	throw SHARKEXCEPTION("[MOCMA::xxxx] offspring point(s) not feasible");
			//}
#if 0
			values[i] = function.eval(offspring[i].searchPoint() );
#else
			info[0] = 0; info[1] = 0; info[2] = cmastep; info[3] = i;

			for (int j=0; j<nx; j++) {
				input[i][j]=offspring[i].searchPoint()[j];
			}

			torc_create(-1, (void (*)())geval, 5,
				nx, MPI_DOUBLE, CALL_BY_VAL,
				1, MPI_INT, CALL_BY_COP,
				ny, MPI_DOUBLE, CALL_BY_RES,
				1, MPI_INT, CALL_BY_COP,
				4, MPI_INT, CALL_BY_COP,
				input[i], &nx, output[i], &ny, info);

#endif
		}
		torc_waitall();

		{
		char filename[256];
		FILE *fp;

		sprintf(filename, "curgen_db_%03d.txt", cmastep); 
		fp = fopen(filename, "w");
		for(std::size_t i = 0; i < np; ++i){
			for (int j = 0; j < nx; j++)
				fprintf(fp, "%f ", input[i][j]);
			for (int j = 0; j < ny; j++)
				fprintf(fp, "%f ", output[i][j]);
			fprintf(fp, "\n");
		}
		fclose(fp);

		}

		}

		for(std::size_t i = 0; i != offspring.size(); ++i){
			ResultType v(ny);
			for (int j = 0; j < ny; j++) v(j) = output[i][j];
			values[i] = v;
			//xxx

			offspring[i].penalizedFitness() = values[i];
			offspring[i].unpenalizedFitness() = values[i];
		}
#endif

		updatePopulation(offspring);		// without constraints (bounds)
		double t1 = torc_gettime();
		printf("YYY took step: %f s\n", t1-t0);
	}

        Indicator& indicator(){
                return m_selection.indicator();
        }
        Indicator const& indicator()const{
                return m_selection.indicator();
        }
protected:
	/// \brief The individual type of the SteadyState-MOCMA.
	typedef CMAIndividual<RealVector> IndividualType;

	void doInit(
		std::vector<SearchPointType> const& initialSearchPoints,
		std::vector<ResultType> const& functionValues,
		std::size_t mu,
		double initialSigma
	){
		SIZE_CHECK(initialSearchPoints.size() > 0);
		m_mu = mu;
		m_initialSigma = initialSigma;
		
		m_best.resize( mu );
		m_parents.resize( mu );
		std::size_t noVariables = initialSearchPoints[0].size();

		//if the number of supplied points is smaller than mu, fill everything in
		std::size_t numPoints = 0;
		if(initialSearchPoints.size()<=mu){
			numPoints = initialSearchPoints.size();
			for(std::size_t i = 0; i != numPoints; ++i){
				m_parents[i] = IndividualType(noVariables,m_individualSuccessThreshold,m_initialSigma);
				m_parents[i].searchPoint() = initialSearchPoints[i];
				m_parents[i].penalizedFitness() = functionValues[i];
				m_parents[i].unpenalizedFitness() = functionValues[i];
			}
		}
		//copy points randomly
		for(std::size_t i = numPoints; i != mu; ++i){
			std::size_t index = random::discrete(*mpe_rng,std::size_t(0),initialSearchPoints.size()-1);
			m_parents[i] = IndividualType(noVariables,m_individualSuccessThreshold,m_initialSigma);
			m_parents[i].searchPoint() = initialSearchPoints[index];
			m_parents[i].penalizedFitness() = functionValues[index];
			m_parents[i].unpenalizedFitness() = functionValues[index];
		}
		//create initial mu best points
		for(std::size_t i = 0; i != mu; ++i){
			m_best[i].point = m_parents[i].searchPoint();
			m_best[i].value = m_parents[i].unpenalizedFitness();
		}

		indicator().init(functionValues.front().size(),mu,*mpe_rng);
	}

	std::vector<IndividualType> generateOffspring(int step)const{
		std::vector<IndividualType> offspring(mu());
		for(std::size_t i = 0; i != mu(); ++i){
			std::size_t parentId = i;
			offspring[i] = m_parents[parentId];

                        // WORKAROUND: if parameters exceed bounds
                        bool feasible;
                        unsigned long count = 0;
                        do
                        {
                                count++;
                                feasible = true;

#if 0
				//printf("m_stepSize = %lf, m_lastStep = %lf, m_lastZ = %lf\n", offspring[i].chromosome().m_stepSize, offspring[i].chromosome().m_lastStep,offspring[i].chromosome().m_lastZ);
				printf("===\n offspring %d, count = %d\n", i, count);
				std::cout << i << " m_stepSize = "     << offspring[i].chromosome().m_stepSize << std::endl; 
				std::cout << "m_lastStep = "  << offspring[i].chromosome().m_lastStep << std::endl;
				std::cout << "m_lastZ = "     << offspring[i].chromosome().m_lastZ    << std::endl;

				std::cout << "offspring before "    << offspring[i].searchPoint()  << std::endl;
#endif
                                offspring[i].mutate(*mpe_rng);
#if 0
				std::cout << "offspring after "     << offspring[i].searchPoint()  << std::endl;
#endif

                                for (int j=0; j<numVar; j++) {
                                        feasible &= isFeasible(offspring[i].searchPoint()[j] , j);
                                }
                                if (count == maxtries) break;
                        } while (!feasible);
			printf("%ld samples of individual # %ld were necessary.\n", count, i);

			if (!feasible)
			{
#if 1
                                // WORKAROUND 2: hard set to bounds
				printf("WORKAROUND2 for step %d and individual %ld\n", step, i);
                                for (int j=0; j<numVar; j++) {
                                        offspring[i].searchPoint()[j] = closestFeasible(offspring[i].searchPoint()[j], j);
#else	// TODO: reflect a proposed parameter that is out of bounds
                                // WORKAROUND 3: reflect proposed parameter
				printf("WORKAROUND3 for step %d and individual %d\n", step, i);
                                for (int j=0; j<numVar; j++) {
                                        offspring[i].searchPoint()[j] = reflectFeasible(offspring[i].searchPoint()[j], j);
#endif

                                }
                        }

			offspring[i].parent() = parentId;
		}
		return offspring;
	}
	
	void updatePopulation(  std::vector<IndividualType> const& offspringVec) {
		m_parents.insert(m_parents.end(),offspringVec.begin(),offspringVec.end());
		m_selection( m_parents, mu());
		
		//determine from the selection which parent-offspring pair has been successful
		for (std::size_t i = 0; i < mu(); i++) {
			CMAChromosome::IndividualSuccess offspringSuccess = CMAChromosome::Unsuccessful;
			//new update: an offspring is successful if it is selected
			if ( m_notionOfSuccess == PopulationBased && m_parents[mu()+i].selected()) {
				m_parents[mu()+i].updateAsOffspring();
				offspringSuccess = CMAChromosome::Successful;
			}
			//old update: an offspring is successfull if it is better than its parent
			else if ( m_notionOfSuccess == IndividualBased && m_parents[mu()+i].selected() && m_parents[mu()+i].rank() <= m_parents[i].rank()) {
				m_parents[mu()+i].updateAsOffspring();
				offspringSuccess = CMAChromosome::Successful;
			}
			if(m_parents[i].selected()) 
				m_parents[i].updateAsParent(offspringSuccess);
		}
		
		//partition the selected individuals to the front and remove the unselected ones
		//std::partition(m_parents.begin(), m_parents.end(),IndividualType::IsSelected);
		std::partition(m_parents.begin(), m_parents.end(),[](IndividualType const& ind){return ind.selected();});
		m_parents.erase(m_parents.begin()+mu(),m_parents.end());

		//update solution set
		for (std::size_t i = 0; i < mu(); i++) {
			noalias(m_best[i].point) = m_parents[i].searchPoint();
			m_best[i].value = m_parents[i].unpenalizedFitness();
		}
	}
	
	std::vector<IndividualType> m_parents; ///< Population of size \f$\mu + 1\f$.

private:
	std::size_t m_mu; ///< Size of parent population
	int numVar;
	double lower[1024];
	double upper[1024];
	unsigned long maxtries;

	IndicatorBasedSelection<Indicator> m_selection; ///< Selection operator relying on the (contributing) hypervolume indicator.

	NotionOfSuccess m_notionOfSuccess; ///< Flag for deciding whether the improved step-size adaptation shall be used.
	double m_individualSuccessThreshold;
	double m_initialSigma;
	random::rng_type* mpe_rng;
	// PHILIPP
	// WORKAROUND NUMBER 1, TODO
	bool isFeasible(double x, int i) const {
		if (x < lower[i]) {
			return false;
		}
		if (x > upper[i]) {
			return false;
		}
		else {
			return true;
		}
	}

	// WORKAROUND NUMBER 2, TODO
	double closestFeasible(double x, int i) const {
		if (x<lower[i]) {
			return lower[i];
		}
		if (x>upper[i]) {
			return upper[i];
		}
		else {
			return x;
		}
	}

	// WORKAROUND NUMBER 2, TODO
	double reflectFeasible(double x, int i) const {
		if (x<lower[i]) {
			double delta = lower[i] - x;
			double newx = lower[i] + delta;
			if (newx>upper[i]) {
				//printf("LLL0 %d\n", i);
				newx=lower[i];
			}
			//printf("LLL1 %d\n", i);
			return newx;
		}
		if (x>upper[i]) {
			double delta = x - upper[i];
			double newx = upper[i] - delta;
			if (newx < lower[i]) {
				//printf("UUU0 %d\n", i);
				newx=upper[i];
			}
			//printf("UUU1 %d\n", i);
			return newx;
		}
		else {
			return x;
		}
	}

	void checkFeasible(double x, int i, int j, int act = 0) const {
		if (x<lower[j]) {
			printf("[%d/%d] x < l -> %f < %f\n", i, j, x, lower[j]);
			//if (act == 1) exit(1);
			return;
		}
		if (x>upper[j]) {
			printf("[%d/%d] x > u -> %f > %f\n", i, j, x, upper[j]);
			//if (act == 1) exit(1);
			return;
		}
		//all good
		//printf("[%d/%d] l < x < u\n", i, j);
	}

};

typedef IndicatorBasedMOCMA< HypervolumeIndicator > MOCMA;
typedef IndicatorBasedMOCMA< AdditiveEpsilonIndicator > EpsilonMOCMA;

}

#endif
