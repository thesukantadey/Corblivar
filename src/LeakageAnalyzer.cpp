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

double LeakageAnalyzer::determineSpatialEntropy(int const& layers,
		std::vector< std::array< std::array<ThermalAnalyzer::PowerMapBin, ThermalAnalyzer::THERMAL_MAP_DIM>, ThermalAnalyzer::THERMAL_MAP_DIM> > const& power_maps_orig) {

	double d_int;
	double d_ext;
	double cur_entropy, entropy, overall_entropy;
	double ratio_bins;

	overall_entropy = 0.0;

	// first, the power maps have to be partitioned/classified
	//
	this->partitionPowerMaps(layers, power_maps_orig);

	// calculate spatial entropy for each layer separately
	//
	for (int layer = 0; layer < layers; layer++) {

		entropy = 0.0;

		if (DBG_BASIC) {
			std::cout << "DBG> Partitions on layer " << layer << ": " << this->power_partitions[layer].size() << std::endl;
		}

		// calculate the avg internal and external distances for all partitions
		//
		for (auto const& cur_part : this->power_partitions[layer]) {

			// internal distance: distance between all elements in same partition
			//
			d_int = 0.0;
			for (auto const& b1 : cur_part.second) {
				for (auto const& b2 : cur_part.second) {

					// ignore comparison with itself
					if (b1.x == b2.x && b1.y == b2.y) {
						continue;
					}

					// Manhattan distance should suffice for grid coordinates/distances
					//
					d_int += std::abs(b1.x - b2.x) + std::abs(b1.y - b2.y);
				}
			}
			// normalize to obtain avg dist; over all compared pairs of elements
			d_int /= (cur_part.second.size() * (cur_part.second.size() - 1));

			// external distance: distance between all elements in this current partition and all elements in all other partitions
			//
			d_ext = 0.0;
			for (auto const& b1 : cur_part.second) {

				for (auto const& other_part : this->power_partitions[layer]) {

					// ignore comparison with same partition, check their id
					if (cur_part.first == other_part.first) {
						continue;
					}

					// calculate distance to every element of other partition
					for (auto const& b2 : other_part.second) {

						// Manhattan distance should suffice for grid coordinates/distances
						//
						d_ext += std::abs(b1.x - b2.x) + std::abs(b1.y - b2.y);
					}
				}
			}
			// normalize to obtain avg dist; over all compared pairs of elements
			d_ext /= (cur_part.second.size() *
					// size of all other partitions taken together, equals whole grid minus this partition
					(std::pow(ThermalAnalyzer::THERMAL_MAP_DIM, 2) - cur_part.second.size())
				);

			// calculate the partial entropy for this partition
			//
			ratio_bins = cur_part.second.size() / std::pow(ThermalAnalyzer::THERMAL_MAP_DIM, 2);
			cur_entropy = (d_int / d_ext) * ratio_bins * std::log2(ratio_bins);

			// dbg logging
			if (DBG) {
				std::cout << "DBG>  Partition: " << cur_part.first << std::endl;
				std::cout << "DBG>   Avg internal dist: " << d_int << std::endl;
				std::cout << "DBG>   Avg external dist: " << d_ext << std::endl;
				std::cout << "DBG>   Partial entropy: " << cur_entropy << std::endl;
			}

			// sum up the entropy; consider current's partition impact
			//
			entropy += cur_entropy;
		}

		// entropy has negative sign
		entropy *= -1;

		// sum up entropy over layers
		overall_entropy += entropy;

		// dbg logging
		if (DBG_BASIC) {
			std::cout << "DBG> Overall entropy on layer " << layer << ": " << entropy << std::endl;
		}
	}

	// return avg entropy
	return (overall_entropy / layers);
}

void LeakageAnalyzer::partitionPowerMaps(int const& layers,
		std::vector< std::array< std::array<ThermalAnalyzer::PowerMapBin, ThermalAnalyzer::THERMAL_MAP_DIM>, ThermalAnalyzer::THERMAL_MAP_DIM> > const& power_maps_orig) {
	double power_avg;
	double power_std_dev;
	unsigned m;
	// layer-wise power values
	std::vector<Bin> power_values;

	this->power_partitions.clear();

	for (int layer = 0; layer < layers; layer++) {

		// allocate empty vector for current layer
		this->power_partitions.push_back(
				// vector of partitions in this layer
				std::vector< std::pair<std::string, std::vector<Bin>> >()
			);

		// put power values along with their coordinates into vector; also track avg power
		//
		power_values.clear();
		power_values.reserve(std::pow(ThermalAnalyzer::THERMAL_MAP_DIM, 2));
		power_avg = 0.0;
		for (int x = 0; x < ThermalAnalyzer::THERMAL_MAP_DIM; x++) {
			for (int y = 0; y < ThermalAnalyzer::THERMAL_MAP_DIM; y++) {

				power_values.push_back({
						x, y,
						power_maps_orig[layer][x][y].power_density
					});

				power_avg += power_values.back().value;
			}
		}
		power_avg /= std::pow(ThermalAnalyzer::THERMAL_MAP_DIM, 2);

		// sort vector according to power values
		std::sort(power_values.begin(), power_values.end(),
				// lambda expression
				[&](Bin b1, Bin b2) {
					return b1.value < b2.value;
				}
			 );

		if (DBG_VERBOSE) {
			for (auto const& p : power_values) {
				std::cout << "DBG>  Power[" << p.x << "][" << p.y << "]: " << p.value << std::endl;
			}
		}

		// determine first cut: index of first value larger than avg
		for (m = 0; m < power_values.size(); m++) {
			
			if (power_values[m].value > power_avg) {
				break;
			}
		}

		// start recursive calls; partition these two ranges iteratively further
		//
		// note that upper-boundary element is left out for actual calculations, but required as upper boundary for traversal of data structures
		this->partitionPowerMapHelper(0, m, layer, power_values);
		this->partitionPowerMapHelper(m, power_values.size(), layer, power_values);

		// now, all partitions along with their power bins are determined and stored in power_partitions
		//


		// dbg logging
		if (DBG) {
			std::cout << "DBG> Partitions on layer " << layer << ": " << this->power_partitions[layer].size() << std::endl;

			for (auto const& cur_part : this->power_partitions[layer]) {

				// determine avg power for current partition
				power_avg = 0.0;
				for (auto const& bin : cur_part.second) {
					power_avg += bin.value;
				}
				power_avg /= cur_part.second.size();

				// determine sum of squared diffs for std dev
				power_std_dev = 0.0;
				for (auto const& bin : cur_part.second) {
					power_std_dev += std::pow(bin.value - power_avg, 2.0);
				}
				// determine std dev
				power_std_dev /= cur_part.second.size();
				power_std_dev = std::sqrt(power_std_dev);
				
				std::cout << "DBG>  Partition: " << cur_part.first << std::endl;
				std::cout << "DBG>   Size: " << cur_part.second.size() << std::endl;
				std::cout << "DBG>   Std dev power: " << power_std_dev << std::endl;
				std::cout << "DBG>   Avg power: " << power_avg << std::endl;
				// min value is represented by first bin, since the underlying data of power_values was sorted by power
				std::cout << "DBG>   Min power: " << cur_part.second.front().value << std::endl;
				// max value is represented by last bin, since the underlying data of power_values was sorted by power
				std::cout << "DBG>   Max power: " << cur_part.second.back().value << std::endl;

				if (DBG_VERBOSE) {
					for (auto const& bin : cur_part.second) {
						std::cout << "DBG>   Power[" << bin.x << "][" << bin.y << "]: " << bin.value << std::endl;
					}
				}
			}
		}
	}
}

/// note that power_partitions are updated in this function
inline void LeakageAnalyzer::partitionPowerMapHelper(unsigned const& lower_bound, unsigned const& upper_bound, int const& layer, std::vector<Bin> const& power_values) {
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
		avg += power_values[i].value;
	}
	avg /= range;

	// determine sum of squared diffs for std dev
	std_dev = 0.0;
	for (i = lower_bound; i < upper_bound; i++) {
		std_dev += std::pow(power_values[i].value - avg, 2.0);
	}
	// determine std dev
	std_dev /= range;
	std_dev = std::sqrt(std_dev);
	
	if (DBG_VERBOSE) {
		std::cout << "DBG> Current range: " << lower_bound << ", " << upper_bound - 1 << std::endl;
		std::cout << "DBG>  Std dev: " << std_dev << std::endl;
		std::cout << "DBG>  Avg: " << avg << std::endl;
		std::cout << "DBG>  Min: " << power_values[lower_bound].value << std::endl;
		std::cout << "DBG>  Max: " << power_values[upper_bound - 1].value << std::endl;
	}

	// determine (potential) cut: index of first value larger than avg
	for (m = lower_bound; m < upper_bound; m++) {
		
		if (power_values[m].value > avg) {
			break;
		}
	}

	// check break criterion for recursive partitioning;
	//
	if (
			// ideal case: std dev of this partition is reaching zero
			Math::doubleComp(0.0, std_dev) ||
			// also maintain (a ``soft'', see below) minimal partition size
			//
			// note that some partitions may be much smaller in case their previous cut was largely skewed towards one boundary; a possible countermeasure here would be
			// to implement the check as look-ahead, but this also triggers some partitions to have a rather large leakage in practice
			(range < MIN_PARTITION_SIZE) ||
			// look-ahead checks still required, to avoid trivial sub-partitions with only one element
			((m - lower_bound) == 1) ||
			((upper_bound - m) == 1)
	   ) {

		// if criterion reached, then memorize this current partition as new partition
		//
		std::vector<Bin> partition;
		for (i = lower_bound; i < upper_bound; i++) {
			partition.push_back(power_values[i]);
		}
		this->power_partitions[layer].push_back(
				std::pair<std::string, std::vector<Bin>>(
					std::string(std::to_string(lower_bound) + "," + std::to_string(upper_bound)),
					partition
				));
		
		return;
	}
	// continue recursively
	//
	else {

		// recursive call for the two new sub-partitions
		// note that upper-boundary element is left out for actual calculations, but required as upper boundary for traversal of data structures
		this->partitionPowerMapHelper(lower_bound, m, layer, power_values);
		this->partitionPowerMapHelper(m, upper_bound, layer, power_values);
	}
}

double LeakageAnalyzer::determinePearsonCorr(std::array< std::array<ThermalAnalyzer::PowerMapBin, ThermalAnalyzer::THERMAL_MAP_DIM>, ThermalAnalyzer::THERMAL_MAP_DIM> const& power_map, std::array< std::array<ThermalAnalyzer::ThermalMapBin, ThermalAnalyzer::THERMAL_MAP_DIM>, ThermalAnalyzer::THERMAL_MAP_DIM> const* thermal_map) {
	double avg_power, avg_temp;
	double max_temp;
	double std_dev_power, std_dev_temp;
	double cov;
	double cur_power_dev, cur_temp_dev;
	double correlation;

	// sanity check for thermal map
	if (thermal_map == nullptr) {
		return std::nan(nullptr);
	}

	avg_power = avg_temp = 0.0;
	max_temp = 0.0;
	cov = std_dev_power = std_dev_temp = 0.0;
	correlation = 0.0;

	// first pass: determine avg values
	//
	for (int x = 0; x < ThermalAnalyzer::THERMAL_MAP_DIM; x++) {
		for (int y = 0; y < ThermalAnalyzer::THERMAL_MAP_DIM; y++) {

			avg_power += power_map[x][y].power_density;
			avg_temp += (*thermal_map)[x][y].temp;
			max_temp = std::max(max_temp, (*thermal_map)[x][y].temp);
		}
	}
	avg_power /= std::pow(ThermalAnalyzer::THERMAL_MAP_DIM, 2);
	avg_temp /= std::pow(ThermalAnalyzer::THERMAL_MAP_DIM, 2);

	// dbg output
	if (DBG) {
		std::cout << "DBG> Avg power: " << avg_power << std::endl;
		std::cout << "DBG> Avg temp: " << avg_temp << std::endl;
		std::cout << "DBG> Max temp: " << max_temp << std::endl;
		std::cout << std::endl;
	}
	
	// second pass: determine covariance and standard deviations
	//
	for (int x = 0; x < ThermalAnalyzer::THERMAL_MAP_DIM; x++) {
		for (int y = 0; y < ThermalAnalyzer::THERMAL_MAP_DIM; y++) {

			// deviations of current values from avg values
			cur_power_dev = power_map[x][y].power_density - avg_power;
			cur_temp_dev = (*thermal_map)[x][y].temp - avg_temp;

			// covariance
			cov += cur_power_dev * cur_temp_dev;

			// standard deviation, calculate its sqrt later on
			std_dev_power += std::pow(cur_power_dev, 2.0);
			std_dev_temp += std::pow(cur_temp_dev, 2.0);
		}
	}
	cov /= std::pow(ThermalAnalyzer::THERMAL_MAP_DIM, 2);
	std_dev_power /= std::pow(ThermalAnalyzer::THERMAL_MAP_DIM, 2);
	std_dev_temp /= std::pow(ThermalAnalyzer::THERMAL_MAP_DIM, 2);

	std_dev_power = std::sqrt(std_dev_power);
	std_dev_temp = std::sqrt(std_dev_temp);

	// calculate Pearson correlation: covariance over product of standard deviations
	//
	correlation = cov / (std_dev_power * std_dev_temp);

	// dbg output
	if (DBG) {
		std::cout << "DBG> Standard deviation of power: " << std_dev_power << std::endl;
		std::cout << "DBG> Standard deviation of temp: " << std_dev_temp << std::endl;
		std::cout << "DBG> Covariance of temp and power: " << cov << std::endl;
		std::cout << std::endl;
	}
	if (DBG_BASIC) {
		std::cout << "DBG> Pearson correlation of temp and power: " << correlation << std::endl;
	}

	return correlation;
}
