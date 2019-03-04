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
#include <nupic/types/Sdr.hpp>
#include <nupic/utils/SdrMetrics.hpp>
#include <nupic/algorithms/Connections.hpp>
#include <nupic/algorithms/TemporalMemory.hpp>
#include <nupic/math/Math.hpp>
#include <nupic/math/Topology.hpp>

namespace nupic {
namespace algorithms {
namespace column_pooler {

using namespace std;
using namespace nupic;
using namespace nupic::math::topology;
using namespace nupic::algorithms::connections;
using nupic::algorithms::temporal_memory::TemporalMemory;


// TODO: Connections learning rules are different.


/* Topology( location, potentialPool, RNG ) */
typedef function<void(SDR&, SDR&, Random&)> Topology_t;


struct Parameters
{
public:
  vector<UInt> proximalInputDimensions;
  vector<UInt> distalInputDimensions;
  vector<UInt> inhibitionDimensions;
  UInt         cellsPerInhibitionArea;

  Real sparsity;

  // Maybe shared pointer?
  Topology_t* potentialPool;
  UInt        proximalSegments;
  UInt        proximalSegmentThreshold;
  Permanence  proximalIncrement;
  Permanence  proximalDecrement;
  Permanence  proximalSynapseThreshold;

  UInt       distalMaxSegments;
  UInt       distalMaxSynapsesPerSegment;
  UInt       distalSegmentThreshold;
  UInt       distalSegmentMatch;
  UInt       distalAddSynapses;
  Permanence distalInitialPermanence; // TODO: Add to python bindings, add to MNIST
  Permanence distalIncrement;
  Permanence distalDecrement;
  Permanence distalMispredictDecrement;
  Permanence distalSynapseThreshold;

  Real stability_rate;    // TODO: Fix naming convention
  Real fatigue_rate;      // TODO: Fix naming convention

  Real period;
  Int  seed;
  bool verbose;
};

const Parameters DefaultParameters = {
  // TODO
};

const Parameters DefaultParametersNoDistalDendrites = {
  // TODO
};

class ColumnPooler // : public Serializable
{
private:
  Parameters args_;
  vector<UInt> cellDimensions_;

  vector<UInt32> rawOverlaps_;
  vector<UInt> proximalMaxSegment_;

  Random rng_;
  vector<Real> tieBreaker_;
  UInt iterationNum_;
  UInt iterationLearnNum_;
  vector<Real> X_act;
  vector<Real> X_inact;

  // This is used by boosting.
  SDR_ActivationFrequency *AF_;

public:
  const Parameters   &parameters     = args_;
  const vector<UInt> &cellDimensions = cellDimensions_;

  const UInt &iterationNum      = iterationNum_;
  const UInt &iterationLearnNum = iterationLearnNum_;

  /**
   * The proximal connections have regular structure.  Cells in an inhibition
   * area are contiguous and all segments on a cell are contiguous.  This
   * allows fast index math instead of slowers lists of ID's.
   */
  Connections proximalConnections;

  TemporalMemory distalConnections;

  ColumnPooler() {}; //default constructor, must call initialize to setup properly

  ColumnPooler( const Parameters &parameters )
    { initialize( parameters ); }

  void initialize( const Parameters &parameters ) {
    args_ = parameters;

    SDR proximalInputs(  args_.proximalInputDimensions );
    SDR inhibitionAreas( args_.inhibitionDimensions );
    cellDimensions_ = inhibitionAreas.dimensions;
    cellDimensions_.push_back( args_.cellsPerInhibitionArea );
    SDR cells( cellDimensions_ );
    rng_ = Random(args_.seed);

    // Setup the proximal segments & synapses.
    proximalConnections.initialize(cells.size, args_.proximalSynapseThreshold);
    SDR_Sparsity PP_Sp(            proximalInputs.dimensions, 10 * proximalInputs.size);
    SDR_ActivationFrequency PP_AF( proximalInputs.dimensions, 10 * proximalInputs.size);
    UInt cell = 0u;
    for(auto inhib = 0u; inhib < inhibitionAreas.size; ++inhib) {
      inhibitionAreas.setSparse(SDR_sparse_t{ inhib });
      for(auto c = 0u; c < args_.cellsPerInhibitionArea; ++c, ++cell) {
        for(auto s = 0u; s < args_.proximalSegments; ++s) {
          auto segment = proximalConnections.createSegment( cell );

          // Make synapses.
          NTA_CHECK(args_.potentialPool != nullptr);
          (*args_.potentialPool)( inhibitionAreas, proximalInputs, rng_ );
          for(const auto presyn : proximalInputs.getSparse() ) {
            auto permanence = initProximalPermanence();
            proximalConnections.createSynapse( segment, presyn, permanence);
          }
          proximalConnections.raisePermanencesToThreshold( segment,
                          args_.proximalSynapseThreshold, args_.proximalSegmentThreshold );
          PP_Sp.addData( proximalInputs );
          PP_AF.addData( proximalInputs );
        }
      }
    }
    tieBreaker_.resize( proximalConnections.numSegments() );
    for(auto i = 0u; i < tieBreaker_.size(); ++i) {
      tieBreaker_[i] = 0.01f * rng_.getReal64();
    }
    proximalMaxSegment_.resize( cells.size );
    AF_ = new SDR_ActivationFrequency( {cells.size, args_.proximalSegments}, args_.period );
    AF_->initializeToValue( args_.sparsity / args_.proximalSegments );

    // Setup the distal dendrites
    distalConnections.initialize(
        /* columnDimensions */            cellDimensions,
        /* cellsPerColumn */              1,
        /* activationThreshold */         args_.distalSegmentThreshold,
        /* initialPermanence */           args_.distalInitialPermanence,
        /* connectedPermanence */         args_.distalSynapseThreshold,
        /* minThreshold */                args_.distalSegmentMatch,
        /* maxNewSynapseCount */          args_.distalAddSynapses,
        /* permanenceIncrement */         args_.distalIncrement,
        /* permanenceDecrement */         args_.distalDecrement,
        /* predictedSegmentDecrement */   args_.distalMispredictDecrement,
        /* seed */                        rng_(),
        /* maxSegmentsPerCell */          args_.distalMaxSegments,
        /* maxSynapsesPerSegment */       args_.distalMaxSynapsesPerSegment,
        /* checkInputs */                 true,
        /* extra */                       SDR(args_.distalInputDimensions).size);

    iterationNum_      = 0u;
    iterationLearnNum_ = 0u;

    reset();

    if( PP_Sp.min() * proximalInputs.size < args_.proximalSegmentThreshold )
      NTA_WARN << "WARNING: Proximal segment has fewer synapses than the segment threshold." << endl;
    if( PP_AF.min() == 0.0f )
      NTA_WARN << "WARNING: Proximal input is unused." << endl;

    if( args_.verbose ) {
      // TODO: Print all parameters
      cout << "Potential Pool Statistics:" << endl
           << PP_Sp
           << PP_AF << endl;
    }
  }


  void reset() {
    X_act.assign( proximalConnections.numCells(), 0.0f );
    X_inact.assign( proximalConnections.numCells(), 0.0f );
    // TODO Zero Previous Updates
    distalConnections.reset();
  }


  void compute(
        const SDR& proximalInputActive,
        bool learn,
        SDR& active) {
    SDR none( args_.distalInputDimensions );
    SDR none2( active.dimensions );
    compute(proximalInputActive, proximalInputActive, none, none, learn, active, none2 );
  }

  void compute(
        const SDR& proximalInputActive,
        const SDR& proximalInputLearning,
        const SDR& distalInputActive,
        const SDR& distalInputLearning,
        bool learn,
        SDR& active,
        SDR& learning) {
    NTA_CHECK( proximalInputActive.dimensions   == args_.proximalInputDimensions );
    NTA_CHECK( proximalInputLearning.dimensions == args_.proximalInputDimensions );
    NTA_CHECK( distalInputActive.dimensions     == args_.distalInputDimensions );
    NTA_CHECK( distalInputLearning.dimensions   == args_.distalInputDimensions );
    NTA_CHECK( active.dimensions                == cellDimensions );
    NTA_CHECK( learning.dimensions              == cellDimensions );

    // Update bookkeeping
    iterationNum_++;
    if( learn )
      iterationLearnNum_++;

    // Feed Forward Input / Proximal Dendrites
    vector<Real> cellOverlaps( active.size );
    activateProximalDendrites( proximalInputActive, cellOverlaps );

    distalConnections.activateDendrites(learn, distalInputActive, distalInputLearning);
    SDR predictedCells( cellDimensions );
    distalConnections.getPredictiveCells( predictedCells );

    activateCells( cellOverlaps, predictedCells, active );

    // Learn
    distalConnections.activateCells( active, learn );
    if( learn ) {
      learnProximalDendrites( proximalInputActive, proximalInputLearning, active );
    }
  }


  // TODO: apply segment overlap threshold
  void activateProximalDendrites( const SDR &feedForwardInputs,
                                  vector<Real> &cellExcitements )
  {
    // Proximal Feed Forward Excitement
    rawOverlaps_.assign( proximalConnections.numSegments(), 0.0f );
    proximalConnections.computeActivity(rawOverlaps_, feedForwardInputs.getSparse());

    // Setup for Boosting
    const Real denominator = 1.0f / log2( args_.sparsity / args_.proximalSegments );
    const auto &af = AF_->activationFrequency;

    // Process Each Segment of Each Cell
    for(auto cell = 0u; cell < proximalConnections.numCells(); ++cell) {
      Real maxOverlap    = -1.0;
      UInt maxSegment    = -1;
      // UInt maxRawOverlap = 0u;
      for(const auto &segment : proximalConnections.segmentsForCell( cell ) ) {
        const auto raw = rawOverlaps_[segment];
        // maxRawOverlap = raw > maxRawOverlap ? raw : maxRawOverlap;

        Real overlap = (Real) raw; // Typecase to floating point.

        // Proximal Tie Breaker
        // NOTE: Apply tiebreakers before boosting, so that boosting is applied
        //   to the tiebreakers.  This is important so that the tiebreakers don't
        //   hurt the entropy of the result by biasing some mini-columns to
        //   activte more often than others.
        overlap += tieBreaker_[segment];

        // Normalize Proximal Excitement by the number of connected synapses.
        const auto nConSyns = proximalConnections.dataForSegment( segment ).numConnected;
        if( nConSyns == 0 )
          overlap = 1.0f;
          // overlap = 0.0f;
        else
          overlap /= nConSyns;

        // Boosting Function
        overlap *= log2( af[segment] ) * denominator;

        // Maximum Segment Overlap Becomes Cell Overlap
        if( overlap > maxOverlap ) {
          maxOverlap = overlap;
          maxSegment = segment;
        }
      }
      proximalMaxSegment_[cell] = maxSegment - cell;

      cellExcitements[cell] = maxOverlap;
      // Apply Stability & Fatigue
      X_act[cell]   += (1.0f - args_.stability_rate) * (maxOverlap - X_act[cell] - X_inact[cell]);
      X_inact[cell] += args_.fatigue_rate * (maxOverlap - X_inact[cell]);
      cellExcitements[cell] = X_act[cell];
    }
  }


  void activateCells( vector<Real> &overlaps,
                      const SDR    &predictiveCells,
                            SDR    &activeCells)
  {
    const UInt inhibitionAreas = activeCells.size / args_.cellsPerInhibitionArea;
    const UInt numDesired = (UInt) std::round(args_.sparsity * args_.cellsPerInhibitionArea);
    NTA_CHECK(numDesired > 0) << "Not enough cellsPerInhibitionArea ("
      << args_.cellsPerInhibitionArea << ") for desired density (" << args_.sparsity << ").";

    // Compare the cell indexes by their overlap.
    auto compare = [&overlaps](const UInt &a, const UInt &b) -> bool
      {return overlaps[a] > overlaps[b];};

    auto &active = activeCells.getSparse();
    active.clear();
    active.reserve(args_.cellsPerInhibitionArea + numDesired * inhibitionAreas );

    for(UInt offset = 0u; offset < activeCells.size; offset += args_.cellsPerInhibitionArea)
    {
      // Sort the columns by the amount of overlap.  First make a list of all of
      // the mini-column indexes.
      const auto activeBegin    = active.end();
      const auto activeBeginIdx = active.size();
      for(UInt i = 0u; i < args_.cellsPerInhibitionArea; i++)
        active.push_back( i + offset );
      // Do a partial sort to divide the winners from the losers.  This sort is
      // faster than a regular sort because it stops after it partitions the
      // elements about the Nth element, with all elements on their correct side of
      // the Nth element.
      std::nth_element(
        activeBegin,
        activeBegin + numDesired,
        active.end(),
        compare);
      // Remove the columns which lost the competition.
      active.resize( active.size() - (args_.cellsPerInhibitionArea - numDesired) );

      // Remove sub-threshold winners
      for(UInt i = activeBeginIdx; i < active.size(); )
      {
        if( rawOverlaps_[ active[i] ] < args_.proximalSegmentThreshold ) {
          active[i] = active.back();
          active.pop_back();
        }
        else
           ++i;
      }
    }
    activeCells.setSparse( active );
  }


  void learnProximalDendrites( const SDR &proximalInputActive,
                               const SDR &proximalInputLearning,
                               const SDR &active ) {
    SDR AF_SDR( AF_->dimensions );
    auto &activeSegments = AF_SDR.getCoordinates();
    for(const auto &cell : active.getSparse())
    {
      // Adapt Proximal Segments
      NTA_CHECK(cell < proximalMaxSegment_.size()) << "cell oob! " << cell << " < " << proximalMaxSegment_.size();
      const auto &maxSegment = proximalMaxSegment_[cell];
      proximalConnections.adaptSegment(maxSegment, proximalInputActive,
                                       args_.proximalIncrement, args_.proximalDecrement);

      proximalConnections.raisePermanencesToThreshold(maxSegment,
                                   args_.proximalSynapseThreshold,
                                   args_.proximalSegmentThreshold);

      activeSegments[0].push_back(cell);
      activeSegments[1].push_back(maxSegment);
    }
    // TODO: Grow new synapses from the learning inputs?

    AF_SDR.setCoordinates( activeSegments );
    AF_->addData( AF_SDR );
  }


  Real initProximalPermanence(Real connectedPct = 0.5f) {
    if( rng_.getReal64() <= connectedPct )
        return args_.proximalSynapseThreshold +
          (1.0f - args_.proximalSynapseThreshold) * rng_.getReal64();
      else
        return args_.proximalSynapseThreshold * rng_.getReal64();
  }


  void setParameters( Parameters &newParameters )
  {
    if( newParameters.proximalInputDimensions     != parameters.proximalInputDimensions ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.distalInputDimensions       != parameters.distalInputDimensions ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.inhibitionDimensions        != parameters.inhibitionDimensions ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.cellsPerInhibitionArea      != parameters.cellsPerInhibitionArea ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.sparsity                    != parameters.sparsity ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.potentialPool               != parameters.potentialPool ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.proximalSegments            != parameters.proximalSegments ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.proximalSegmentThreshold    != parameters.proximalSegmentThreshold ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.proximalIncrement           != parameters.proximalIncrement ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.proximalDecrement           != parameters.proximalDecrement ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.proximalSynapseThreshold    != parameters.proximalSynapseThreshold ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.distalMaxSegments           != parameters.distalMaxSegments ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.distalMaxSynapsesPerSegment != parameters.distalMaxSynapsesPerSegment ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.distalSegmentThreshold      != parameters.distalSegmentThreshold ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.distalSegmentMatch          != parameters.distalSegmentMatch ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.distalAddSynapses           != parameters.distalAddSynapses ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.distalIncrement             != parameters.distalIncrement ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.distalDecrement             != parameters.distalDecrement ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.distalMispredictDecrement   != parameters.distalMispredictDecrement ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.distalSynapseThreshold      != parameters.distalSynapseThreshold ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.stability_rate              != parameters.stability_rate ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.fatigue_rate                != parameters.fatigue_rate ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.period                      != parameters.period ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.seed                        != parameters.seed ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.verbose                     != parameters.verbose ) {
      NTA_THROW << "Setter unimplemented.";
    }
  }
};


class DefaultTopology : public Topology_t
{
public:
  Real potentialPct;
  Real potentialRadius;
  bool wrapAround;

  DefaultTopology(Real potentialPct, Real radius, bool wrapAround)
  : potentialPct(potentialPct), potentialRadius(radius), wrapAround(wrapAround) 
  {}

  void operator()(SDR& cell, SDR& potentialPool, Random &rng) {

    vector<vector<UInt>> inputCoords;//(cell.dimensions.size());
    for(auto i = 0u; i < cell.dimensions.size(); i++)
    {
      const Real columnCoord = cell.getCoordinates()[i][0];
      const Real inputCoord = (columnCoord + 0.5f) *
                              (potentialPool.dimensions[i] / (Real)cell.dimensions[i]);
      inputCoords.push_back({ (UInt32)floor(inputCoord) });
    }
    potentialPool.setCoordinates(inputCoords);
    NTA_CHECK(potentialPool.getSparse().size() == 1u);
    const auto centerInput = potentialPool.getSparse()[0];

    vector<UInt> columnInputs;
    if (wrapAround) {
      for (UInt input : WrappingNeighborhood(centerInput, potentialRadius, potentialPool.dimensions)) {
        columnInputs.push_back(input);
      }
    } else {
      for (UInt input :
           Neighborhood(centerInput, potentialRadius, potentialPool.dimensions)) {
        columnInputs.push_back(input);
      }
    }

    const UInt numPotential = (UInt)round(columnInputs.size() * potentialPct);
    const auto selectedInputs = rng.sample<UInt>(columnInputs, numPotential);
    potentialPool.setSparse( selectedInputs );
  }
};

} // End namespace column_pooler
} // End namespace algorithmn
} // End namespace nupic
