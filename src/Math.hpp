/**
 * =====================================================================================
 *
 *    Description:  Corblivar mathematical stuff
 *
 *    Copyright (C) 2013-2016 Johann Knechtel, johann aett jknechtel dot de
 *
 *    This file is part of Corblivar.
 *    
 *    Corblivar is free software: you can redistribute it and/or modify it under the terms
 *    of the GNU General Public License as published by the Free Software Foundation,
 *    either version 3 of the License, or (at your option) any later version.
 *    
 *    Corblivar is distributed in the hope that it will be useful, but WITHOUT ANY
 *    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 *    PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *    
 *    You should have received a copy of the GNU General Public License along with
 *    Corblivar.  If not, see <http://www.gnu.org/licenses/>.
 *
 * =====================================================================================
 */
#ifndef _CORBLIVAR_MATH
#define _CORBLIVAR_MATH

// library includes
#include "Corblivar.incl.hpp"
// Corblivar includes, if any
// forward declarations, if any

/// Corblivar mathematical stuff
class Math {
	// debugging code switch (private)
	private:

	// private data, functions
	private:

	// constructors, destructors, if any non-implicit
	private:
		/// empty default constructor; private in order to avoid instances of ``static'' class
		Math() {
		}

	// public data, functions
	public:
		/// small epsilon value, usually to be considered in order to avoid
		/// division by zero
		static constexpr double epsilon = 1.0e-10;

		/// random-number functions
		/// note: range is [min, max)
		inline static int randI(int const& min, int const& max) {
			if (max == min) {
				return min;
			}
			else {
				return min + (rand() % (max - min));
			}
		};
		/// random decision
		inline static bool randB() {
			int const r = rand();
			return (r < (RAND_MAX / 2));
		};
		/// random-number functions
		/// note: range is [min, max)
		inline static double randF(double const& min, double const& max) {
			double const r = static_cast<double>(rand()) / RAND_MAX;
			return r * (max - min) + min;
		};

		/// standard deviation of samples
		inline static double stdDev(std::vector<double> const& samples) {

			// determine std dev; sqrt of variance
			return std::sqrt(variance(samples));
		};

		inline static double variance(std::vector<double> const& samples) {
			double avg, sq_diffs;

			// determine avg of samples
			avg = 0.0;
			for (double const& s : samples) {
				avg += s;
			}
			avg /= samples.size();

			// determine sum of squared diffs
			sq_diffs = 0.0;
			for (double const& s : samples) {
				sq_diffs += std::pow(s - avg, 2.0);
			}

			// return variance
			return (sq_diffs / samples.size());
		};

		/// 1D gauss function; used for separated convolution w/ 2D gauss function,
		/// provides the impulse response function for power blurring
		inline static double gauss1D(double const& value, double const& factor, double const& spread) {
			return factor * exp(-(1.0 / spread) * std::pow(value, 2.0));
		};

		/// comparison of double values, allows minor deviation
		inline static bool looseDoubleComp(double const& d1, double const& d2, double const& precision = 1.0e-03) {
			return std::abs(d1 - d2) < precision;
		};

		/// factor to scale um downto m;
		static constexpr double SCALE_UM_M = 1.0e-06;
};

#endif
