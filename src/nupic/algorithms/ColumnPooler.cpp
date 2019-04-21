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
 * ---------------------------------------------------------------------- */

/** @file
 * Implementation of ColumnPooler
 */

#include <iostream>
#include <vector>
#include <string>
#include <algorithm> // accumulate, max
#include <limits>  // numeric_limits
#include <iomanip> // setprecision
#include <functional> // function, multiplies

// TODO: #include <nupic/algorithms/ColumnPooler.hpp>
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
using namespace nupic::sdr;
using namespace nupic::math::topology;
using namespace nupic::algorithms::connections;
using nupic::algorithms::temporal_memory::TemporalMemory;


typedef function<Permanence(Random&, const SDR&, const SDR&)> InitialPermanence_t;

InitialPermanence_t defaultProximalInitialPermanence(
                        Permanence connectedThreshold, Real connectedPct)
{
  return [=](Random &rng, const SDR& presyn, const SDR& postsyn) -> Permanence {
    if( rng.getReal64() <= connectedPct )
        return connectedThreshold +
          (1.0f - connectedThreshold) * rng.getReal64();
      else
        return connectedThreshold * rng.getReal64();
  };
}


struct Parameters
{
public:
  // TODO SENSIBLE DEFAULTS
  vector<UInt> proximalInputDimensions;
  vector<UInt> inhibitionDimensions;
  UInt         cellsPerInhibitionArea;

  Real minSparsity            = 0.02f;
  Real maxDepolarizedSparsity = 3.0f;
  Real maxBurstSparsity       = 4.0f;

  Topology_t  potentialPool     = NoTopology(1.0f);
  UInt        proximalSegments  = 1u;
  Permanence  proximalIncrement = 0.01f;
  Permanence  proximalDecrement = 0.002f;
  Permanence  proximalSynapseThreshold = 0.40f;
  UInt        proximalSegmentThreshold = 1u;
  InitialPermanence_t proximalInitialPermanence =
                                  defaultProximalInitialPermanence(0.40f, 0.5f);

  vector<UInt> distalInputDimensions       = {0u};
  UInt         distalMaxSegments           = 255;
  UInt         distalMaxSynapsesPerSegment = 255;
  UInt         distalSegmentThreshold      = 13;
  UInt         distalSegmentMatch          = 10;
  UInt         distalAddSynapses           = 20;
  Permanence   distalInitialPermanence     = 0.21;
  Permanence   distalIncrement             = 0.10;
  Permanence   distalDecrement             = 0.10;
  Permanence   distalMispredictDecrement   = 0.0f;
  Permanence   distalSynapseThreshold      = 0.50;

  Real stabilityRate;
  Real fatigueRate;

  UInt period;
  Int  seed    = 0;
  bool verbose = true;
};


// TODO:
// const extern Parameters SpatialPoolerParameters = {};


class ColumnPooler // : public Serializable
{
private:
  Parameters args_;
  vector<UInt> cellDimensions_;

  vector<UInt16> rawOverlaps_;
  vector<Real> cellOverlaps_;
  vector<UInt> proximalMaxSegment_;

  Random rng_;
  vector<Real> tieBreaker_;
  UInt iterationNum_;
  UInt iterationLearnNum_;
  vector<Real> X_act;
  vector<Real> X_inact;

  // This is used by boosting.
  ActivationFrequency *AF_;

  SDR predictiveCells_;

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

  ColumnPooler() {}; // Default constructor, call initialize to setup properly

  ColumnPooler( const Parameters &parameters )
    { initialize( parameters ); }

  void initialize( const Parameters &parameters ) {
    // TODO: Move all of the parameter checks into setParameters
    NTA_CHECK( parameters.proximalSegments > 0u );
    NTA_CHECK( parameters.cellsPerInhibitionArea > 0u );

    NTA_CHECK( parameters.minSparsity * parameters.cellsPerInhibitionArea > 0.5f )
      << "Not enough cellsPerInhibitionArea ("
      << args_.cellsPerInhibitionArea << ") for desired density (" << args_.minSparsity << ").";
    // TODO: Check that minSparsity <= maxDepolarizedSparsity & maxBurstSparsity

    args_ = parameters;
    SDR proximalInputs(  args_.proximalInputDimensions );
    SDR inhibitionAreas( args_.inhibitionDimensions );
    cellDimensions_ = inhibitionAreas.dimensions;
    cellDimensions_.push_back( args_.cellsPerInhibitionArea );
    SDR cells( cellDimensions_ );
    rng_ = Random(args_.seed);

    // Setup Proximal segments & synapses.
    proximalConnections.initialize(cells.size, args_.proximalSynapseThreshold, true);
    Sparsity            PP_Sp( proximalInputs.dimensions, cells.size * args_.proximalSegments * 2);
    ActivationFrequency PP_AF( proximalInputs.dimensions, cells.size * args_.proximalSegments * 2);
    UInt cell = 0u;
    for(auto inhib = 0u; inhib < inhibitionAreas.size; ++inhib)
    {
      inhibitionAreas.setSparse(SDR_sparse_t{ inhib });
      for(auto c = 0u; c < args_.cellsPerInhibitionArea; ++c, ++cell)
      {
        cells.setSparse(SDR_sparse_t{ cell });
        for(auto s = 0u; s < args_.proximalSegments; ++s)
        {
          auto segment = proximalConnections.createSegment( cell );

          // Find the pool of potential inputs to this proximal segment.
          SDR pp = args_.potentialPool( inhibitionAreas, args_.proximalInputDimensions, rng_ );
          PP_Sp.addData( pp );
          PP_AF.addData( pp );
          for(const auto presyn : pp.getSparse() )
          {
            // Find an initial permanence for this synapse.
            proximalInputs.setSparse(SDR_sparse_t{ presyn });
            auto permanence = args_.proximalInitialPermanence(rng_, proximalInputs, cells);

            // Make the synapses.
            proximalConnections.createSynapse( segment, presyn, permanence);
          }
          proximalConnections.raisePermanencesToThreshold( segment,
                                              args_.proximalSegmentThreshold );
        }
      }
    }
    // Setup Proximal data structures.
    rawOverlaps_.resize( proximalConnections.numSegments() );
    cellOverlaps_.resize( cells.size );
    proximalMaxSegment_.resize( cells.size );
    tieBreaker_.resize( proximalConnections.numSegments() );
    for(auto i = 0u; i < tieBreaker_.size(); ++i) {
      tieBreaker_[i] = 0.01f * rng_.getReal64();
    }
    AF_ = new ActivationFrequency( {cells.size, args_.proximalSegments},
                        args_.period, args_.minSparsity / args_.proximalSegments );

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

    predictiveCells_.initialize( cellDimensions );

    iterationNum_      = 0u;
    iterationLearnNum_ = 0u;

    reset();

    if( PP_Sp.min() * proximalInputs.size < args_.proximalSegmentThreshold ) {
      NTA_WARN << "WARNING: Proximal segment has fewer synapses than the segment threshold." << endl;
    }
    NTA_CHECK( PP_Sp.min() > 0.0f );
    if( PP_AF.min() == 0.0f ) {
      NTA_WARN << "WARNING: Proximal input is unused." << endl;
    }

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
    proximalConnections.reset();
    distalConnections.reset();
  }


  void compute(
        const SDR& proximalInputActive,
        bool learn,
        SDR& active) {
    SDR none( args_.distalInputDimensions );
    SDR winner( cellDimensions );
    compute(proximalInputActive, proximalInputActive, none, none, learn, active, winner );
  }

  void compute(
        const SDR& proximalInputActive,
        const SDR& proximalInputLearning,
        const SDR& distalInputActive,
        const SDR& distalInputLearning,
        bool learn,
        SDR& active,
        SDR& winner) {
    NTA_CHECK( proximalInputActive.dimensions   == args_.proximalInputDimensions );
    NTA_CHECK( proximalInputLearning.dimensions == args_.proximalInputDimensions );
    NTA_CHECK( distalInputActive.dimensions     == args_.distalInputDimensions );
    NTA_CHECK( distalInputLearning.dimensions   == args_.distalInputDimensions );
    NTA_CHECK( active.dimensions                == cellDimensions );
    NTA_CHECK( winner.dimensions                == cellDimensions );

    // Update bookkeeping
    iterationNum_++;
    if( learn )
      iterationLearnNum_++;

    // Feed Forward Input / Proximal Dendrites
    activateProximalDendrites( proximalInputActive );

    distalConnections.activateDendrites(learn, distalInputActive, distalInputLearning);
    distalConnections.getPredictiveCells( predictiveCells_ );

    activateCells( active, winner );

    // Learn
    std::sort( active.getSparse().begin(), active.getSparse().end() );
    distalConnections.activateCells( active, learn );
    if( learn ) {
      learnProximalDendrites( proximalInputActive, active );
    }
  }


  void activateProximalDendrites( const SDR &feedForwardInputs )
  {
    // Proximal Feed Forward Excitement
    fill( rawOverlaps_.begin(), rawOverlaps_.end(), 0.0f );
    proximalConnections.computeActivity(rawOverlaps_, feedForwardInputs.getSparse());

    // Setup for Boosting
    const Real denominator = 1.0f / log2( args_.minSparsity / args_.proximalSegments );
    const auto &af = AF_->activationFrequency;

    // Process Each Segment of Each Cell
    for(auto cell = 0u; cell < proximalConnections.numCells(); ++cell) {
      Real maxOverlap    = -1.0;
      UInt maxSegment    = -1;
      // TODO: THIS FOR LOOP SHOULD USE INDEX MATH!
      for(const auto &segment : proximalConnections.segmentsForCell( cell ) ) {
        const auto raw = rawOverlaps_[segment];

        Real overlap = (Real) raw; // Typecast to floating point.

        // Proximal Tie Breaker
        // NOTE: Apply tiebreakers before boosting, so that boosting is applied
        //   to the tiebreakers.  This is important so that the tiebreakers don't
        //   hurt the entropy of the result by biasing some mini-columns to
        //   activte more often than others.
        overlap += tieBreaker_[segment];

        // Normalize Proximal Excitement by the number of connected synapses.
        const auto nConSyns = proximalConnections.dataForSegment( segment ).numConnected;
        if( nConSyns == 0 )
          // overlap = 1.0f;
          overlap = 0.0f;
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
      proximalMaxSegment_[cell] = maxSegment;

      // Apply Stability & Fatigue
      X_act[cell]   += (1.0f - args_.stabilityRate) * (maxOverlap - X_act[cell] - X_inact[cell]);
      X_inact[cell] += args_.fatigueRate * (maxOverlap - X_inact[cell]);
      cellOverlaps_[cell] = X_act[cell];
    }
  }


  void applyProximalSegmentThreshold( vector<UInt> &cells, UInt threshold )
  {
    for( Int idx = cells.size() - 1; idx >= 0; --idx )
    {
      const auto maxSeg  = proximalMaxSegment_[ cells[idx] ];
      const auto segOvlp = rawOverlaps_[ maxSeg ];
      if( segOvlp < threshold )
      {
        cells[idx] = cells.back();
        cells.pop_back();
      }
    }
  }


  void learnProximalDendrites( const SDR &proximalInputActive,
                               const SDR &active ) {
    SDR AF_SDR( AF_->dimensions );
    auto &activeSegments = AF_SDR.getSparse();
    for(const auto &cell : active.getSparse())
    {
      const auto &maxSegment = proximalMaxSegment_[cell];
      proximalConnections.adaptSegment(maxSegment, proximalInputActive,
                                       args_.proximalIncrement, args_.proximalDecrement);

      proximalConnections.raisePermanencesToThreshold(maxSegment,
                                              args_.proximalSegmentThreshold);

      activeSegments.push_back( maxSegment );
    }
    AF_SDR.setSparse( activeSegments );
    AF_->addData( AF_SDR );
  }


  void activateDistalDendrites() {}
  void learnDistalDendrites() {}


  void activateCells(SDR &activeCells, SDR &winnerCells)
  {
    auto &activeVec = activeCells.getSparse();
    auto &winnerVec = winnerCells.getSparse();
    activeVec.clear();
    winnerVec.clear();

    for(UInt offset = 0u; offset < activeCells.size; offset += args_.cellsPerInhibitionArea) {
      activateInhibitionArea(
            offset, offset + args_.cellsPerInhibitionArea,
            activeVec, winnerVec);
    }
    activeCells.setSparse( activeVec );
    winnerCells.setSparse( winnerVec );
  }


  void activateInhibitionArea(
            const UInt    areaStart,
            const UInt    areaEnd,
            vector<UInt> &active,
            vector<UInt> &winner)
  {
    // Inhibition areas are contiguous blocks of cells, find the size of it.
    const auto areaSize = areaEnd - areaStart;
    // Compare the cell indexes by their overlap.
    auto compare = [&](const UInt &a, const UInt &b) -> bool
                    { return cellOverlaps_[a] > cellOverlaps_[b]; };

    // PHASE ONE: Predicted / depolarized cells compete to activate.

    const UInt phase1numDesired = round( args_.maxDepolarizedSparsity * areaSize );
    if( predictiveCells_.getSum() > 0u ) {
      // Select the predicted cells.  Store them in-place in vector "phase1Active"
      SDR_sparse_t phase1Active;
      const auto &predictiveCellsDense = predictiveCells_.getDense();
      for( auto idx = areaStart; idx < areaEnd; ++idx ) {
        if( predictiveCellsDense[idx] ) {
          phase1Active.push_back( idx );
        }
      }
      if( phase1Active.size() > phase1numDesired ) {
        // Do a partial sort to divide the winners from the losers.  This sort is
        // faster than a regular sort because it stops after it partitions the
        // elements about the Nth element, with all elements on their correct side of
        // the Nth element.
        std::nth_element(
          phase1Active.begin(),
          phase1Active.begin() + phase1numDesired,
          phase1Active.end(),
          compare);
        // Remove cells which lost the competition.
        phase1Active.resize( phase1numDesired );
      }
      // Apply proximal segments activation threshold.
      applyProximalSegmentThreshold( phase1Active, args_.proximalSegmentThreshold );
      // Sort the phase 1 active cell indexes for fast access (needed later).
      std::sort( phase1Active.begin(), phase1Active.end() );
      // Output the active & winner cells from Phase 1.
      // All predicted active cells are winner cells.
      for( const auto cell : phase1Active ) {
        active.push_back( cell );
        winner.push_back( cell );
      }
      // TODO: Consider calling the learning methods for phase 1 right here.
    }

    // PHASE TWO: All remaining inactive cells compete to activate.

    const UInt minActive = round( args_.minSparsity * areaSize );
    UInt phase2NumDesired;
    UInt phase2NumWinners;
    if( phase1Active.size() >= minActive ) {
      // Predictive Regime
      phase2NumDesired = 0u;
      phase2NumWinners = 0u;
    }
    else {
      // Burst Regime
      const Real percentBurst = (Real) (minActive - phase1Active.size()) / minActive;
      const UInt slope = round(args_.maxBurstSparsity * areaSize) - minActive;
      const UInt totalActive = (percentBurst * slope) + minActive;
      phase2NumDesired = totalActive - phase1Active.size();

      phase2NumWinners = minActive - phase1Active.size();
    }

    // Select all cells which are still inactive.  Store them in-place in vector "phase2Active"
    SDR_sparse_t phase2Active;
    phase2Active.reserve( areaSize - phase1Active.size() );
    phase1Active.push_back( std::numeric_limits<UInt>::max() ); // Append a sentinel
    auto phase1ActiveIter = phase1Active.begin();
    for( auto idx = areaStart; idx < areaEnd; ++idx ) {
      if( idx == *phase1ActiveIter ) { // This works because phase1Active is sorted.
        ++phase1ActiveIter;
      }
      else {
        phase2Active.push_back( idx );
      }
    }
    // Do a partial sort to divide the winners from the losers.
    std::nth_element(
      phase2Active.begin(),
      phase2Active.begin() + phase2NumDesired,
      phase2Active.end(),
      compare);
    // Remove cells which lost the competition.
    phase2Active.resize( phase2NumDesired );
    // Apply activation threshold to proximal segments.
    // EXPERIMENT!
    // applyProximalSegmentThreshold( phase2Active, args_.proximalSegmentThreshold );
    // Output the active cells from Phase 2.
    for( const auto idx : phase2Active ) {
      active.push_back( idx );
    }

    // Promote some of the phase 2 active cells to winners.
    if( phase2Active.size() > phase2NumWinners ) {
      // Select the cells with the fewest distal segments.
      vector<UInt> phase2numSegments; phase2numSegments.reserve( phase2Active.size() );
      for( const auto cell : phase2Active ) {
        phase2numSegments.push_back( distalConnections.connections.numSegments( cell ));
      }
      auto least_used = [&phase2numSegments](const UInt &A, const UInt &B) -> bool
                      { return phase2numSegments[A] < phase2numSegments[B]; };

      std::nth_element( phase2Active.begin(),
                        phase2Active.begin() + phase2NumWinners,
                        phase2Active.end(),
                        least_used);

      for( UInt i = 0; i < phase2NumWinners; ++i ) {
        winner.push_back( phase2Active[i] );
      }
    }
    else {
      // All phase 2 active cells are winners.
      for( const auto cell : phase2Active ) {
        winner.push_back( cell );
      }
    }

    // TODO: Consider calling the learning methods for phase 2 right here.
  }


  void setParameters( Parameters &newParameters, bool init=false ) // TODO Hide the init param
  {
    args_ = newParameters;

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
    // if( newParameters.sparsity                    != parameters.sparsity ) {
    //   NTA_THROW << "Setter unimplemented.";
    // }
    // if( newParameters.potentialPool               != parameters.potentialPool ) {
    //   NTA_THROW << "Setter unimplemented.";
    // }
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
    if( newParameters.stabilityRate              != parameters.stabilityRate ) {
      NTA_THROW << "Setter unimplemented.";
    }
    if( newParameters.fatigueRate                != parameters.fatigueRate ) {
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

  // Real anomaly() {
  //   return distalConnections.anomaly();
  // }
};

} // End namespace column_pooler
} // End namespace algorithmn
} // End namespace nupic
