/**
 * =====================================================================================
 *
 *    Description:  Corblivar handler for multiple voltages
 *
 *    Copyright (C) 2015-2016 Johann Knechtel, johann aett jknechtel dot de
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
#ifndef _CORBLIVAR_MULTIPLEVOLTAGES
#define _CORBLIVAR_MULTIPLEVOLTAGES

// library includes
#include "Corblivar.incl.hpp"
// Corblivar includes, if any
#include "ContiguityAnalysis.hpp"
// forward declarations, if any
class Block;
class Rect;
class Net;

/// Corblivar handler for multiple voltages
class MultipleVoltages {
	public:
		/// debugging code switch (public)
		static constexpr bool DBG = false;
		/// debugging code switch (public)
		static constexpr bool DBG_VERBOSE = false;
		/// debugging code switch (public)
		static constexpr bool DBG_FLOORPLAN = false;

		/// dimension for feasible voltages;
		/// represents the upper bound for globally available voltages
		static constexpr int MAX_VOLTAGES = 4;

	// public POD, to be declared early on
	public:
		struct Parameters {

			/// voltages and related scaling factors for power consumption and
			/// module delays
			std::vector<double> voltages;
			/// voltages and related scaling factors for power consumption and
			/// module delays
			std::vector<double> voltages_power_factors;
			/// voltages and related scaling factors for power consumption and
			/// module delays
			std::vector<double> voltages_delay_factors;

			/// global layer/die count
			int layers;

			/// internal weights, used for internal cost terms
			double weight_power_saving;
			/// internal weights, used for internal cost terms
			double weight_corners;
			/// internal weights, used for internal cost terms
			double weight_level_shifter;
			/// internal weights, used for internal cost terms
			double weight_modules_count;
			/// internal weights, used for internal cost terms
			double weight_power_variation;
		} parameters;

		/// max evaluation values have to memorized as well, in order to enable
		/// comparison during different SA iterations
		struct max_values {
			double inv_power_saving, corners_avg;
			unsigned module_count;
			unsigned level_shifter;
			double power_variation_max;
		} max_values;

	/// inner class of compound modules, to be declared early on
	class CompoundModule {

		// private data
		private:
			friend class MultipleVoltages;
			friend class IO;

			/// comprised blocks
			std::vector<Block const*> blocks;

			/// flags to encode assigned blocks: each block encoded by its
			/// numerical id will result in a `true' flag at the index related
			/// to its numerical id
			std::vector<bool> block_ids;

			/// die-wise bounding boxes for whole module; only the set/vector of
			/// by other blocks not covered partial boxes are memorized; thus,
			/// the die-wise voltage islands' proper outlines are captured here
			std::vector< std::vector<Rect> > outline;

			/// (local) cost term: outline cost is ratio of (by other blocks
			/// with non-compatible voltage) intruded area of the module's bb;
			/// the lower the better; current cost value is calculated via
			/// updateOutlineCost()
			///
			double outline_cost = 0.0;

			/// container for estimated max number of corners in power rings
			/// per die
			///
			/// each rectangle / partial bb of the die outline will introduce
			/// four corners; however, if for example two rectangles are
			/// sharing a boundary then only six corners are found for these
			/// two rectangles; thus, we only consider the unique boundaries
			/// and estimate that each unique boundary introduces two corners;
			/// a shared boundary may arise in vertical or horizontal
			/// direction, the actual number of corners will be given by the
			/// maximum of both estimates
			std::vector<unsigned> corners_powerring;

			/// feasible voltages for whole module; defined by intersection of
			/// all comprised blocks
			std::bitset<MAX_VOLTAGES> feasible_voltages;

			/// power values will be memorized locally;
			/// to avoid redundant recalculations, these values will only be updated whenever the
			/// set of feasible_voltages changes
			///
			double power_saving_avg_;
			double power_saving_wasted_avg_;
			/// the avg values are kept layer-wise; first element in pair encodes the number of blocks in layer, second element the avg power densities of those blocks
			std::vector< std::pair<int, double> > power_dens_avg_;
			/// the std dev values are kept layer-wise; only the values are kept, the blocks count is already kept in power_dens_avg_
			std::vector< double > power_std_dev_;

			/// values for level shifters will be memorized locally;
			/// to avoid redundant recalculations, these values will only be updated whenever updateLevelShifter() is called
			unsigned level_shifter_upper_bound = 0;
			unsigned level_shifter_actual = 0;
			
			/// memorize global cost locally, to avoid recalculation
			double cost = -1;

			/// key: neighbour's numerical block id
			///
			/// with an map, redundant neighbours which may arise during
			/// stepwise build-up of compound modules are ignored; order not
			/// required and thus related computations be can avoided
			///
			std::unordered_map<unsigned, ContiguityAnalysis::ContiguousNeighbour*> contiguous_neighbours;

		// private functions
		private:
			/// local cost; required during bottom-up construction
			double updateOutlineCost(ContiguityAnalysis::ContiguousNeighbour* neighbour, ContiguityAnalysis& cont, bool apply_update = true);

			/// helper function to return string comprising all (sorted) block ids
			std::string id() const;

			/// helper to return index of minimal assignable voltage, note that
			/// this is not necessarily the globally minimal index but rather
			/// depends on the intersection of all comprised blocks' voltages
			inline unsigned min_voltage_index() const {

				for (unsigned v = 0; v < MAX_VOLTAGES; v++) {

					if (this->feasible_voltages[v]) {
						return v;
					}
				}

				return MAX_VOLTAGES - 1;
			}

			/// helper to count all required level shifters; whenever a nets is connecting from this compound module to another one with a different voltage setting, we
			/// need to account for a level shifter
			///
			/// note that the count of level shifters is calculated in two versions; before actual top-down selection of compound modules, we shall estimate the level
			/// shifters as upper bound (upper_bound = true), i.e., we assume that any net passing to another module requires a level shifter; after top-down selection,
			/// we can check for the assigned voltages of different modules and thus exclude level shifters whenever the voltages are the same (upper_bound = false)
			inline void updateLevelShifter(std::vector<Net> const& all_nets, bool upper_bound = true);

			/// set global cost, required for top-down selection of modules
			///
			inline void setCost(
					double const& max_power_saving,
					double const& min_power_saving,
					std::vector<double> const& max_power_std_dev,
					int const& max_count,
					unsigned const& max_corners,
					unsigned const& max_level_shifter,
					unsigned const& min_level_shifter,
					MultipleVoltages::Parameters const& parameters
				);

		// public functions
		public:
			/// helper to update power reduction etc.
			inline void updatePower(int const& layers, Block const* block_to_consider = nullptr);

			/// helper to retrieve avg gain in power reduction
			inline double power_saving_avg(bool subtract_wasted_saving = true) const {

				if (subtract_wasted_saving) {
					return (this->power_saving_avg_ - this->power_saving_wasted_avg_);
				}
				else {
					return this->power_saving_avg_;
				}
			};
			/// helper to retrieve absolute gain in power reduction
			inline double power_saving() const {

				return this->blocks.size() * this->power_saving_avg_;
			};

			/// helper to obtain overall (over all dies) max number of corners
			/// in power rings
			inline unsigned corners_powerring_max() const {
				unsigned ret = this->corners_powerring[0];

				for (unsigned i = 1; i < this->corners_powerring.size(); i++) {
					ret = std::max(ret, this->corners_powerring[i]);
				}

				return ret;
			}

			/// helper to retrieve all required level shifters; whenever a nets is connecting from this compound module to another one with a different voltage setting,
			/// we need to account for a level shifter
			inline unsigned level_shifter(bool upper_bound = true) const {

				if (upper_bound) {
					return this->level_shifter_upper_bound;
				}
				else {
					return this->level_shifter_actual;
				}
			};

			/// getter
			inline double power_std_dev_max() const {
				double ret = 0.0;

				for (double const& std_dev : this->power_std_dev_) {
					ret = std::max(ret, std_dev);
				}

				return ret;
			}
	};

	// private data, functions
	private:
		friend class IO;

		/// map of compound modules; keys are a vector<bool>, with each
		/// index representing a numerical block id, and each index' position set
		/// true when the related block is comprised in the module; unordered map
		/// is more efficient in accessing individual elements
		typedef std::unordered_multimap< std::vector<bool>, CompoundModule > modules_type;
		modules_type modules;

		/// vector of selected modules, filled by selectCompoundModules()
		std::vector<CompoundModule*> selected_modules;

	// constructors, destructors, if any non-implicit
	public:

	// public data, functions
	public:
		/// helper to determine all compound modules
		void determineCompoundModules(std::vector<Block> const& blocks, ContiguityAnalysis& contig);
		/// helper to perform top-down selection of compound modules
		std::vector<CompoundModule*> const& selectCompoundModules(std::vector<Net> const& all_nets, bool const& finalize, bool const& merge_selected_modules = false);

	// private helper data, functions
	private:
		/// internal helper to recursively build up compound modules
		void buildCompoundModulesHelper(CompoundModule& module, modules_type::iterator hint, ContiguityAnalysis& cont);
		/// internal helper to manage compound module in data structure
		inline void insertCompoundModuleHelper(
				CompoundModule& module,
				ContiguityAnalysis::ContiguousNeighbour* neighbour,
				bool consider_prev_neighbours,
				std::bitset<MAX_VOLTAGES>& feasible_voltages,
				modules_type::iterator& hint,
				ContiguityAnalysis& cont
			);
};

#endif
