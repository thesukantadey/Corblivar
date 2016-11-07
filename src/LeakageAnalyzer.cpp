/*
 * =====================================================================================
 *
 *    Description:  Corblivar thermal-related side-channel leakage analyzer, based on
 *    Pearson correlation of power and thermal maps and spatial entropy of power maps
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

// own Corblivar header
#include "LeakageAnalyzer.hpp"
// required Corblivar headers
#include "ThermalAnalyzer.hpp"

void LeakageAnalyzer::partitionPowerMaps(int const& layers,
		std::vector< std::array< std::array<ThermalAnalyzer::PowerMapBin, ThermalAnalyzer::THERMAL_MAP_DIM>, ThermalAnalyzer::THERMAL_MAP_DIM> > const& power_maps_orig) {
	double power_avg;
	double power_std_dev;
	unsigned m;
	std::vector<Point> bins_partition;
	std::vector< std::vector<Point> > partitions;

	this->power_partitions.clear();
	this->power_partitions.reserve(layers);

	for (int layer = 0; layer < layers; layer++) {

		// put power values along with their coordinates into vector; also track avg power
		//
		this->power_values.clear();
		this->power_values.reserve(std::pow(ThermalAnalyzer::THERMAL_MAP_DIM, 2));
		power_avg = 0.0;
		for (int x = 0; x < ThermalAnalyzer::THERMAL_MAP_DIM; x++) {
			for (int y = 0; y < ThermalAnalyzer::THERMAL_MAP_DIM; y++) {

				this->power_values.push_back( std::pair<double, Point>(
							power_maps_orig[layer][x][y].power_density,
							Point(x, y)
						)
					);

				power_avg += power_maps_orig[layer][x][y].power_density;
			}
		}
		power_avg /= std::pow(ThermalAnalyzer::THERMAL_MAP_DIM, 2);

		// sort vector according to power values
		std::sort(this->power_values.begin(), this->power_values.end(),
				// lambda expression
				[&](std::pair<double, Point> p1, std::pair<double, Point> p2) {
					return p1.first < p2.first;
				}
			 );

		if (DBG_VERBOSE) {
			for (auto const& p : this->power_values) {
				std::cout << "DBG>  Power[" << p.second.x << "][" << p.second.y << "]: " << p.first << std::endl;
			}
		}

		// determine first cut: index of first value larger than avg
		for (m = 0; m < this->power_values.size(); m++) {
			
			if (this->power_values[m].first > power_avg) {
				break;
			}
		}

		// start recursive calls; partition these two ranges iteratively further
		this->partition_ranges.clear();
		// note that upper-boundary element is left out for actual calculations, but required as upper boundary for traversal of data structures
		this->partitionPowerMapHelper(0, m);
		this->partitionPowerMapHelper(m, this->power_values.size());

		// now, all partitions along with their power bins are determined
		// store them in separate, public data structure
		//

		// walk ranges of all partitions on this layer
		partitions.clear();
		for (auto const& p : this->partition_ranges) {

			// copy power bin coordinates/indices for current partition
			bins_partition.clear();
			for (unsigned i = p.first; i < p.second; i++) {
				bins_partition.push_back(this->power_values[i].second);
			}

			partitions.push_back(bins_partition);
		}

		// store all partitions of this layer
		this->power_partitions.push_back(partitions);


		// dbg logging
		if (DBG) {
			std::cout << "DBG> Partitions on layer " << layer << ": " << this->power_partitions[layer].size() << std::endl;

			for (auto const& cur_part : this->power_partitions[layer]) {

				// determine avg power for current partition
				power_avg = 0.0;
				for (auto const& bin : cur_part) {
					power_avg += power_maps_orig[layer][bin.x][bin.y].power_density;
				}
				power_avg /= cur_part.size();

				// determine sum of squared diffs for std dev
				power_std_dev = 0.0;
				for (auto const& bin : cur_part) {
					power_std_dev += std::pow(power_maps_orig[layer][bin.x][bin.y].power_density - power_avg, 2.0);
				}
				// determine std dev
				power_std_dev /= cur_part.size();
				power_std_dev = std::sqrt(power_std_dev);
				
				std::cout << "DBG>  Partition" << std::endl;
				std::cout << "DBG>   Size: " << cur_part.size() << std::endl;
				std::cout << "DBG>   Std dev power: " << power_std_dev << std::endl;
				std::cout << "DBG>   Avg power: " << power_avg << std::endl;
				// min value is represented by first bin, since the underlying data of power_values was sorted by power
				std::cout << "DBG>   Min power: " << power_maps_orig[layer][cur_part.front().x][cur_part.front().y].power_density << std::endl;
				// max value is represented by last bin, since the underlying data of power_values was sorted by power
				std::cout << "DBG>   Max power: " << power_maps_orig[layer][cur_part.back().x][cur_part.back().y].power_density << std::endl;

				if (DBG_VERBOSE) {
					for (auto const& bin : cur_part) {
						std::cout << "DBG>   Power[" << bin.x << "][" << bin.y << "]: " << power_maps_orig[layer][bin.x][bin.y].power_density << std::endl;
					}
				}
			}
		}
	}
}

/// note that partition_ranges are updated in this function
inline void LeakageAnalyzer::partitionPowerMapHelper(unsigned lower_bound, unsigned upper_bound) {
	double avg, std_dev;
	unsigned range;
	unsigned m, i;

	// sanity check for proper ranges
	if (upper_bound <= lower_bound) {
		return;
	}

	range = upper_bound - lower_bound;

	// determine avg power for given data range
	avg = 0.0;
	for (i = lower_bound; i < upper_bound; i++) {
		avg += this->power_values[i].first;
	}
	avg /= range;

	// determine sum of squared diffs for std dev
	std_dev = 0.0;
	for (i = lower_bound; i < upper_bound; i++) {
		std_dev += std::pow(this->power_values[i].first - avg, 2.0);
	}
	// determine std dev
	std_dev /= range;
	std_dev = std::sqrt(std_dev);
	
	if (DBG_VERBOSE) {
		std::cout << "DBG> Current range: " << lower_bound << ", " << upper_bound - 1 << std::endl;
		std::cout << "DBG>  Std dev: " << std_dev << std::endl;
		std::cout << "DBG>  Avg: " << avg << std::endl;
		std::cout << "DBG>  Min: " << this->power_values[lower_bound].first << std::endl;
		std::cout << "DBG>  Max: " << this->power_values[upper_bound - 1].first << std::endl;
	}

	// check break criterion for recursive partitioning;
	//
	if (
			// ideal case: std dev of this partition is reaching zero
			Math::doubleComp(0.0, std_dev) ||
			// also maintain (a ``soft'', see below) minimal partition size
			//
			// note that some partitions may be much smaller in case their previous cut was largely skewed towards one boundary; a possible mitigation here would be to
			// implement the check as look-ahead, but this also triggers some partitions to have a rather large leakage in practice
			(range < LeakageAnalyzer::MIN_PARTITION_SIZE)
			// look-ahead may increase std dev too much
//			((m - lower_bound) < LeakageAnalyzer::MIN_PARTITION_SIZE) ||
//			((upper_bound - m) < LeakageAnalyzer::MIN_PARTITION_SIZE)
	   ) {

		// if criterion reached, then memorize this current partition
		this->partition_ranges.push_back( std::pair<unsigned, unsigned>(lower_bound, upper_bound) );
		
		return;
	}
	// continue recursively
	//
	else {
		// determine cut: index of first value larger than avg
		for (m = lower_bound; m < upper_bound; m++) {
			
			if (this->power_values[m].first > avg) {
				break;
			}
		}

		// recursive call for the two new sub-partitions
		// note that upper-boundary element is left out for actual calculations, but required as upper boundary for traversal of data structures
		this->partitionPowerMapHelper(lower_bound, m);
		this->partitionPowerMapHelper(m, upper_bound);
	}
}