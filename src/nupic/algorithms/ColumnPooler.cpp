/* ---------------------------------------------------------------------
 * Numenta Platform for Intelligent Computing (NuPIC)
 * Copyright (C) 2013, Numenta, Inc.
 * Copyright (C) 2019, David McDougall
 *
 * Unless you have an agreement with Numenta, Inc., for a separate license for
 * this software code, the following terms and conditions apply:
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
 * ----------------------------------------------------------------------
 */

/** @file
 * Implementation of ColumnPooler
 */

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip> // std::setprecision
#include <functional> // function

// #include <nupic/algorithms/ColumnPooler.hpp>
#include <nupic/types/Types.hpp>
#include <nupic/types/Serializable.hpp>
#include <nupic/ntypes/Sdr.hpp>
#include <nupic/ntypes/Sdr_Metrics.hpp>
#include <nupic/algorithms/Connections.hpp>
#include <nupic/math/Math.hpp>
#include <nupic/math/Topology.hpp>

using namespace std;
using namespace nupic;
using namespace nupic::algorithms::spatial_pooler;
using namespace nupic::math::topology;
using nupic::utils::VectorHelpers;

// Topology( location, potentialPooler )
typedef function<void(SDR&, SDR&)> Topology_t;

class ColumnPooler // : public Serializable
{
public:

    Connections proximalConnections;
    Connections distalConnections;

    vector<Real> tieBreaker_;


  ColumnPooler(
    const vector<UInt> proximalInputDimensions,
    const vector<UInt> distalInputDimensions,
    const vector<UInt> cellDimensions,

    Real minimumSparsity,
    Real maximumSparsity,

    UInt       proximalSegments,
    Topology_t potentialPool,
    UInt       proximalSegmentThreshold,
    Permanence proximalIncrement,
    Permanence proximalDecrement,
    Permanence proximalSynapseThreshold,

    UInt       distalSegments,
    UInt       distalSegmentSize,
    UInt       distalSegmentThreshold,
    Permanence distalIncrement,
    Permanence distalDecrement,
    Permanence distalSynapseThreshold,

    Real period,
    Int  seed,
    bool verbose)
  {

    SDR proximal( proximalInputDimensions );
    SDR distal( distalInputDimensions );
    SDR cells( cellDimensions );

    // Setup the proximal segments & synapses.
    proximalConnections.initialize(cells.size, proximalSynapseThreshold);
    for(UInt i = 0u; i < cells.size; ++i) {
      cells.setFlatSparse({ i });
      for(UInt s = 0u; s < proximalSegments; ++s) {
        auto segment = proximalConnections.createSegment( i );

        // Make synapses, first find the potential pool.
        potentialPool( cells, proximal );

        for(const auto presyn : proximal.getFlatSparse() ) {
          auto permanence =
          proximalConnections.createSynapse( segment, presyn, permanence);
        }
        proximalConnections.raisePermanencesToThreshold( segment,
                            proximalSynapseThreshold, proximalSegmentThreshold );
      }
    }

    // TODO Assign all of the parameters to attributes...


    rng_ = Random(seed);

    iterationNum_ = 0u;
    iterationLearnNum_ = 0u;

    tieBreaker_.resize(numColumns_);
    for(Size i = 0; i < numColumns_; ++i) {
      tieBreaker_[i] = 0.01f * rng_.getReal64();
    }

    activeDutyCycles_.assign(numColumns_, 0);

    if( verbose ) {
      // Print everything
    }
  }


  void reset() {
    1/0
  }


  void compute(SDR &input, bool learn, SDR &active)
  {
    // Update bookkeeping
    iterationNum_++;
    if( learn )
      iterationLearnNum_++;

    auto cellExcitements = activateProximalDendrites(...);
    auto predictiveCells = activateDistalDendrites(...);

    auto activeCells = activateCells( cellExcitements, predictiveCells );

    if( learn ) {
      learnProximalDendrites()
      learnDistalDendrites()
    }
  }


  vector<Real>& activateProximalSegments() {
    // Proximal Feed Forward Excitement
    const auto numProxSegs = proximalConnections.numSegments();
    vector<UInt32> rawOverlaps( numProxSegs, 0.0f );
    proximalConnections.computeActivity(rawOverlaps, input.getFlatSparse());

    // Process Each Cell
    vector<Real> overlaps( active.size );
    vector<Real> segments( proximalSegments );
    for(auto cell = 0u; cell < active.size; ++cell) {
      // Process Each Proximal Segment on Cell
      auto segments = proximalConnections.segmentsForCell();
      for(auto seg = 0u; seg < proximalSegments; ++seg) {
        
      }
    }


    for(auto i = 0u; i < numProxSegs; ++i) {
      Real ovlp = (Real) rawOverlaps[i]; // Typecase to floating point.
      // Proximal Tie Breaker
      // NOTE: Apply tiebreakers before boosting, so that boosting is applied
      //   to the tiebreakers.  This is important so that the tiebreakers don't
      //   hurt the entropy of the result by biasing some mini-columns to
      //   activte more often than others.
      ovlp += tieBreaker_[i];

      // Normalize Proximal Excitement by the number of connected synapses.
      auto nConSyns = TODO;
      if( nConSyns == 0 )
        overlaps[i] = 1.0f;
      else
        overlaps[i] = ovlp / nConSyns

      // Boosting Function
      boostOverlaps_(overlaps_, boostedOverlaps_);
    }

    // Maximum Segment Overlap Becomes Cell Overlap
    1/0

    // Apply Stability & Fatigue
    1/0

    return cellExcitements;
  }


  void learnProximalSegments() {
    // Adapt Proximal Segments
    for(const auto &column : active.getFlatSparse()) {
      connections_.adaptSegment(column, input, synPermActiveInc_, synPermInactiveDec_);
      connections_.raisePermanencesToThreshold(
                                  column, synPermConnected_, stimulusThreshold_);
    }
    // Bump up weak cells
    for (UInt i = 0; i < numColumns_; i++) {
      if (overlapDutyCycles_[i] >= minOverlapDutyCycles_[i]) {
        continue;
      }
      connections_.bumpSegment( i, synPermBelowStimulusInc_ );
    }
  }


  vector<UInt>& activateDistalDendrites() {
    // Concatenate external predictive inputs with self.active
    1/0

    // Compute Feed Forward Overlaps.
    1/0

    // Find Active Segments
    1/0

    // Update book keeping
    lastUsedIterationForSegment += active

    // Find Matching Segments
    1/0

    return cellsWithActiveSegments;
  }


  void learnDistalDendrites( SDR &activeCells ) {

    // Adapt Predicted Active Cells
    1/0

    // Grow Unpredicted Active Cells
    1/0

    // Punish Predicted Inactive Cells
    1/0
  }





Real ColumnPooler::initPermConnected_() {
  Real p =
      synPermConnected_ + (connections::maxPermanence - synPermConnected_) * rng_.getReal64();

  return round5_(p);
}


Real ColumnPooler::initPermNonConnected_() {
  Real p = synPermConnected_ * rng_.getReal64();
  return round5_(p);
}


vector<Real> ColumnPooler::initPermanence_(const vector<UInt> &potential, //TODO make potential sparse
                                            Real connectedPct) {
  vector<Real> perm(numInputs_, 0);
  for (UInt i = 0; i < numInputs_; i++) {
    if (potential[i] < 1) {
      continue;
    }

    if (rng_.getReal64() <= connectedPct) {
      perm[i] = initPermConnected_();
    } else {
      perm[i] = initPermNonConnected_();
    }
  }

  return perm;
}


void ColumnPooler::inhibitColumns_(const vector<Real> &overlaps,
                                    vector<UInt> &activeColumns) const {
  Real density = localAreaDensity_;
  if (numActiveColumnsPerInhArea_ > 0) {
    UInt inhibitionArea =
        pow((Real)(2 * inhibitionRadius_ + 1), (Real)columnDimensions_.size());
    inhibitionArea = min(inhibitionArea, numColumns_);
    density = ((Real)numActiveColumnsPerInhArea_) / inhibitionArea;
    density = min(density, (Real)MAX_LOCALAREADENSITY);
  }

  if (globalInhibition_ ||
      inhibitionRadius_ >
          *max_element(columnDimensions_.begin(), columnDimensions_.end())) {
    inhibitColumnsGlobal_(overlaps, density, activeColumns);
  } else {
    inhibitColumnsLocal_(overlaps, density, activeColumns);
  }
}


void ColumnPooler::boostOverlaps_(const vector<UInt> &overlaps,
                                   vector<Real> &boosted) const {
  const Real denominator = 1.0f / log2( localAreaDensity_ );
  for (UInt i = 0; i < numColumns_; i++) {
    boosted[i] = overlaps[i] * log2(activeDutyCycles_[i]) * denominator;
  }
}


void ColumnPooler::inhibitColumnsGlobal_(const vector<Real> &overlaps,
                          Real density,
                          vector<UInt> &activeColumns) const
{
  NTA_ASSERT(!overlaps.empty());
  NTA_ASSERT(density > 0.0f && density <= 1.0f);
  const UInt miniColumns = columnDimensions_.back();
  const UInt macroColumns = numColumns_ / miniColumns;
  const UInt numDesired = (UInt)(density * miniColumns + .5);
  NTA_CHECK(numDesired > 0) << "Not enough columns (" << miniColumns << ") "
                            << "for desired density (" << density << ").";

  // Compare the column indexes by their overlap.
  auto compare = [&overlaps](const UInt &a, const UInt &b) -> bool
    {return overlaps[a] > overlaps[b];};

  activeColumns.clear();
  activeColumns.reserve(miniColumns + numDesired * macroColumns );

  for(UInt offset = 0; offset < numColumns_; offset += miniColumns)
  {
    // Sort the columns by the amount of overlap.  First make a list of all of
    // the mini-column indexes.
    auto outPtr = activeColumns.end();
    for(UInt i = 0; i < miniColumns; i++)
      activeColumns.push_back( i + offset );
    // Do a partial sort to divide the winners from the losers.  This sort is
    // faster than a regular sort because it stops after it partitions the
    // elements about the Nth element, with all elements on their correct side of
    // the Nth element.
    std::nth_element(
      outPtr,
      outPtr + numDesired,
      activeColumns.end(),
      compare);
    // Remove the columns which lost the competition.
    activeColumns.resize( activeColumns.size() - (miniColumns - numDesired) );
    // Finish sorting the winner columns by their overlap.
    std::sort(outPtr, activeColumns.end(), compare);
    // Remove sub-threshold winners
    while( activeColumns.size() > offset &&
           overlaps[activeColumns.back()] < stimulusThreshold_)
        activeColumns.pop_back();
  }
}

