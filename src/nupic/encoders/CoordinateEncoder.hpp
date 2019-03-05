/* ---------------------------------------------------------------------
 * Numenta Platform for Intelligent Computing (NuPIC)
 * Copyright (C) 2019, David McDougall
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero Public License for more details.
 *
 * You should have received a copy of the GNU Affero Public License
 * along with this program.  If not, see http://www.gnu.org/licenses.
 *
 * http://numenta.org/licenses/
 * ---------------------------------------------------------------------
 */

/** @file
 * Define the CoordinateEncoder
 */

#ifndef NTA_ENCODERS_COORD
#define NTA_ENCODERS_COORD

#include <nupic/types/Sdr.hpp>
#include <nupic/types/Types.hpp>
#include <nupic/types/Serializable.hpp>
#include <nupic/utils/Random.hpp>

namespace nupic {

// TODO: python bindings
// TODO: unit tests
// TODO: docs

/**
 * TODO DOCUMENTATION
      COPY FROM NUPIC!
 */
class CoordinateEncoder // : public Serializable // TODO Serializable unimplemented!
{
private:
  UInt size_;
  Real sparsity_;
  UInt ndim_;
  Real radius_;
  UInt seed_;

public:
  /**
   * TODO DOCUMENTATION
   * https://arxiv.org/pdf/1602.05925.pdf
   */
  CoordinateEncoder(UInt size, Real sparsity, UInt ndim, Real radius, UInt seed = 0u)
    { initialize(size, sparsity, ndim, radius, seed); }

  void initialize(UInt size, Real sparsity, UInt ndim, Real radius, UInt seed = 0u) {
    size_       = size;
    sparsity_   = sparsity;
    ndim_       = ndim;
    radius_     = radius;
    // Use the given seed to make a better, more randomized seed.
    Random apple( seed );
    seed_ = apple() / 2;

    NTA_CHECK(sparsity >= 0.0f);
    NTA_CHECK(sparsity <= 1.0f);
    NTA_CHECK(radius > 0.0f);
  }

  const UInt &size      = size_;
  const Real &sparsity  = sparsity_;
  const UInt &ndim      = ndim_;
  const Real &radius    = radius_;

  /**
   * TODO DOCUMENTATION
   */
  void encode(vector<Real> coordinate, SDR &output) const {
    NTA_CHECK( coordinate.size() == ndim );
    NTA_CHECK( output.size == size );
    const UInt n_active   = round(size * sparsity);

    // Dimensions of the squarest area with N-dimensions & area of n_active
    vector<UInt> neighborhoodDimensions( ndim );
    auto remainder = n_active;
    for(UInt dim = ndim; dim > 0u; --dim)
    {
      UInt X = round(pow( (Real) remainder, (Real) 1.0f / dim ));
      remainder /= X;
      neighborhoodDimensions[dim - 1u]  = X;
    }
    NTA_ASSERT( remainder == 1 );
    // Use an SDR to find the coordinates of every location in the neighborhood.
    SDR neighborhood( neighborhoodDimensions );
    SDR_sparse_t allNeighbors( neighborhood.size );
    iota( allNeighbors.begin(), allNeighbors.end(), 0u );
    neighborhood.setSparse( allNeighbors );
    const auto &neighborOffsets = neighborhood.getCoordinates();

    // Find where we are in the space.
    vector<UInt> index( ndim );
    for(UInt dim = 0; dim < ndim; ++dim) {
      const Real resolution = (Real) 2.0f * radius / neighborhood.dimensions[dim];
      index[dim] = seed_ + (UInt) (coordinate[dim] / resolution);
    }

    // Iterate through the area near this location.  Hash each nearby location
    // and use each hash to set a bit in the output SDR.
    SDR_dense_t data( size, 0 );
    hash<std::string> h;
    for(UInt loc = 0; loc < neighborhood.getSum(); ++loc) {
      std::stringstream temp;
      for(UInt d = 0; d < ndim; ++d)
        temp << index[d] + neighborOffsets[d][loc];
      UInt bucket = h(temp.str()) % size;
      // Don't worry about hash collisions.  Instead measure the critical
      // properties of the encoder in unit tests and quantify how significant
      // the hash collisions are.  This encoder can not fix the collisions
      // because it does not record past encodings.  Collisions cause small
      // deviations in the sparsity or semantic similarity, depending on how
      // they're handled.
      // TODO: Calculate the probability of a hash collision and account for
      // it in n_active.
      data[bucket] = 1u;
    }
    output.setDense( data );
  }
};     // End class CoordinateEncoder

}      // End namespace nupic
#endif // End ifdef NTA_ENCODERS_COORD
