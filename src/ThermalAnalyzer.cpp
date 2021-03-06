/*
 * =====================================================================================
 *
 *    Description:  Corblivar thermal analyzer, based on power blurring
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
#include "ThermalAnalyzer.hpp"
// required Corblivar headers
#include "Rect.hpp"
#include "Net.hpp"
#include "Block.hpp"
#include "Math.hpp"
#include "CorblivarAlignmentReq.hpp"

/// memory allocation
constexpr unsigned ThermalAnalyzer::POWER_MAPS_DIM;

void ThermalAnalyzer::initThermalMap(Point const& die_outline) {
	unsigned x, y;

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "-> ThermalAnalyzer::initThermalMap()" << std::endl;
	}

	// scale of thermal map dimensions
	this->thermal_map_dim_x = die_outline.x / ThermalAnalyzer::THERMAL_MAP_DIM;
	this->thermal_map_dim_y = die_outline.y / ThermalAnalyzer::THERMAL_MAP_DIM;

	// init map data structure
	for (x = 0; x < ThermalAnalyzer::THERMAL_MAP_DIM; x++) {
		for (y = 0; y < ThermalAnalyzer::THERMAL_MAP_DIM; y++) {

			this->thermal_map[x][y] = {
					// init w/ zero temp value
					0.0,
					// grid-map coordinates
					x,
					y,
					// dummy bb
					Rect(),
					// hotspot/blob region id; initialize as undefined
					ThermalAnalyzer::HOTSPOT_UNDEFINED,
					// allocation for neighbor's list; to be
					// initialized during clustering
					std::list<ThermalMapBin*>()
			};

			// determine the bin's bb
			this->thermal_map[x][y].bb.ll.x = x * this->thermal_map_dim_x;
			this->thermal_map[x][y].bb.ll.y = y * this->thermal_map_dim_y;
			this->thermal_map[x][y].bb.ur.x = (x + 1) * this->thermal_map_dim_x;
			this->thermal_map[x][y].bb.ur.y = (y + 1) * this->thermal_map_dim_y;
			this->thermal_map[x][y].bb.w = this->thermal_map_dim_x;
			this->thermal_map[x][y].bb.h = this->thermal_map_dim_y;
			this->thermal_map[x][y].bb.area = this->thermal_map_dim_x * this->thermal_map_dim_y;
		}
	}

	// build-up neighbor relations for thermal-map grid;
	// inner core
	for (x = 1; x < ThermalAnalyzer::THERMAL_MAP_DIM - 1; x++) {
		for (y = 1; y < ThermalAnalyzer::THERMAL_MAP_DIM - 1; y++) {

			this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x-1][y-1]);
			this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x-1][y]);
			this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x-1][y+1]);
			this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x][y+1]);
			this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x+1][y+1]);
			this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x+1][y]);
			this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x+1][y-1]);
			this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x][y-1]);
      	}
      }

	// build-up neighbor relations for thermal-map grid;
	// outer rows and columns 
	x = 0;
	for (y = 1; y < ThermalAnalyzer::THERMAL_MAP_DIM - 1; y++) {

		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x][y+1]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x+1][y+1]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x+1][y]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x+1][y-1]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x][y-1]);
	}

	x = ThermalAnalyzer::THERMAL_MAP_DIM - 1;
	for (y = 1; y < ThermalAnalyzer::THERMAL_MAP_DIM - 1; y++) {

		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x-1][y-1]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x-1][y]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x-1][y+1]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x][y+1]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x][y-1]);
	}

	y = 0;
	for (x = 1; x < ThermalAnalyzer::THERMAL_MAP_DIM - 1; x++) {

		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x-1][y]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x-1][y+1]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x][y+1]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x+1][y+1]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x+1][y]);
	}

	y = ThermalAnalyzer::THERMAL_MAP_DIM - 1;
	for (x = 1; x < ThermalAnalyzer::THERMAL_MAP_DIM - 1; x++) {

		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x-1][y-1]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x-1][y]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x+1][y]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x+1][y-1]);
		this->thermal_map[x][y].neighbors.push_back(&this->thermal_map[x][y-1]);
	}

	// build-up neighbor relations for thermal-map grid;
	// corner points
	this->thermal_map[0][0].neighbors.push_back(&this->thermal_map[0][1]);
	this->thermal_map[0][0].neighbors.push_back(&this->thermal_map[1][1]);
	this->thermal_map[0][0].neighbors.push_back(&this->thermal_map[1][0]);
	this->thermal_map[0][ThermalAnalyzer::THERMAL_MAP_DIM - 1].neighbors.push_back(&this->thermal_map[0][ThermalAnalyzer::THERMAL_MAP_DIM - 2]);
	this->thermal_map[0][ThermalAnalyzer::THERMAL_MAP_DIM - 1].neighbors.push_back(&this->thermal_map[1][ThermalAnalyzer::THERMAL_MAP_DIM - 2]);
	this->thermal_map[0][ThermalAnalyzer::THERMAL_MAP_DIM - 1].neighbors.push_back(&this->thermal_map[1][ThermalAnalyzer::THERMAL_MAP_DIM - 1]);
	this->thermal_map[ThermalAnalyzer::THERMAL_MAP_DIM - 1][ThermalAnalyzer::THERMAL_MAP_DIM - 1].neighbors.push_back(
			&this->thermal_map[ThermalAnalyzer::THERMAL_MAP_DIM - 2][ThermalAnalyzer::THERMAL_MAP_DIM - 1]);
	this->thermal_map[ThermalAnalyzer::THERMAL_MAP_DIM - 1][ThermalAnalyzer::THERMAL_MAP_DIM - 1].neighbors.push_back(
			&this->thermal_map[ThermalAnalyzer::THERMAL_MAP_DIM - 2][ThermalAnalyzer::THERMAL_MAP_DIM - 2]);
	this->thermal_map[ThermalAnalyzer::THERMAL_MAP_DIM - 1][ThermalAnalyzer::THERMAL_MAP_DIM - 1].neighbors.push_back(
			&this->thermal_map[ThermalAnalyzer::THERMAL_MAP_DIM - 1][ThermalAnalyzer::THERMAL_MAP_DIM - 2]);
	this->thermal_map[ThermalAnalyzer::THERMAL_MAP_DIM - 1][0].neighbors.push_back(&this->thermal_map[ThermalAnalyzer::THERMAL_MAP_DIM - 2][0]);
	this->thermal_map[ThermalAnalyzer::THERMAL_MAP_DIM - 1][0].neighbors.push_back(&this->thermal_map[ThermalAnalyzer::THERMAL_MAP_DIM - 2][1]);
	this->thermal_map[ThermalAnalyzer::THERMAL_MAP_DIM - 1][0].neighbors.push_back(&this->thermal_map[ThermalAnalyzer::THERMAL_MAP_DIM - 1][1]);

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "<- ThermalAnalyzer::initThermalMap" << std::endl;
	}
}

void ThermalAnalyzer::initPowerMaps(int const& layers, Point const& die_outline) {
	unsigned b;
	int i;
	ThermalAnalyzer::PowerMapBin init_bin;

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "-> ThermalAnalyzer::initPowerMaps(" << layers << ", " << die_outline.x << ", " << die_outline.y << ")" << std::endl;
	}

	this->power_maps.clear();
	this->power_maps_orig.clear();

	// allocate power-maps arrays
	for (i = 0; i < layers; i++) {
		this->power_maps.emplace_back(
			std::array<std::array<ThermalAnalyzer::PowerMapBin, ThermalAnalyzer::POWER_MAPS_DIM>, ThermalAnalyzer::POWER_MAPS_DIM>()
		);
		this->power_maps_orig.emplace_back(
			std::array<std::array<ThermalAnalyzer::PowerMapBin, ThermalAnalyzer::THERMAL_MAP_DIM>, ThermalAnalyzer::THERMAL_MAP_DIM>()
		);
	}

	// init the maps w/ zero values
	init_bin.power_density = init_bin.TSV_density = 0.0;
	for (i = 0; i < layers; i++) {
		for (auto& partial_map : this->power_maps[i]) {
			partial_map.fill(init_bin);
		}
		for (auto& partial_map : this->power_maps_orig[i]) {
			partial_map.fill(init_bin);
		}
	}

	// scale power map dimensions to outline of thermal map; this way the padding of
	// power maps doesn't distort the block outlines in the thermal map
	this->power_maps_dim_x = die_outline.x / ThermalAnalyzer::THERMAL_MAP_DIM;
	this->power_maps_dim_y = die_outline.y / ThermalAnalyzer::THERMAL_MAP_DIM;

	// determine offset for blocks, related to padding of power maps
	this->blocks_offset_x = this->power_maps_dim_x * ThermalAnalyzer::POWER_MAPS_PADDED_BINS;
	this->blocks_offset_y = this->power_maps_dim_y * ThermalAnalyzer::POWER_MAPS_PADDED_BINS;

	// determine max distance for blocks' upper/right boundaries to upper/right die
	// outline to be padded
	this->padding_right_boundary_blocks_distance = ThermalAnalyzer::PADDING_ZONE_BLOCKS_DISTANCE_LIMIT * die_outline.x;
	this->padding_upper_boundary_blocks_distance = ThermalAnalyzer::PADDING_ZONE_BLOCKS_DISTANCE_LIMIT * die_outline.y;

	// predetermine map bins' area and lower-left corner coordinates; note that the
	// last bin represents the upper-right coordinates for the penultimate bin
	this->power_maps_bin_area = this->power_maps_dim_x * this->power_maps_dim_y;
	for (b = 0; b <= ThermalAnalyzer::POWER_MAPS_DIM; b++) {
		this->power_maps_bins_ll_x[b] = b * this->power_maps_dim_x;
	}
	for (b = 0; b <= ThermalAnalyzer::POWER_MAPS_DIM; b++) {
		this->power_maps_bins_ll_y[b] = b * this->power_maps_dim_y;
	}

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "<- ThermalAnalyzer::initPowerMaps" << std::endl;
	}
}

/// Determine masks for lowest layer, i.e., hottest layer.
/// Based on a gaussian-like thermal impulse response fuction.
/// Note that masks are centered, i.e., the value f(x=0) resides in the middle of the
/// (uneven) array.
/// Note that masks are 1D, sufficient for the separated convolution in
/// performPowerBlurring()
void ThermalAnalyzer::initThermalMasks(int const& layers, bool const& log, MaskParameters const& parameters) {
	int i, ii;
	double scale;
	double layer_impulse_factor;
	int x_y;

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "-> ThermalAnalyzer::initThermalMasks(" << layers << ", " << log << ")" << std::endl;
	}

	if (log) {
		std::cout << "ThermalAnalyzer> ";
		std::cout << "Initializing thermals masks for power blurring ..." << std::endl;
	}

	// reset mask arrays
	this->thermal_masks.clear();

	// allocate mask arrays
	for (i = 0; i < layers; i++) {
		this->thermal_masks.emplace_back(
			std::array<double,ThermalAnalyzer::THERMAL_MASK_DIM>()
		);
	}

	// determine scale factor such that mask_boundary_value is reached at the
	// boundary of the lowermost (2D) mask; based on general 2D gauss equation,
	// determines gauss(x = y) = mask_boundary_value;
	// constant spread (e.g., 1.0) is sufficient since this function fitting
	// only requires two parameters, i.e., varying spread has no impact
	static constexpr double SPREAD = 1.0;
	// scaling is required for function fitting; the maximum of the gauss / exp
	// function is defined by the impulse factor, the minimum by the
	// mask_boundary_value
	scale = std::sqrt(SPREAD * std::log(parameters.impulse_factor / (parameters.mask_boundary_value))) / std::sqrt(2.0);
	// normalize factor according to half of mask dimension; i.e., fit spreading of
	// exp function
	scale /=  ThermalAnalyzer::THERMAL_MASK_CENTER;

	// determine all masks, starting from lowest layer, i.e., hottest layer
	for (i = 1; i <= layers; i++) {

		// impulse factor is to be reduced notably for increasing layer count
		layer_impulse_factor = parameters.impulse_factor / std::pow(i, parameters.impulse_factor_scaling_exponent);

		ii = 0;
		for (x_y = -ThermalAnalyzer::THERMAL_MASK_CENTER; x_y <= static_cast<int>(ThermalAnalyzer::THERMAL_MASK_CENTER); x_y++) {
			// sqrt for impulse factor is mandatory since the mask is
			// used for separated convolution (i.e., factor will be
			// squared in final convolution result)
			this->thermal_masks[i - 1][ii] = Math::gauss1D(x_y * scale, std::sqrt(layer_impulse_factor), SPREAD);

			ii++;
		}
	}

	if (ThermalAnalyzer::DBG) {
		// enforce fixed digit count for printing mask
		std::cout << std::fixed;
		// dump mask
		for (i = 0; i < layers; i++) {
			std::cout << "DBG> Thermal 1D mask for point source on layer " << i << ":" << std::endl;
			for (x_y = 0; x_y < ThermalAnalyzer::THERMAL_MASK_DIM; x_y++) {
				std::cout << this->thermal_masks[i][x_y] << ", ";
			}
			std::cout << std::endl;
		}
		// reset to default floating output
		std::cout.unsetf(std::ios_base::floatfield);

		std::cout << std::endl;
		std::cout << "DBG> Note that these values will be multiplied w/ each other in the final 2D mask" << std::endl;
	}

	if (log) {
		std::cout << "ThermalAnalyzer> ";
		std::cout << "Done" << std::endl << std::endl;
	}

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "<- ThermalAnalyzer::initThermalMasks" << std::endl;
	}
}

void ThermalAnalyzer::generatePowerMaps(int const& layers, std::vector<Block> const& blocks, Point const& die_outline, MaskParameters const& parameters, bool const& extend_boundary_blocks_into_padding_zone) {
	int i;
	unsigned x, y;
	unsigned x_lower, x_upper, y_lower, y_upper;
	Rect bin, intersect, block_offset;
	bool padding_zone;
	ThermalAnalyzer::PowerMapBin init_bin;

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "-> ThermalAnalyzer::generatePowerMaps(" << layers << ", " << &blocks << ", (" << die_outline.x << ", " << die_outline.y << "), " << &parameters << ", " << extend_boundary_blocks_into_padding_zone << ")" << std::endl;
	}

	init_bin.power_density = 0.0;
	init_bin.TSV_density = 0.0;

	// determine maps for each layer
	for (i = 0; i < layers; i++) {

		// reset map to zero
		// note: this also implicitly pads the map w/ zero power density
		for (auto& m : this->power_maps[i]) {
			m.fill(init_bin);
		}
		for (auto& m : this->power_maps_orig[i]) {
			m.fill(init_bin);
		}

		// consider each block on the related layer
		for (Block const& block : blocks) {

			// drop blocks assigned to other layers
			if (block.layer != i) {
				continue;
			}

			// determine offset, i.e., shifted, block bb; relates to block's
			// bb in padded power map
			block_offset = block.bb;

			// don't offset blocks at the left/lower chip boundaries,
			// implicitly extend them into power-map padding zone; this way,
			// during convolution, the thermal estimate increases for these
			// blocks; blocks not at the boundaries are shifted
			if (extend_boundary_blocks_into_padding_zone && block.bb.ll.x == 0.0) {
			}
			else {
				block_offset.ll.x += this->blocks_offset_x;
			}
			if (extend_boundary_blocks_into_padding_zone && block.bb.ll.y == 0.0) {
			}
			else {
				block_offset.ll.y += this->blocks_offset_y;
			}

			// also consider extending blocks into right/upper padding zone if
			// they are close to the related chip boundaries
			if (
					extend_boundary_blocks_into_padding_zone &&
					std::abs(die_outline.x - block.bb.ur.x) < this->padding_right_boundary_blocks_distance
			   ) {
				// consider offset twice in order to reach right/uppper
				// boundary related to layout described by padded power map
				block_offset.ur.x = die_outline.x + 2.0 * this->blocks_offset_x;
			}
			// simple shift otherwise; compensate for padding of left/bottom
			// boundaries
			else {
				block_offset.ur.x += this->blocks_offset_x;
			}

			if (
					extend_boundary_blocks_into_padding_zone
					&& std::abs(die_outline.y - block.bb.ur.y) < this->padding_upper_boundary_blocks_distance
			   ) {
				block_offset.ur.y = die_outline.y + 2.0 * this->blocks_offset_y;
			}
			else {
				block_offset.ur.y += this->blocks_offset_y;
			}

			// determine index boundaries for offset block; based on boundary
			// of blocks and the covered bins; note that casting truncates
			// toward zero, i.e., performs like floor for positive numbers
			x_lower = static_cast<unsigned>(block_offset.ll.x / this->power_maps_dim_x);
			y_lower = static_cast<unsigned>(block_offset.ll.y / this->power_maps_dim_y);
			// +1 in order to efficiently emulate the result of ceil(); limit
			// upper bound to power-maps dimensions
			x_upper = std::min(static_cast<unsigned>(block_offset.ur.x / this->power_maps_dim_x) + 1, ThermalAnalyzer::POWER_MAPS_DIM);
			y_upper = std::min(static_cast<unsigned>(block_offset.ur.y / this->power_maps_dim_y) + 1, ThermalAnalyzer::POWER_MAPS_DIM);

			// walk power-map bins covering block outline
			for (x = x_lower; x < x_upper; x++) {
				for (y = y_lower; y < y_upper; y++) {

					// determine if bin w/in padding zone
					if (
							x < ThermalAnalyzer::POWER_MAPS_PADDED_BINS
							|| x >= (ThermalAnalyzer::POWER_MAPS_DIM - ThermalAnalyzer::POWER_MAPS_PADDED_BINS)
							|| y < ThermalAnalyzer::POWER_MAPS_PADDED_BINS
							|| y >= (ThermalAnalyzer::POWER_MAPS_DIM - ThermalAnalyzer::POWER_MAPS_PADDED_BINS)
					   ) {
						padding_zone = true;
					}
					else {
						padding_zone = false;
					}

					// consider full block power density for fully covered bins
					if (x_lower < x && x < (x_upper - 1) && y_lower < y && y < (y_upper - 1)) {
						if (padding_zone) {
							this->power_maps[i][x][y].power_density += block.power_density() * parameters.power_density_scaling_padding_zone;
						}
						else {
							this->power_maps[i][x][y].power_density += block.power_density();
						}
					}
					// else consider block power according to
					// intersection of current bin and block
					else {
						// determine real coords of map bin
						bin.ll.x = this->power_maps_bins_ll_x[x];
						bin.ll.y = this->power_maps_bins_ll_y[y];
						// note that +1 is guaranteed to be within bounds
						// of power_maps_bins_ll_x/y (size =
						// ThermalAnalyzer::POWER_MAPS_DIM + 1); the
						// related last tuple describes the upper-right
						// corner coordinates of the right/top boundary
						bin.ur.x = this->power_maps_bins_ll_x[x + 1];
						bin.ur.y = this->power_maps_bins_ll_y[y + 1];

						// determine intersection
						intersect = Rect::determineIntersection(bin, block_offset);
						// normalize to full bin area
						intersect.area /= this->power_maps_bin_area;

						if (padding_zone) {
							this->power_maps[i][x][y].power_density += block.power_density() * intersect.area * parameters.power_density_scaling_padding_zone;
						}
						else {
							this->power_maps[i][x][y].power_density += block.power_density() * intersect.area;
						}
					}
				}
			}
		}
	}

	// copy inner, unpadded frame of just generated basic power map to power_maps_orig
	//
	for (i = 0; i < layers; i++) {
		for (x = ThermalAnalyzer::POWER_MAPS_PADDED_BINS; x < ThermalAnalyzer::THERMAL_MAP_DIM + ThermalAnalyzer::POWER_MAPS_PADDED_BINS; x++) {
			for (y = ThermalAnalyzer::POWER_MAPS_PADDED_BINS; y < ThermalAnalyzer::THERMAL_MAP_DIM + ThermalAnalyzer::POWER_MAPS_PADDED_BINS; y++) {
				this->power_maps_orig[i][x - ThermalAnalyzer::POWER_MAPS_PADDED_BINS][y - ThermalAnalyzer::POWER_MAPS_PADDED_BINS].power_density +=
					this->power_maps[i][x][y].power_density;
			}
		}
	}
	
	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "<- ThermalAnalyzer::generatePowerMaps" << std::endl;
	}
}

/// note that this function only accounts for (via TSVs improved heat conduction) lower
/// local power consumption, not the (much smaller) increase of power consumption due to
/// resistivity of TSVs; TSVs densities, required for HotSpot calculation, are also
/// adapted here
void ThermalAnalyzer::adaptPowerMapsTSVs(int const& layers, std::vector<TSV_Island> TSVs, std::vector<TSV_Island> dummy_TSVs, MaskParameters const& parameters) {
	unsigned x, y;
	int i;

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "-> ThermalAnalyzer::adaptPowerMapsTSVs(" << layers << ", " << &TSVs << ", " << &parameters << ")" << std::endl;
	}

	// consider impact of all TSVs, real and dummy ones; map TSV densities to power
	// maps; also required for HotSpot calculation, to model different material
	// properties in regions with and without TSVs
	//
	for (TSV_Island const& TSVi : TSVs) {
		this->adaptPowerMapsTSVsHelper(TSVi);
	}
	for (TSV_Island const& TSVi : dummy_TSVs) {
		this->adaptPowerMapsTSVsHelper(TSVi);
	}

	// walk power-map bins; adapt power according to TSV densities
	for (x = ThermalAnalyzer::POWER_MAPS_PADDED_BINS; x < ThermalAnalyzer::THERMAL_MAP_DIM + ThermalAnalyzer::POWER_MAPS_PADDED_BINS; x++) {
		for (y = ThermalAnalyzer::POWER_MAPS_PADDED_BINS; y < ThermalAnalyzer::THERMAL_MAP_DIM + ThermalAnalyzer::POWER_MAPS_PADDED_BINS; y++) {

			// sanity check; TSV density should be <= 100%; might be larger
			// due to superposition in calculations above
			for (i = 0; i < layers; i++) {
				this->power_maps[i][x][y].TSV_density = std::min(100.0, this->power_maps[i][x][y].TSV_density);
			}

			// adapt maps for all layers; the uppermost layer next the
			// heatsink may also contain (dummy) thermal TSVs
			for (i = 0; i < layers; i++) {

				// ignore cases w/o TSVs
				if (this->power_maps[i][x][y].TSV_density == 0.0)
					continue;

				// scaling depends on TSV density; the larger the TSV
				// density, the larger the power down-scaling; note that
				// the factor power_density_scaling_TSV_region ranges b/w
				// 0.0 and 1.0, whereas TSV_density ranges b/w 0.0 and
				// 100.0
				this->power_maps[i][x][y].power_density *=
					1.0 +
					((parameters.power_density_scaling_TSV_region - 1.0) / 100.0) * this->power_maps[i][x][y].TSV_density;
			}
		}
	}

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "<- ThermalAnalyzer::adaptPowerMapsTSVs" << std::endl;
	}
}

/// note that local copies of TSVs islands are used in order to not mess with the actual
/// coordinates of the islands
void ThermalAnalyzer::adaptPowerMapsTSVsHelper(TSV_Island TSVi) {
	unsigned x, y;
	unsigned x_lower, x_upper, y_lower, y_upper;
	Rect bin, bin_intersect;

	// offset intersection, i.e., account for padded power maps and related
	// offset in coordinates
	TSVi.bb.ll.x += this->blocks_offset_x;
	TSVi.bb.ll.y += this->blocks_offset_y;
	TSVi.bb.ur.x += this->blocks_offset_x;
	TSVi.bb.ur.y += this->blocks_offset_y;

	// determine index boundaries for offset intersection; based on boundary
	// of intersection and the covered bins; note that casting truncates
	// toward zero, i.e., performs like floor for positive numbers
	x_lower = static_cast<unsigned>(TSVi.bb.ll.x / this->power_maps_dim_x);
	y_lower = static_cast<unsigned>(TSVi.bb.ll.y / this->power_maps_dim_y);
	// +1 in order to efficiently emulate the result of ceil(); limit upper
	// bound to power-maps dimensions
	x_upper = std::min(static_cast<unsigned>(TSVi.bb.ur.x / this->power_maps_dim_x) + 1, ThermalAnalyzer::POWER_MAPS_DIM);
	y_upper = std::min(static_cast<unsigned>(TSVi.bb.ur.y / this->power_maps_dim_y) + 1, ThermalAnalyzer::POWER_MAPS_DIM);

	if (ThermalAnalyzer::DBG) {
		std::cout << "DBG> TSV group " << TSVi.id << std::endl;
		std::cout << "DBG>  Affected power-map bins: " << x_lower << "," << y_lower
			<< " to " <<
			x_upper << "," << y_upper << std::endl;
	}

	// walk power-map bins covering intersection outline; adapt TSV densities
	for (x = x_lower; x < x_upper; x++) {
		for (y = y_lower; y < y_upper; y++) {

			// consider full TSV density for fully covered bins
			if (x_lower < x && x < (x_upper - 1) && y_lower < y && y < (y_upper - 1)) {

				// adapt map on affected layer
				this->power_maps[TSVi.layer][x][y].TSV_density += 100.0;
			}
			// else consider TSV density according to partial
			// intersection with current bin
			else {
				// determine real coords of map bin
				bin.ll.x = this->power_maps_bins_ll_x[x];
				bin.ll.y = this->power_maps_bins_ll_y[y];
				// note that +1 is guaranteed to be within
				// bounds of power_maps_bins_ll_x/y (size
				// = ThermalAnalyzer::POWER_MAPS_DIM + 1);
				// the related last tuple describes the
				// upper-right corner coordinates of the
				// right/top boundary
				bin.ur.x = this->power_maps_bins_ll_x[x + 1];
				bin.ur.y = this->power_maps_bins_ll_y[y + 1];

				// determine intersection
				bin_intersect = Rect::determineIntersection(bin, TSVi.bb);
				// normalize to full bin area
				bin_intersect.area /= this->power_maps_bin_area;

				// adapt map on affected layer
				this->power_maps[TSVi.layer][x][y].TSV_density += 100.0 * bin_intersect.area;
			}
		}
	}
}

void ThermalAnalyzer::adaptPowerMapsWiresHelper(std::vector<Block>& wires, int const& layer, Rect const& net_bb, double const& total_wire_power) {

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "-> ThermalAnalyzer::adaptPowerMapsWiresHelper(" << &wires << ", " << layer << ", " << &net_bb << ", " << total_wire_power << ")" << std::endl;
	}

	// update respective dummy block; these blocks encapsulate all the wires in each layer within the overall bounding box
	//
	// note that this limits the accuracy, but this is acceptable as the power consumed by the wires will typically be much smaller than that consumed by real blocks; further,
	// HotSpot does not cope well with an excessive number of dummy blocks (which would be required if we were to handled each net separately)
	//
	// finally, this also reduces runtime efforts notably, as we walk the power maps only once, after we considered all the wires/nets
	//

	// extend bb such that this net is also covered
	if (wires[layer].bb.area != 0.0) {
		wires[layer].bb = Rect::determBoundingBox(wires[layer].bb, net_bb);
	}
	// init bb using this net in case no bb is stored yet
	else {
		wires[layer].bb = net_bb;
	}
	// sum up power of each net; encoded in power_density_unscaled only for these
	// dummy blocks
	wires[layer].power_density_unscaled += total_wire_power;

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "<- ThermalAnalyzer::adaptPowerMapsWiresHelper" << std::endl;
	}
}

void ThermalAnalyzer::adaptPowerMapsWires(std::vector<Block>& wires) {
	double power_density;
	unsigned x, y;
	unsigned x_lower, x_upper, y_lower, y_upper;
	Rect net_bb;

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "-> ThermalAnalyzer::adaptPowerMapsWires(" << &wires << ")" << std::endl;
	}

	// now, adapt power maps to consider all the wires in each layer separately
	//
	for (unsigned layer = 0; layer < wires.size(); layer++) {

		net_bb = wires[layer].bb;

		// offset bb, i.e., account for padded power maps and related offset in
		// coordinates; note that net_bb is a copy
		net_bb.ll.x += this->blocks_offset_x;
		net_bb.ll.y += this->blocks_offset_y;
		net_bb.ur.x += this->blocks_offset_x;
		net_bb.ur.y += this->blocks_offset_y;

		// determine index boundaries for offset intersection; based on boundary of bb and
		// the covered bins; note that casting truncates toward zero, i.e., performs
		// like floor for positive numbers
		x_lower = static_cast<unsigned>(net_bb.ll.x / this->power_maps_dim_x);
		y_lower = static_cast<unsigned>(net_bb.ll.y / this->power_maps_dim_y);
		// +1 in order to efficiently emulate the result of ceil(); limit upper bound to
		// power-maps dimensions
		x_upper = std::min(static_cast<unsigned>(net_bb.ur.x / this->power_maps_dim_x) + 1, ThermalAnalyzer::POWER_MAPS_DIM);
		y_upper = std::min(static_cast<unsigned>(net_bb.ur.y / this->power_maps_dim_y) + 1, ThermalAnalyzer::POWER_MAPS_DIM);

		// determine power density
		power_density = wires[layer].power_density_unscaled / net_bb.area;

		// walk power-map bins covering bb; adapt power densities
		for (x = x_lower; x < x_upper; x++) {
			for (y = y_lower; y < y_upper; y++) {

				// adapt map on affected layer
				//
				// don't consider partial overlaps, any affected bin is considered
				// as fully affected; the loss in accuracy is expected to be
				// rather low
				this->power_maps[layer][x][y].power_density += power_density;
			}
		}
	}

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "<- ThermalAnalyzer::adaptPowerMapsWires" << std::endl;
	}
}

/// Thermal-analyzer routine based on power blurring,
/// i.e., convolution of thermals masks and power maps into thermal maps.
/// Based on a separated convolution using separated 2D gauss function, i.e., 1D gauss
/// fct., see http://www.songho.ca/dsp/convolution/convolution.html#separable_convolution
/// Returns thermal map of lowest layer, i.e., hottest layer
void ThermalAnalyzer::performPowerBlurring(ThermalAnalysisResult& ret, int const& layers, MaskParameters const& parameters) {
	int layer;
	unsigned x, y, i;
	unsigned map_x, map_y;
	unsigned mask_i;
	double max_temp, avg_temp;
	// required as buffer for separated convolution; note that its dimensions
	// corresponds to a power map, which is required to hold temporary results for 1D
	// convolution of padded power maps
	std::array< std::array<double, ThermalAnalyzer::POWER_MAPS_DIM>, ThermalAnalyzer::POWER_MAPS_DIM> thermal_map_tmp;

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "-> ThermalAnalyzer::performPowerBlurring(" << &ret << ", " << ", " << layers << ", " << &parameters << ")" << std::endl;
	}

	// init temp map w/ zero
	for (auto& m : thermal_map_tmp) {
		m.fill(0.0);
	}

	// init final map w/ temperature offset; offset is a additive factor, and thus not
	// considered during convolution
	for (x = 0; x < ThermalAnalyzer::THERMAL_MAP_DIM; x++) {
		for (y = 0; y < ThermalAnalyzer::THERMAL_MAP_DIM; y++) {
			this->thermal_map[x][y].temp = parameters.temp_offset;
		}
	}

	/// perform 2D convolution by performing two separated 1D convolution iterations;
	/// note that no (kernel) flipping is required since the mask is symmetric
	//
	// start w/ horizontal convolution (with which to start doesn't matter actually)
	for (layer = 0; layer < layers; layer++) {

		// walk power-map grid for horizontal convolution; store into
		// thermal_map_tmp; note that during horizontal convolution we need to
		// walk the full y-dimension related to the padded power map in order to
		// reasonably model the thermal effect in the padding zone during
		// subsequent vertical convolution
		for (y = 0; y < ThermalAnalyzer::POWER_MAPS_DIM; y++) {

			// for the x-dimension during horizontal convolution, we need to
			// restrict the considered range according to the thermal map in
			// order to exploit the padded power map w/o mask boundary checks
			for (x = ThermalAnalyzer::POWER_MAPS_PADDED_BINS; x < ThermalAnalyzer::THERMAL_MAP_DIM + ThermalAnalyzer::POWER_MAPS_PADDED_BINS; x++) {

				// perform horizontal 1D convolution, i.e., multiply
				// input[x] w/ mask
				//
				// e.g., for x = 0, THERMAL_MASK_DIM = 3
				// convol1D(x=0) = input[-1] * mask[0] + input[0] * mask[1] + input[1] * mask[2]
				//
				// can be also illustrated by aligning and multiplying
				// both arrays:
				// input array (power map); unpadded view
				// |x=-1|x=0|x=1|x=2|
				// input array (power map); padded, real view
				// |x=0 |x=1|x=2|x=3|
				// mask:
				// |m=0 |m=1|m=2|
				//
				for (mask_i = 0; mask_i < ThermalAnalyzer::THERMAL_MASK_DIM; mask_i++) {

					// determine power-map index; note that it is not
					// out of range due to the padded power maps
					i = x + (mask_i - ThermalAnalyzer::THERMAL_MASK_CENTER);

					if (ThermalAnalyzer::DBG) {

						// mass of dbg messages; only for insane
						// flag
						if (ThermalAnalyzer::DBG_INSANE) {
							std::cout << "DBG> y=" << y << ", x=" << x << ", mask_i=" << mask_i << ", i=" << i << std::endl;
						}

						if (i >= ThermalAnalyzer::POWER_MAPS_DIM) {
							std::cout << "DBG> Convolution data error; i out of range (should be limited by x)" << std::endl;
						}
					}

					// convolution; multiplication of mask element and
					// power-map bin
					thermal_map_tmp[x][y] +=
						this->power_maps[layer][i][y].power_density *
						this->thermal_masks[layer][mask_i];
				}
			}
		}
	}

	// continue w/ vertical convolution; here we convolute the temp thermal map (sized
	// like the padded power map) w/ the thermal masks in order to obtain the final
	// thermal map (sized like a non-padded power map)
	for (layer = 0; layer < layers; layer++) {

		// walk power-map grid for vertical convolution; convolute mask w/ data
		// obtained by horizontal convolution (thermal_map_tmp)
		for (x = ThermalAnalyzer::POWER_MAPS_PADDED_BINS; x < ThermalAnalyzer::THERMAL_MAP_DIM + ThermalAnalyzer::POWER_MAPS_PADDED_BINS; x++) {

			// index for final thermal map, considers padding offset
			map_x = x - ThermalAnalyzer::POWER_MAPS_PADDED_BINS;

			for (y = ThermalAnalyzer::POWER_MAPS_PADDED_BINS; y < ThermalAnalyzer::THERMAL_MAP_DIM + ThermalAnalyzer::POWER_MAPS_PADDED_BINS; y++) {

				// index for final thermal map, considers padding offset
				map_y = y - ThermalAnalyzer::POWER_MAPS_PADDED_BINS;

				// perform 1D vertical convolution
				for (mask_i = 0; mask_i < ThermalAnalyzer::THERMAL_MASK_DIM; mask_i++) {

					// determine power-map index; note that it is not
					// out of range due to the temp thermal map (sized
					// like the padded power map)
					i = y + (mask_i - ThermalAnalyzer::THERMAL_MASK_CENTER);

					if (ThermalAnalyzer::DBG) {

						// mass of dbg messages; only for insane
						// flag
						if (ThermalAnalyzer::DBG_INSANE) {
							std::cout << "DBG> x=" << x << ", y=" << y << ", map_x=" << map_x << ", map_y=" << map_y;
							std::cout << ", mask_i=" << mask_i << ", i=" << i << std::endl;
						}

						if (i >= ThermalAnalyzer::POWER_MAPS_DIM) {
							std::cout << "DBG> Convolution data error; i out of range (should be limited by y)" << std::endl;
						}
					}

					// convolution; multiplication of mask element and
					// power-map bin
					this->thermal_map[map_x][map_y].temp +=
						thermal_map_tmp[x][i] *
						this->thermal_masks[layer][mask_i];
				}
			}
		}
	}

	// determine max and avg value
	max_temp = avg_temp = 0.0;
	for (x = 0; x < ThermalAnalyzer::THERMAL_MAP_DIM; x++) {
		for (y = 0; y < ThermalAnalyzer::THERMAL_MAP_DIM; y++) {
			max_temp = std::max(max_temp, this->thermal_map[x][y].temp);
			avg_temp += this->thermal_map[x][y].temp;
		}
	}
	avg_temp /= std::pow(ThermalAnalyzer::THERMAL_MAP_DIM, 2);

	// determine cost: max temp estimation, weighted w/ avg temp
	ret.cost_temp = avg_temp * max_temp;
	// store max temp
	ret.max_temp = max_temp;
	// also store temp offset
	ret.temp_offset = parameters.temp_offset;
	// also link whole thermal map to result
	ret.thermal_map = &this->thermal_map;

	if (ThermalAnalyzer::DBG_CALLS) {
		std::cout << "<- ThermalAnalyzer::performPowerBlurring" << std::endl;
	}
}
