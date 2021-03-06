/*
 * =====================================================================================
 *
 *    Description:  Corblivar contiguity analysis, required for multiple-voltages feature
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

// own Corblivar header
#include "ContiguityAnalysis.hpp"
// required Corblivar headers
#include "Block.hpp"
#include "Math.hpp"

/// Extract blocks' boundaries, and order them by coordinates; this will reduce required
/// comparisons between (in principal all pairs of) blocks notably by considering only
/// relevant blocks. For intra-die contiguity, these are abutting boundaries, and for
/// inter-die contiguity, these are boundaries within a block's outline.
///
void ContiguityAnalysis::analyseBlocks(int layers, std::vector<Block> const& blocks) {

	ContiguityAnalysis::Boundary cur_boundary;

	// this data structure is used for inter-die contiguity analysis; note and see
	// below that considering / walking one dimension's boundaries is sufficient to
	// determine intersections in both dimensions / overlaps
	std::vector< std::vector<ContiguityAnalysis::Boundary> > inter_die_boundaries_vert;

	std::vector<ContiguityAnalysis::Boundary>::iterator i1;
	std::vector<ContiguityAnalysis::Boundary>::iterator i2;
	// (TODO) drop; not required as of now
//	double common_boundary_hor, common_boundary_vert;

	// init die-wise lists of boundaries
	this->boundaries_hor.clear();
	this->boundaries_hor.reserve(layers);
	this->boundaries_vert.clear();
	this->boundaries_vert.reserve(layers);
	inter_die_boundaries_vert.reserve(layers - 1);
	for (int l = 0; l < layers; l++) {
		this->boundaries_hor.push_back(std::vector<ContiguityAnalysis::Boundary>());
		this->boundaries_vert.push_back(std::vector<ContiguityAnalysis::Boundary>());
	}
	for (int l = 0; l < layers - 1; l++) {
		inter_die_boundaries_vert.push_back(std::vector<ContiguityAnalysis::Boundary>());
	}

	// add blocks' boundaries into corresponding list
	for (Block const& block : blocks) {

		// reset previous contiguous neighbours
		block.contiguous_neighbours.clear();

		cur_boundary.block = &block;

		// left boundary
		cur_boundary.low.x = block.bb.ll.x;
		cur_boundary.low.y = block.bb.ll.y;
		cur_boundary.high.x = block.bb.ll.x;
		cur_boundary.high.y = block.bb.ur.y;

		// intra-die contiguity
		this->boundaries_vert[block.layer].push_back(cur_boundary);

		// inter-die contiguity; merge left boundaries for adjacent dies' blocks
		// into one layer of inter_die_boundaries, such that determination of
		// inter-die contiguity will be simplified
		if (block.layer == 0) {
			inter_die_boundaries_vert[0].push_back(cur_boundary);
		}
		else if (block.layer == layers - 1) {
			inter_die_boundaries_vert[block.layer - 1].push_back(cur_boundary);
		}
		// layer > 0; block has to be considered for both this and the layer below
		// in the dedicated data structure
		else {
			inter_die_boundaries_vert[block.layer].push_back(cur_boundary);
			inter_die_boundaries_vert[block.layer - 1].push_back(cur_boundary);
		}

		// right boundary
		cur_boundary.low.x = block.bb.ur.x;
		cur_boundary.low.y = block.bb.ll.y;
		cur_boundary.high.x = block.bb.ur.x;
		cur_boundary.high.y = block.bb.ur.y;

		// intra-die contiguity
		this->boundaries_vert[block.layer].push_back(cur_boundary);

		// bottom boundary
		cur_boundary.low.x = block.bb.ll.x;
		cur_boundary.low.y = block.bb.ll.y;
		cur_boundary.high.x = block.bb.ur.x;
		cur_boundary.high.y = block.bb.ll.y;

		// intra-die contiguity
		this->boundaries_hor[block.layer].push_back(cur_boundary);

		// top boundary
		cur_boundary.low.x = block.bb.ll.x;
		cur_boundary.low.y = block.bb.ur.y;
		cur_boundary.high.x = block.bb.ur.x;
		cur_boundary.high.y = block.bb.ur.y;

		// intra-die contiguity
		this->boundaries_hor[block.layer].push_back(cur_boundary);
	}

	// determine horizontal and vertical contiguous neighbours die-wise; intra-die
	// contiguity
	//
	for (int l = 0; l < layers; l++) {

		// first, sort boundaries such that they are ordered by their orthogonal
		// dimension first (i.e., y for horizontal, and x for vertical boundaries)
		// and also by their extension dimension; this way, boundaries can next be
		// easily compared with each other
		std::sort(this->boundaries_hor[l].begin(), this->boundaries_hor[l].end(), ContiguityAnalysis::boundaries_hor_comp);
		std::sort(this->boundaries_vert[l].begin(), this->boundaries_vert[l].end(), ContiguityAnalysis::boundaries_vert_comp);

		if (ContiguityAnalysis::DBG) {

			std::cout << "DBG_CONTIGUITY> Sorted boundaries; die " << l << "; horizontal boundaries:" << std::endl;
			for (auto const& boundary : this->boundaries_hor[l]) {

				std::cout << "DBG_CONTIGUITY>  Boundary: ";
				std::cout << "(" << boundary.low.x << "," << boundary.low.y << ")";
				std::cout << "(" << boundary.high.x << "," << boundary.high.y << "); block " << boundary.block->id << std::endl;
			}

			std::cout << "DBG_CONTIGUITY> Sorted boundaries; die " << l << "; vertical boundaries:" << std::endl;
			for (auto const& boundary : this->boundaries_vert[l]) {

				std::cout << "DBG_CONTIGUITY>  Boundary: ";
				std::cout << "(" << boundary.low.x << "," << boundary.low.y << ")";
				std::cout << "(" << boundary.high.x << "," << boundary.high.y << "); block " << boundary.block->id << std::endl;
			}

			std::cout << "DBG_CONTIGUITY>" << std::endl;
		}

		// then, walk boundaries and whenever two boundaries are intersecting on
		// the same x- and y-coordinates, consider their related blocks as
		// contiguous neighbours
		//
		if (ContiguityAnalysis::DBG) {

			std::cout << "DBG_CONTIGUITY> Determine intersecting boundaries on die " << l << "; derive intra-die contiguity" << std::endl;
		}

		// vertical boundaries
		for (i1 = this->boundaries_vert[l].begin(); i1 != this->boundaries_vert[l].end(); ++i1) {

			ContiguityAnalysis::Boundary& b1 = (*i1);

			if (ContiguityAnalysis::DBG) {
				std::cout << "DBG_CONTIGUITY>  Currently considered vertical segment ";
				std::cout << "(" << b1.low.x << "," << b1.low.y << ")";
				std::cout << "(" << b1.high.x << "," << b1.high.y << "); block " << b1.block->id << std::endl;
			}

			// the boundary b2, to be compared to b1, should have the same
			// x-coordinate; thus, we start from the next element (not the
			// same, in order to avoid comparison with itself) in the set of
			// sorted boundaries
			i2 = i1 + 1;
			for (; i2 != this->boundaries_vert[l].end(); ++i2) {

				ContiguityAnalysis::Boundary& b2 = (*i2);

				// break loop conditions;
				if (
					// if the lower point of b2 is just matching or
					// already above the upper point of b1, i.e., when
					// no intersection is feasible anymore
					(b2.low.y >= b1.high.y)
					// or if we reached the next x-coordinate already,
					// i.e., again no intersection is feasible anymore
					|| (b2.low.x > b1.low.x)
				   ) {
					break;
				}

				// otherwise, some intersection exist;
				//
				// (TODO) drop; not required as of now
//				// determine amount of intersection / common boundary and
//				// memorize within both blocks
//				common_boundary_vert = ContiguityAnalysis::common_boundary_vert(b1, b2);

				// init neighbourship storage
				ContiguityAnalysis::ContiguousNeighbour neighbour_for_b1;
				ContiguityAnalysis::ContiguousNeighbour neighbour_for_b2;
				neighbour_for_b1.block = b2.block;
				neighbour_for_b2.block = b1.block;

				// (TODO) drop; not required as of now
//				// for b2 being right of b1, the common boundary to be
//				// stored in b1 is positive and the one for b2 is
//				// negative
//				if (b2.block->bb.ll.x > b1.block->bb.ll.x) {
//					neighbour_for_b1.common_boundary_vert = common_boundary_vert;
//					neighbour_for_b2.common_boundary_vert = -common_boundary_vert;
//				}
//				// b2 is left of b1, store negative value in b1 and
//				// positive in b2
//				else {
//					neighbour_for_b1.common_boundary_vert = -common_boundary_vert;
//					neighbour_for_b2.common_boundary_vert = common_boundary_vert;
//				}

				// memorize within each block
				b1.block->contiguous_neighbours.push_back(neighbour_for_b1);
				b2.block->contiguous_neighbours.push_back(neighbour_for_b2);

				if (ContiguityAnalysis::DBG) {
					std::cout << "DBG_CONTIGUITY>   Common boundary with block " << b2.block->id;
					std::cout << "; " << b2.block->id << "'s related segment ";
					std::cout << "(" << b2.low.x << "," << b2.low.y << ")";
					std::cout << "(" << b2.high.x << "," << b2.high.y << ");";
					// (TODO) drop; not required as of now
//					std::cout << " length of boundary: " << common_boundary_vert;
					std::cout << std::endl;
				}
			}
		}

		// horizontal boundaries
		for (i1 = this->boundaries_hor[l].begin(); i1 != this->boundaries_hor[l].end(); ++i1) {

			ContiguityAnalysis::Boundary& b1 = (*i1);

			if (ContiguityAnalysis::DBG) {
				std::cout << "DBG_CONTIGUITY>  Currently considered horizontal segment ";
				std::cout << "(" << b1.low.x << "," << b1.low.y << ")";
				std::cout << "(" << b1.high.x << "," << b1.high.y << "); block " << b1.block->id << std::endl;
			}

			// the boundary b2, to be compared to b1, should have the same
			// y-coordinate; thus, we start from the next element (not the
			// same, in order to avoid comparison with itself) in the set of
			// sorted boundaries
			i2 = i1 + 1;
			for (; i2 != this->boundaries_hor[l].end(); ++i2) {

				ContiguityAnalysis::Boundary& b2 = (*i2);

				// break loop conditions;
				if (
					// if the left point of b2 is just matching or
					// already to the right of the right point of b1,
					// i.e., when no intersection is feasible anymore
					(b2.low.x >= b1.high.x)
					// or if we reached the next y-coordinate already,
					// i.e., again no intersection is feasible anymore
					|| (b2.low.y > b1.low.y)
				   ) {
					break;
				}

				// otherwise, some intersection exist;
				//
				// (TODO) drop; not required as of now
//				// determine amount of intersection / common boundary and
//				// memorize within both blocks
//				common_boundary_hor = ContiguityAnalysis::common_boundary_hor(b1, b2);

				// init neighbourship storage
				ContiguityAnalysis::ContiguousNeighbour neighbour_for_b1;
				ContiguityAnalysis::ContiguousNeighbour neighbour_for_b2;
				neighbour_for_b1.block = b2.block;
				neighbour_for_b2.block = b1.block;

				// (TODO) drop; not required as of now
//				// for b2 being atop of b1, the common boundary to be
//				// stored in b1 is positive and the one for b2 is
//				// negative
//				if (b2.block->bb.ll.y > b1.block->bb.ll.y) {
//					neighbour_for_b1.common_boundary_hor = common_boundary_hor;
//					neighbour_for_b2.common_boundary_hor = -common_boundary_hor;
//				}
//				// b2 is below of b1, store negative value in b1 and
//				// positive in b2
//				else {
//					neighbour_for_b1.common_boundary_hor = -common_boundary_hor;
//					neighbour_for_b2.common_boundary_hor = common_boundary_hor;
//				}

				// memorize within each block
				b1.block->contiguous_neighbours.push_back(neighbour_for_b1);
				b2.block->contiguous_neighbours.push_back(neighbour_for_b2);

				if (ContiguityAnalysis::DBG) {
					std::cout << "DBG_CONTIGUITY>   Common boundary with block " << b2.block->id;
					std::cout << "; " << b2.block->id << "'s related segment ";
					std::cout << "(" << b2.low.x << "," << b2.low.y << ")";
					std::cout << "(" << b2.high.x << "," << b2.high.y << ");";
					// (TODO) drop; not required as of now
//					std::cout << " length of boundary: " << common_boundary_hor;
					std::cout << std::endl;
				}
			}
		}

		if (ContiguityAnalysis::DBG) {
			std::cout << "DBG_CONTIGUITY>" << std::endl;
		}
	}

	// determine contiguous neighbours across adjacent dies; inter-die contiguity
	//
	for (int l = 0; l < layers - 1; l++) {

		// first, sort boundaries such that they are ordered by their orthogonal
		// dimension first (i.e., y for horizontal, and x for vertical boundaries)
		// and also by their extension dimension; this way, boundaries can next be
		// easily compared with each other
		std::sort(inter_die_boundaries_vert[l].begin(), inter_die_boundaries_vert[l].end(), ContiguityAnalysis::boundaries_vert_comp);

		if (ContiguityAnalysis::DBG) {

			std::cout << "DBG_CONTIGUITY> Sorted and merged boundaries; dies " << l << " and " << l + 1 << "; left (vertical) boundaries:" << std::endl;
			for (auto const& boundary : inter_die_boundaries_vert[l]) {

				std::cout << "DBG_CONTIGUITY>  Boundary: ";
				std::cout << "(" << boundary.low.x << "," << boundary.low.y << ")";
				std::cout << "(" << boundary.high.x << "," << boundary.high.y << "); block " << boundary.block->id << "; die " << boundary.block->layer << std::endl;
			}

			std::cout << "DBG_CONTIGUITY>" << std::endl;
		}

		// then, walk boundaries and whenever one lower boundary is intersecting
		// with a block's outline, consider the blocks pair as contiguous
		// neighbours
		//
		if (ContiguityAnalysis::DBG) {

			std::cout << "DBG_CONTIGUITY> Determine intersecting boundaries for dies " << l << " and " << l + 1 << "; derive inter-die contiguity" << std::endl;
		}

		// walking vertical boundaries; this is sufficient for determining
		// overlaps in x- and y-dimension
		for (i1 = inter_die_boundaries_vert[l].begin(); i1 != inter_die_boundaries_vert[l].end(); ++i1) {

			ContiguityAnalysis::Boundary& b1 = (*i1);

			if (ContiguityAnalysis::DBG) {
				std::cout << "DBG_CONTIGUITY>  Currently considered vertical segment ";
				std::cout << "(" << b1.low.x << "," << b1.low.y << ")";
				std::cout << "(" << b1.high.x << "," << b1.high.y << "); block " << b1.block->id << "; die " << b1.block->layer << std::endl;
			}

			// the boundary b2, to be compared to b1, should be within the
			// x-range of b1.block; thus, we start from the next element (not
			// the same, in order to avoid comparison with itself) in the set
			// of sorted boundaries
			i2 = i1 + 1;
			for (; i2 != inter_die_boundaries_vert[l].end(); ++i2) {

				ContiguityAnalysis::Boundary& b2 = (*i2);

				// break condition; if b2 is outside of b1 (to the right
				// of b1.block), no intersection if feasible anymore
				if (b2.low.x > b1.block->bb.ur.x) {
					break;
				}

				// otherwise, some intersection _may_ exist, but only a)
				// for blocks of the other die and b) if there is some
				// overlap in y-direction
				if ((b2.block->layer != b1.block->layer) &&
						(b1.low.y <= b2.high.y && b2.low.y <= b1.high.y)
				   ) {
					// at this point, we know that b2 is intersecting
					// with b1 to some degree in _both_ dimensions;
					// thus, we can simplify the inter-die contiguity
					// analysis by checking only the vertical
					// boundaries 
					//
					// (TODO) drop; not required as of now
//					// determine amount of intersections / common
//					// boundaries and memorize within both blocks
//					common_boundary_vert = ContiguityAnalysis::common_boundary_vert(b1, b2);
//					// the boundaries b1 and b2 are vertical ones,
//					// thus we have to extract the horizontal common
//					// boundary from the blocks themselves
//					common_boundary_hor = ContiguityAnalysis::common_boundary_hor(b1.block, b2.block);

					// init neighbourship storage
					ContiguityAnalysis::ContiguousNeighbour neighbour_for_b1;
					ContiguityAnalysis::ContiguousNeighbour neighbour_for_b2;
					neighbour_for_b1.block = b2.block;
					neighbour_for_b2.block = b1.block;

					// (TODO) drop; not required as of now
//					// for b2 being right of b1, the common boundary to be
//					// stored in b1 is positive and the one for b2 is
//					// negative
//					if (b2.block->bb.ll.x > b1.block->bb.ll.x) {
//						neighbour_for_b1.common_boundary_inter_die_vert = common_boundary_vert;
//						neighbour_for_b2.common_boundary_inter_die_vert = -common_boundary_vert;
//					}
//					// b2 is left of b1, store negative value in b1 and
//					// positive in b2
//					else {
//						neighbour_for_b1.common_boundary_inter_die_vert = -common_boundary_vert;
//						neighbour_for_b2.common_boundary_inter_die_vert = common_boundary_vert;
//					}
//
//					// for b2 being atop of b1, the common boundary to
//					// be stored in b1 is positive and the one for b2
//					// is negative
//					if (b2.block->bb.ll.y > b1.block->bb.ll.y) {
//						neighbour_for_b1.common_boundary_inter_die_hor = common_boundary_hor;
//						neighbour_for_b2.common_boundary_inter_die_hor = -common_boundary_hor;
//					}
//					// b2 is below of b1, store negative value in b1
//					// and positive in b2
//					else {
//						neighbour_for_b1.common_boundary_inter_die_hor = -common_boundary_hor;
//						neighbour_for_b2.common_boundary_inter_die_hor = common_boundary_hor;
//					}

					// memorize within each block
					b1.block->contiguous_neighbours.push_back(neighbour_for_b1);
					b2.block->contiguous_neighbours.push_back(neighbour_for_b2);

					if (ContiguityAnalysis::DBG) {
						std::cout << "DBG_CONTIGUITY>   2-dimensional contiguity with block " << b2.block->id;
						std::cout << "; die " << b2.block->layer << ":";
						// (TODO) drop; not required as of now
//						std::cout << " (" << neighbour_for_b1.common_boundary_inter_die_hor;
//						std::cout << ", " << neighbour_for_b1.common_boundary_inter_die_vert;
//						std::cout << ");";
						std::cout << " dimensions of related intersection box:";
						Rect bb = Rect::determineIntersection(b1.block->bb, b2.block->bb);
						std::cout << " (" << bb.w;
						std::cout << ", " << bb.h;
						std::cout << ")" << std::endl;
					}
				}
			}
		}

		if (ContiguityAnalysis::DBG) {
			std::cout << "DBG_CONTIGUITY>" << std::endl;
		}
	}

	if (ContiguityAnalysis::DBG) {
		std::cout << "DBG_CONTIGUITY> Contiguous neighbours for all blocks:" << std::endl;

		for (Block const& block : blocks) {

			std::cout << "DBG_CONTIGUITY>  Block " << block.id << ":" << std::endl;

			for (auto& neighbour : block.contiguous_neighbours) {
				std::cout << "DBG_CONTIGUITY>   " << neighbour.block->id;
				// (TODO) drop; not required as of now
//				std::cout << " (" << neighbour.common_boundary_hor;
//				std::cout << ", " << neighbour.common_boundary_vert;
//				std::cout << ", " << neighbour.common_boundary_inter_die_hor;
//				std::cout << ", " << neighbour.common_boundary_inter_die_vert;
//				std::cout << ")";
				std::cout << std::endl;
			}
		}

		std::cout << std::endl;
	}
}

/// helper to check vertical boundaries
inline bool ContiguityAnalysis::boundaries_vert_comp(ContiguityAnalysis::Boundary const& b1, ContiguityAnalysis::Boundary const& b2) {
	return (
			// x-coordinates are the first criterion; note
			// that it's sufficient to compare the first
			// (lower) points since it's a vertical segment
			(b1.low.x < b2.low.x)
			// for boundaries with same x-coordinate, resolve
			// equal values by considering the boundaries'
			// y-coordinate
			|| ((b1.low.x == b2.low.x) && (b1.low.y < b2.low.y))
			// there will also be cases when segments start on
			// the same x- and y-coordinate; then, resolve by
			// block order, i.e., the segment of the left
			// block comes first
			|| ((b1.low.x == b2.low.x) && (b1.low.y == b2.low.y) && (b1.block->bb.ll.x < b2.block->bb.ll.x))
	       );
};

/// helper to check horizontal boundaries
inline bool ContiguityAnalysis::boundaries_hor_comp(ContiguityAnalysis::Boundary const& b1, ContiguityAnalysis::Boundary const& b2) {
	return (
			// y-coordinates are the first criterion; note
			// that it's sufficient to compare the first
			// (left) points since it's a horizontal segment
			(b1.low.y < b2.low.y)
			// for boundaries with same y-coordinate, resolve
			// equal values by considering the boundaries'
			// x-coordinate
			|| ((b1.low.y == b2.low.y) && (b1.low.x < b2.low.x))
			// there will also be cases when segments start on
			// the same y- and x-coordinate; then, resolve by
			// block order, i.e., the segment of the lower
			// block comes first
			|| ((b1.low.y == b2.low.y) && (b1.low.x == b2.low.x) && (b1.block->bb.ll.y < b2.block->bb.ll.y))
	       );
};

// (TODO) drop; not required as of now
//
//inline double ContiguityAnalysis::common_boundary_vert(ContiguityAnalysis::Boundary const& b1, ContiguityAnalysis::Boundary const& b2) {
//	double y_lower, y_upper;
//
//	y_lower = std::max(b1.low.y, b2.low.y);
//	y_upper = std::min(b1.high.y, b2.high.y);
//
//	return y_upper - y_lower;
//};
//
//inline double ContiguityAnalysis::common_boundary_hor(ContiguityAnalysis::Boundary const& b1, ContiguityAnalysis::Boundary const& b2) {
//	double x_left, x_right;
//
//	x_left = std::max(b1.low.x, b2.low.x);
//	x_right = std::min(b1.high.x, b2.high.x);
//
//	return x_right - x_left;
//};
//
//inline double ContiguityAnalysis::common_boundary_hor(Block const* b1, Block const* b2) {
//	double x_left, x_right;
//
//	x_left = std::max(b1->bb.ll.x, b2->bb.ll.x);
//	x_right = std::min(b1->bb.ur.x, b2->bb.ur.x);
//
//	return x_right - x_left;
//};
