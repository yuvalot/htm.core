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
#include <htm/types/Types.hpp>
#include <htm/types/Serializable.hpp>
#include <htm/types/Sdr.hpp>
#include <htm/utils/SdrMetrics.hpp>
#include <htm/algorithms/Connections.hpp>
#include <htm/algorithms/TemporalMemory.hpp>
#include <htm/utils/Topology.hpp>

namespace htm {

using namespace std;
using namespace htm;

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

  UInt cellsPerInhibitionArea = 2048;
  Real sparsity = 0.02f;

  Topology_t  potentialPool     = NoTopology(1.0f);
  UInt        proximalSegments  = 1u;
  Permanence  proximalIncrement = 0.01f;
  Permanence  proximalDecrement = 0.002f;
  Permanence  proximalSynapseThreshold = 0.40f;
  UInt        proximalSegmentThreshold = 6u;
  InitialPermanence_t proximalInitialPermanence =
                                  defaultProximalInitialPermanence(0.40f, 0.5f);
  Real        proximalMinConnections = 0.05f;
  Real        proximalMaxConnections = 0.30f;

  vector<UInt> distalInputDimensions       = {0u};
  UInt         distalMaxSegments           = 128;
  UInt         distalMaxSynapsesPerSegment = 64;
  UInt         distalAddSynapses           = 20;
  UInt         distalSegmentThreshold      = 14;
  UInt         distalSegmentMatch          = 9;
  Permanence   distalSynapseThreshold      = 0.50;
  Permanence   distalInitialPermanence     = 0.41;
  Permanence   distalIncrement             = 0.10;
  Permanence   distalDecrement             = 0.001;
  Permanence   distalMispredictDecrement   = 0.0;

  Real stabilityRate = 0.0f;;
  Real fatigueRate   = 0.0f;;

  UInt period;
  Int  seed    = 0;
  bool verbose = true;
};


class ColumnPooler // : public Serializable
{
private:
  Parameters args_;
  vector<UInt> cellDimensions_;
  UInt size_;

  // Proximal Dendrite Data:
  // TODO: Move distalConnections here, expose public references to all of this junk!
  vector<UInt16> rawOverlaps_;
  vector<Real>   proximalOverlaps_;
  vector<UInt>   proximalMaxSegment_;
  ActivationFrequency *AF_;   // This is used by boosting.
  vector<Real> X_act;
  vector<Real> X_inact;
  vector<Real> tieBreaker_;

  // Distal Dendrite Data:
  // TODO: Move distalConnections here, expose public references to all of this junk!
  vector<SynapseIdx> numActiveConnectedSynapsesForSegment_;
  vector<SynapseIdx> numActivePotentialSynapsesForSegment_;
  vector<SegmentIdx> activeSegments_;
  vector<UInt> lastUsedIterationForSegment_;

  SDR activeCells_;

  Real rawAnomaly_;     // TODO Unimplemented
  Real meanAnomaly_;    // TODO Unimplemented
  Real varAnomaly_;     // TODO Unimplemented

  UInt iterationNum_;
  Random rng_;

public:
  const Parameters   & parameters     = args_;
  const vector<UInt> & cellDimensions = cellDimensions_;
  const vector<UInt> & dimensions     = cellDimensions;
  const UInt         & size           = size_;
  const SDR          & activeCells    = activeCells_;

  const Real & rawAnomaly = rawAnomaly_;

  const UInt &iterationNum      = iterationNum_;

  /**
   * The proximal connections have regular structure.  Cells in an inhibition
   * area are contiguous and all segments on a cell are contiguous.  This
   * allows fast index math instead of slowers lists of ID's.
   */
  Connections proximalConnections;

  Connections distalConnections;

  ColumnPooler() {}; // Default constructor, call initialize to setup properly.

  ColumnPooler( const Parameters &parameters )
    { initialize( parameters ); }

  void initialize( const Parameters &parameters ) {
    // TODO: Move all of the parameter checks into setParameters
    NTA_CHECK( parameters.proximalSegments > 0u );
    NTA_CHECK( parameters.cellsPerInhibitionArea > 0u );

    #define RANGE_0_to_1(P) ( P >= 0.0f && P <= 1.0 )
    NTA_CHECK( RANGE_0_to_1( parameters.sparsity ));
    NTA_CHECK( RANGE_0_to_1( parameters.proximalIncrement ));
    NTA_CHECK( RANGE_0_to_1( parameters.proximalDecrement ));
    NTA_CHECK( RANGE_0_to_1( parameters.proximalSynapseThreshold ));
    NTA_CHECK( RANGE_0_to_1( parameters.proximalMinConnections ));
    NTA_CHECK( RANGE_0_to_1( parameters.proximalMaxConnections ));
    NTA_CHECK( RANGE_0_to_1( parameters.distalInitialPermanence ));
    NTA_CHECK( RANGE_0_to_1( parameters.distalSynapseThreshold ));
    NTA_CHECK( RANGE_0_to_1( parameters.distalIncrement ));
    NTA_CHECK( RANGE_0_to_1( parameters.distalDecrement ));
    NTA_CHECK( RANGE_0_to_1( parameters.distalMispredictDecrement ));
    NTA_CHECK( RANGE_0_to_1( parameters.stabilityRate ));
    NTA_CHECK( RANGE_0_to_1( parameters.fatigueRate ));
    #undef RANGE_0_to_1

    NTA_CHECK( parameters.sparsity * parameters.cellsPerInhibitionArea > 0.5f )
      << "cellsPerInhibitionArea (" << args_.cellsPerInhibitionArea
      << ") is too small for desired density (" << args_.sparsity << ").";

    NTA_CHECK( SDR(parameters.inhibitionDimensions).size > 0 )
        << "Must have at least one inhibition area.";

    NTA_CHECK( parameters.distalSegmentMatch <= parameters.distalSegmentThreshold );

    args_ = parameters;
    SDR proximalInputs(  args_.proximalInputDimensions );
    SDR inhibitionAreas( args_.inhibitionDimensions );
    cellDimensions_ = inhibitionAreas.dimensions;
    cellDimensions_.push_back( args_.cellsPerInhibitionArea );
    SDR cells( cellDimensions_ );
    activeCells_.initialize( cells.dimensions );
    size_ = cells.size;
    rng_ = Random(args_.seed);

    // Setup Proximal Segments & Synapses.
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
            rng_(); // Pybind copies this, so force it to a new state.

            // Make the synapses.
            proximalConnections.createSynapse( segment, presyn, permanence);
          }
          const auto numPotentialSyn = proximalConnections.dataForSegment( segment ).synapses.size();
          // proximalConnections.synapseCompetition(segment,
          //           (SynapseIdx) (args_.proximalMinConnections * numPotentialSyn),
          //           (SynapseIdx) (args_.proximalMaxConnections * numPotentialSyn));
        }
      }
    }
    if( PP_Sp.min() * proximalInputs.size < args_.proximalSegmentThreshold ) {
      NTA_WARN << "WARNING: Proximal segment has fewer synapses than the segment threshold." << endl;
    }
    NTA_CHECK( PP_Sp.min() > 0.0f );
    if( PP_AF.min() == 0.0f ) {
      NTA_WARN << "WARNING: Proximal input is unused." << endl;
    }
    // Setup Proximal data structures.
    rawOverlaps_.resize( proximalConnections.numSegments() );
    proximalOverlaps_.resize( cells.size );
    proximalMaxSegment_.resize( cells.size );
    tieBreaker_.resize( proximalConnections.numSegments() );
    for(auto i = 0u; i < tieBreaker_.size(); ++i) {
      tieBreaker_[i] = 0.01f * rng_.getReal64();
    }
    AF_ = new ActivationFrequency( {cells.size, args_.proximalSegments},
                        args_.period, args_.sparsity / args_.proximalSegments );

    // Setup Distal dendrites.
    distalConnections.initialize(cells.size, args_.distalSynapseThreshold, true);
    lastUsedIterationForSegment_.clear();
    if( args_.distalInputDimensions == vector<UInt>{0} ) {
      args_.distalInputDimensions = cellDimensions;
    }
    activeSegments_.resize(cells.size);

    iterationNum_ = 0u;

    reset();

    if( args_.verbose ) {
      // TODO: Print all parameters, make method to print Parameters struct.
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
    activeSegments_.clear();
    rawAnomaly_ = -1.0f;
    activeCells_.zero();
    // TODO: Clear misc. internal data.  If it is publicly visible is should be zero'd!
  }


  void compute( const SDR& proximalInputActive, bool learn ) {
    SDR previousActiveCells(activeCells_);
    compute( proximalInputActive, previousActiveCells, learn);
  }

  void compute(
        const SDR& proximalInputActive,
        const SDR& distalInputActive,
        const bool learn)
  {
    NTA_CHECK( proximalInputActive.dimensions == args_.proximalInputDimensions );
    NTA_CHECK( distalInputActive.dimensions   == args_.distalInputDimensions );
    iterationNum_++;

    // Compute dendrites
    computeProximalDendrites( proximalInputActive );
    computeDistalDendrites( distalInputActive );

    // Compute cell states
    auto & cells = activeCells_.getSparse();
    cells.clear();
    for(UInt offset = 0u; offset < activeCells_.size; offset += args_.cellsPerInhibitionArea) {
        const auto active = computeInhibitionArea(offset, offset + args_.cellsPerInhibitionArea);
        for( const auto x : active ) {
          cells.push_back( x );
        }
    }
    sort( cells.begin(), cells.end() );
    activeCells_.setSparse( cells );

    rawAnomaly_ = 0.0f;
    for( const auto cell : activeCells_.getSparse() ) {
      if( activeSegments_[cell] == 0 ) {
        rawAnomaly_++;
      }
    }
    rawAnomaly_ /= activeCells_.getSum();

    if( learn ) {
      learnProximalDendrites( proximalInputActive, activeCells );
      // Don't learn if the distal dendrites are disabled, will crash.
      if( args_.distalMaxSegments           > 0u &&
          args_.distalMaxSynapsesPerSegment > 0u &&
          args_.distalAddSynapses           > 0u )
      {
        learnDistalDendrites( distalInputActive );
      }
    }
  }


  void computeProximalDendrites( const SDR &feedForwardInputs )
  {
    // Proximal Feed Forward Excitement
    fill( rawOverlaps_.begin(), rawOverlaps_.end(), 0.0f );
    proximalConnections.computeActivity(rawOverlaps_, feedForwardInputs.getSparse());

    // Setup for Boosting
    const Real denominator = 1.0f / log2( args_.sparsity / args_.proximalSegments );
    const auto &af = AF_->activationFrequency;

    // Process Each Segment of Each Cell
    for(auto cell = 0u; cell < proximalConnections.numCells(); ++cell) {
      Real maxOverlap    = -1.0;
      UInt maxSegment    = -1;
      // TODO: THIS FOR LOOP SHOULD USE INDEX MATH!
      for(const auto &segment : proximalConnections.segmentsForCell( cell ) ) {
        Real overlap = (Real) rawOverlaps_[segment];

        // Proximal Tie Breaker
        // NOTE: Apply tiebreakers before boosting, so that boosting is applied
        //   to the tiebreakers.  This is important so that the tiebreakers don't
        //   hurt the entropy of the result by biasing some mini-columns to
        //   activte more often than others.
        overlap += tieBreaker_[segment];

        // Logarithmic Boosting Function
        overlap *= log2( af[segment] ) * denominator;

        // Exponential Boosting Function
        // overlap *= exp((args_.sparsity - af[segment]) * 25.0);

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
      proximalOverlaps_[cell] = X_act[cell];
    }
  }


  void learnProximalDendrites( const SDR &proximalInputActive,
                               const SDR &active )
  {
    SDR AF_SDR( AF_->dimensions );
    auto &activeSegments = AF_SDR.getSparse();
    for( const auto cell : active.getSparse() )
    {
      const auto & maxSegment = proximalMaxSegment_[ cell ];
      proximalConnections.adaptSegment(
          maxSegment, proximalInputActive,
          args_.proximalIncrement, args_.proximalDecrement);

      // const auto numPotentialSyn = proximalConnections.dataForSegment( maxSegment ).synapses.size();
      // proximalConnections.synapseCompetition(maxSegment,
      //           (SynapseIdx) (args_.proximalMinConnections * numPotentialSyn),
      //           (SynapseIdx) (args_.proximalMaxConnections * numPotentialSyn));

      activeSegments.push_back( maxSegment );
    }
    AF_SDR.setSparse( activeSegments );
    AF_->addData( AF_SDR );
  }


  void computeDistalDendrites( const SDR& distalInputActive )
  {
    const size_t length = distalConnections.segmentFlatListLength();
    numActiveConnectedSynapsesForSegment_.assign( length, 0u );
    numActivePotentialSynapsesForSegment_.assign( length, 0u );
    distalConnections.computeActivity(numActiveConnectedSynapsesForSegment_,
                                      numActivePotentialSynapsesForSegment_,
                                      distalInputActive.getSparse() );

    // Count the activate segments per cell.
    activeSegments_.assign( size, 0.0f );
    for( CellIdx c = 0; c < size; c++) {
      const auto & segments = distalConnections.segmentsForCell( c );
      for( Segment s : segments ) {
        if( numActiveConnectedSynapsesForSegment_[s] >= args_.distalSegmentThreshold ) {
          activeSegments_[c]++;
        }
      }
    }
  }


  Segment createDistalSegment( const CellIdx cell ) {
    while( distalConnections.numSegments( cell ) >= args_.distalMaxSegments ) {
      const vector<Segment> &destroyCandidates =
                                      distalConnections.segmentsForCell( cell );

      auto leastRecentlyUsedSegment =
          std::min_element(destroyCandidates.begin(), destroyCandidates.end(),
                           [&](const Segment a, const Segment b) {
                             return (lastUsedIterationForSegment_[a] <
                                     lastUsedIterationForSegment_[b]);
                           });

      distalConnections.destroySegment(*leastRecentlyUsedSegment);
    }

    const Segment segment = distalConnections.createSegment(cell);
    const auto length = distalConnections.segmentFlatListLength();
    numActiveConnectedSynapsesForSegment_.resize( length );
    numActivePotentialSynapsesForSegment_.resize( length );
    lastUsedIterationForSegment_.resize(length);
    numActiveConnectedSynapsesForSegment_[ segment ] = 0u;
    numActivePotentialSynapsesForSegment_[ segment ] = 0u;
    lastUsedIterationForSegment_[ segment ]          = iterationNum;
    return segment;
  }


  void growSynapses(const Segment segment,
                    const SynapseIdx nDesiredNewSynapses,
                    const Permanence initialPermanence,
                    const SynapseIdx maxSynapsesPerSegment,
                    const SDR &distalInputWinnerPrevious)
  {
    auto &prevWinnerCells = distalInputWinnerPrevious.getSparse();
    std::sort(prevWinnerCells.begin(), prevWinnerCells.end());
    SDR_sparse_t candidates( prevWinnerCells.begin(), prevWinnerCells.end() ); // copy
    NTA_ASSERT(std::is_sorted(candidates.begin(), candidates.end()));

    // Remove cells that are already synapsed on by this segment
    for (const Synapse& synapse : distalConnections.synapsesForSegment(segment)) {
      const CellIdx presynapticCell = distalConnections.dataForSynapse(synapse).presynapticCell;
      const auto already = std::lower_bound(candidates.cbegin(), candidates.cend(), presynapticCell);
      if (already != candidates.cend() && *already == presynapticCell) {
        candidates.erase(already);
      }
    }

    const int nActual = std::min((int)(nDesiredNewSynapses), (int)candidates.size());

    // Check if we're going to surpass the maximum number of synapses. //TODO delegate this to createSynapse(segment)
    const int overrun = (distalConnections.numSynapses(segment) + nActual - maxSynapsesPerSegment);
    if (overrun > 0) {
      distalConnections.destroyMinPermanenceSynapses(segment, overrun, distalInputWinnerPrevious.getSparse());
    }

    // Recalculate in case we weren't able to destroy as many synapses as needed.
    const int nActualWithMax = std::min((int)nActual,
        (int)((maxSynapsesPerSegment) - distalConnections.numSynapses(segment)));

    // Pick nActual cells randomly.

    // It's possible to optimize this, swapping candidates to the end as
    // they're used. But this is awkward to mimic in other
    // implementations, especially because it requires iterating over
    // the existing synapses in a particular order.
    //
    // OR: Use random.sample....
    for (int c = 0; c < nActualWithMax; c++) {
      const auto i = rng_.getUInt32((UInt32)(candidates.size()));
      distalConnections.createSynapse(segment, candidates[i], initialPermanence);
      candidates.erase(candidates.begin() + i);
    }
  }


  void learnDistalSegment( const Segment segment,
                           const SDR & distalInputActivePrevious)
  {
    distalConnections.adaptSegment( segment, distalInputActivePrevious,
                                    args_.distalIncrement, args_.distalDecrement);

    const Int nGrowDesired = args_.distalAddSynapses -
                              numActivePotentialSynapsesForSegment_[ segment ];
    if( nGrowDesired > 0 ) {
      growSynapses( segment, nGrowDesired,
                    args_.distalInitialPermanence,
                    args_.distalMaxSynapsesPerSegment,
                    distalInputActivePrevious);
    }
  }


  void learnDistalDendrites( const SDR & distalInputActivePrevious )
  {
    const auto & dense = activeCells_.getDense();
    for( CellIdx cell = 0; cell < size; cell++ ) {
      if( dense[cell] ) {
        // Active cells learn.
        UInt nr_segs_learned = 0;
        for( const auto segment : distalConnections.segmentsForCell( cell )) {
          if( numActivePotentialSynapsesForSegment_[segment] >= args_.distalSegmentMatch ) {
            learnDistalSegment( segment, distalInputActivePrevious );
            nr_segs_learned++;
          }
        }
        if( nr_segs_learned == 0 ) {
          auto segment = createDistalSegment( cell );
          learnDistalSegment( segment, distalInputActivePrevious );
        }
      }
      else {
        // Inactive cells punish mispredictions.
        for( const auto segment : distalConnections.segmentsForCell( cell )) {
          if( numActivePotentialSynapsesForSegment_[segment] >= args_.distalSegmentMatch ) {
            distalConnections.adaptSegment( segment, distalInputActivePrevious,
                                      -args_.distalMispredictDecrement, 0.0f);
          }
        }
      }
    }
  }


  SDR_sparse_t computeInhibitionArea(
      const UInt areaStart,
      const UInt areaEnd)
  {
    // Inhibition areas are contiguous blocks of cells, find the size of it.
    const auto areaSize = areaEnd - areaStart;
    const UInt targetActiveCells = round( areaSize * args_.sparsity );

    // Competition for most distal inputs.  In this phase cells need both feed
    // forward / proximal and predictive / distal support.
    SDR_sparse_t activeCells;  activeCells.reserve( areaSize );
    for( auto cell = areaStart; cell < areaEnd; ++cell ) {
      if( proximalOverlaps_[cell] > args_.proximalSegmentThreshold ) {
        if( activeSegments_[cell] > 0 ) {
          activeCells.push_back( cell );
        }
      }
    }
    if( activeCells.size() > targetActiveCells ) {
      // Compare the cell indexes by their distal inputs, and use the proximal
      // input as a tiebreaker.
      auto compare1 = [&](const UInt &a, const UInt &b) -> bool
      {
        const auto distal_a = activeSegments_[a];
        const auto distal_b = activeSegments_[b];
        if( distal_a == distal_b ) {
          return proximalOverlaps_[a] > proximalOverlaps_[b];
        }
        else {
          return distal_a > distal_b;
        }
      };
      // Do a partial sort to divide the winners from the losers.  This sort is
      // faster than a regular sort because it stops after it partitions the
      // elements about the Nth element, with all elements on their correct side
      // of the Nth element.
      std::nth_element( activeCells.begin(),
                        activeCells.begin() + targetActiveCells,
                        activeCells.end(),
                        compare1);
      // Remove cells which lost the competition.
      activeCells.resize( targetActiveCells );
    }

    // If we have not yet reached the target sparsity, then activate cells with
    // feed forward support but no lateral support.
    if( activeCells.size() < targetActiveCells ) {
      const UInt proxCompActivate = targetActiveCells - activeCells.size();
      // First build a list of all cells which are not yet active.
      vector<CellIdx> proximalCompetition;
      activeCells.push_back(-1); // Append a sentinel.
      auto alreadyActive = activeCells.begin();
      for( auto cell = areaStart; cell < areaEnd; ++cell ) {
        if( cell == *alreadyActive ) {
          alreadyActive++;
        }
        else {
          proximalCompetition.push_back( cell );
        }
      }
      activeCells.pop_back(); // Remove the sentinel.

      auto compareProximalOverlap = [&](const UInt &a, const UInt &b) -> bool
          {return proximalOverlaps_[a] > proximalOverlaps_[b]; };
      std::nth_element( proximalCompetition.begin(),
                        proximalCompetition.begin() + proxCompActivate,
                        proximalCompetition.end(),
                        compareProximalOverlap);
      proximalCompetition.resize( proxCompActivate );

      for(const auto cell : proximalCompetition) {
        activeCells.push_back( cell );
      }
    }

    return activeCells;
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
};

} // End namespace htm
