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

namespace nupic {

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
  UInt         cellsPerInhibitionArea;

  Real minSparsity            = 0.02f;
  Real maxBurstSparsity       = 0.10f; // TODO: Reformulate this as cellsPerMiniColumn * minSparsity
  Real maxDepolarizedSparsity = 0.05f; // TODO: Reformulate this as maxPredictionsPerMiniColumn * minSparsity

  Topology_t  potentialPool     = NoTopology(1.0f);
  UInt        proximalSegments  = 1u;
  Permanence  proximalIncrement = 0.01f;
  Permanence  proximalDecrement = 0.002f;
  Permanence  proximalSynapseThreshold = 0.40f;
  UInt        proximalSegmentThreshold = 1u;
  InitialPermanence_t proximalInitialPermanence =
                                  defaultProximalInitialPermanence(0.40f, 0.5f);
  Real        proximalMinConnections = 0.05f;
  Real        proximalMaxConnections = 0.30f;

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


class ColumnPooler // : public Serializable
{
private:
  Parameters args_;
  vector<UInt> cellDimensions_;
  UInt size_;

  // Proximal Dendrite Data:
  // TODO: Move distalConnections here, expose public references to all of this junk!
  vector<UInt16> rawOverlaps_;
  vector<UInt16> potentialOverlaps_;
  vector<Real>   cellOverlaps_;
  vector<UInt>   proximalMaxSegment_;
  ActivationFrequency *AF_;   // This is used by boosting.
  vector<Real> X_act;
  vector<Real> X_inact;
  vector<Real> tieBreaker_;

  // Distal Dendrite Data:
  // TODO: Move distalConnections here, expose public references to all of this junk!
  vector<SynapseIdx> numActiveConnectedSynapsesForSegment_;
  vector<SynapseIdx> numActivePotentialSynapsesForSegment_;
  SDR predictiveCells_;
  vector<UInt> lastUsedIterationForSegment_;

  SDR activeCells_;
  SDR winnerCells_;

  Real rawAnomaly_;
  Real meanAnomaly_;    // TODO Unimplemented
  Real varAnomaly_;     // TODO Unimplemented

  UInt iterationNum_;
  UInt iterationLearnNum_;
  Random rng_;

public:
  const Parameters   &parameters     = args_;
  const vector<UInt> &cellDimensions = cellDimensions_;
  const vector<UInt> &dimensions     = cellDimensions;
  const UInt         &size           = size_;

  const SDR & activeCells     = activeCells_;
  const SDR & predictiveCells = predictiveCells_;
  const SDR & winnerCells     = winnerCells_;

  const Real & rawAnomaly = rawAnomaly_;

  const UInt &iterationNum      = iterationNum_;
  const UInt &iterationLearnNum = iterationLearnNum_;

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
    NTA_CHECK( RANGE_0_to_1( parameters.minSparsity ));
    NTA_CHECK( RANGE_0_to_1( parameters.maxBurstSparsity ));
    NTA_CHECK( RANGE_0_to_1( parameters.maxDepolarizedSparsity ));
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

    NTA_CHECK( parameters.minSparsity * parameters.cellsPerInhibitionArea > 0.5f )
      << "Not enough cellsPerInhibitionArea ("
      << args_.cellsPerInhibitionArea << ") for desired density (" << args_.minSparsity << ").";

    NTA_CHECK( SDR(parameters.inhibitionDimensions).size > 0 )
        << "Must have at least one inhibition area.";

    NTA_CHECK( parameters.minSparsity <= parameters.maxBurstSparsity );
    NTA_CHECK( parameters.minSparsity <= parameters.maxDepolarizedSparsity );
    NTA_CHECK( parameters.distalSegmentMatch <= parameters.distalSegmentThreshold );

    args_ = parameters;
    SDR proximalInputs(  args_.proximalInputDimensions );
    SDR inhibitionAreas( args_.inhibitionDimensions );
    cellDimensions_ = inhibitionAreas.dimensions;
    cellDimensions_.push_back( args_.cellsPerInhibitionArea );
    SDR cells( cellDimensions_ );
    activeCells_.initialize( cells.dimensions );
    winnerCells_.initialize( cells.dimensions );
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
    potentialOverlaps_.resize( proximalConnections.numSegments() );
    cellOverlaps_.resize( cells.size );
    proximalMaxSegment_.resize( cells.size );
    tieBreaker_.resize( proximalConnections.numSegments() );
    for(auto i = 0u; i < tieBreaker_.size(); ++i) {
      tieBreaker_[i] = 0.01f * rng_.getReal64();
    }
    AF_ = new ActivationFrequency( {cells.size, args_.proximalSegments},
                        args_.period, args_.minSparsity / args_.proximalSegments );

    // Setup Distal dendrites.
    distalConnections.initialize(cells.size, args_.distalSynapseThreshold, true);
    lastUsedIterationForSegment_.clear();
    predictiveCells_.initialize( cellDimensions );
    if( args_.distalInputDimensions == vector<UInt>{0} ) {
      args_.distalInputDimensions = cellDimensions;
    }

    iterationNum_      = 0u;
    iterationLearnNum_ = 0u;

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
    predictiveCells_.zero();
    rawAnomaly_ = -1.0f;
    activeCells_.zero();
    winnerCells_.zero();
    // TODO: Clear misc. internal data.  If it is publicly visible is should be zero'd!
  }


  void compute( const SDR& proximalInputActive, bool learn ) {
    SDR previousActiveCells(activeCells_);
    SDR previousWinnerCells(winnerCells_);
    compute( proximalInputActive, previousActiveCells, previousWinnerCells, learn);
  }

  void compute(
        const SDR& proximalInputActive,
        const SDR& distalInputActive,
        const SDR& distalInputWinner,
        const bool learn) {
    NTA_CHECK( proximalInputActive.dimensions == args_.proximalInputDimensions );
    NTA_CHECK( distalInputActive.dimensions   == args_.distalInputDimensions );
    NTA_CHECK( distalInputWinner.dimensions   == args_.distalInputDimensions );
    // Update bookkeeping
    iterationNum_++;
    if( learn )
      iterationLearnNum_++;

    // Feed Forward Input / Proximal Dendrites
    computeProximalDendrites( proximalInputActive );

    computeDistalDendrites( distalInputActive, learn );

    activeCells_.getSparse().clear();
    winnerCells_.getSparse().clear();
    // TODO: Parallelize this loop.
    for(UInt offset = 0u; offset < activeCells_.size; offset += args_.cellsPerInhibitionArea) {
      computeInhibitionArea(
            offset, offset + args_.cellsPerInhibitionArea, learn,
            distalInputActive, distalInputWinner,
            activeCells_.getSparse(), winnerCells_.getSparse());
    }
    activeCells_.setSparse( activeCells_.getSparse() );
    winnerCells_.setSparse( winnerCells_.getSparse() );

    if( learn ) {
      learnProximalDendrites( proximalInputActive, activeCells );
    }
  }


  void computeProximalDendrites( const SDR &feedForwardInputs )
  {
    // Proximal Feed Forward Excitement
    fill( rawOverlaps_.begin(), rawOverlaps_.end(), 0.0f );
    fill( potentialOverlaps_.begin(), potentialOverlaps_.end(), 0.0f );
    // proximalConnections.computeActivity(rawOverlaps_, potentialOverlaps_, feedForwardInputs.getSparse());
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

        // Logarithmic Boosting Function
        overlap *= log2( af[segment] ) * denominator;

        // Exponential Boosting Function
        // overlap *= exp((args_.minSparsity - af[segment]) * 25.0);

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

      const auto numPotentialSyn = proximalConnections.dataForSegment( maxSegment ).synapses.size();
      // proximalConnections.synapseCompetition(maxSegment,
      //           (SynapseIdx) (args_.proximalMinConnections * numPotentialSyn),
      //           (SynapseIdx) (args_.proximalMaxConnections * numPotentialSyn));

      activeSegments.push_back( maxSegment );
    }
    AF_SDR.setSparse( activeSegments );
    AF_->addData( AF_SDR );
  }


  void computeDistalDendrites( const SDR& distalInputActive, bool learn )
  {
    const size_t length = distalConnections.segmentFlatListLength();
    numActiveConnectedSynapsesForSegment_.assign( length, 0u );
    numActivePotentialSynapsesForSegment_.assign( length, 0u );
    distalConnections.computeActivity(numActiveConnectedSynapsesForSegment_,
                                      numActivePotentialSynapsesForSegment_,
                                      distalInputActive.getSparse() );

    // Activate segments, connected synapses.
    vector<Segment> activeSegments_;
    for( Segment segment = 0; segment < length; segment++ ) {
      if( numActiveConnectedSynapsesForSegment_[segment] >= args_.distalSegmentThreshold ) {
        activeSegments_.push_back(segment);
      }
    }
    std::sort(
        activeSegments_.begin(), activeSegments_.end(),
        [&](Segment a, Segment b) { return distalConnections.compareSegments(a, b); });

    // Distal dendrites make predictions.
    auto &predCellsVec = predictiveCells_.getSparse();
    predCellsVec.clear();
    for( auto segment = activeSegments_.cbegin(); segment != activeSegments_.cend(); segment++) {
      CellIdx cell = distalConnections.cellForSegment(*segment);
      if( segment == activeSegments_.begin() || cell != predCellsVec.back()) {
        predCellsVec.push_back(cell);
      }
    }
    predictiveCells_.setSparse( predCellsVec );

    // Update segment bookkeeping.
    if( learn ) {
      for( const auto segment : activeSegments_ ) {
        lastUsedIterationForSegment_[segment] = iterationNum_;
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
                    const SDR &distalInputWinnerPrevious) {

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
      distalConnections.destroyMinPermanenceSynapses(segment, overrun, distalInputWinnerPrevious);
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
                          const SDR & distalInputActivePrevious,
                          const SDR & distalInputWinnerPrevious) {
    distalConnections.adaptSegment( segment, distalInputActivePrevious,
                                    args_.distalIncrement, args_.distalDecrement);

    const Int nGrowDesired = args_.distalAddSynapses -
                              numActivePotentialSynapsesForSegment_[ segment ];
    if( nGrowDesired > 0 ) {
      growSynapses( segment, nGrowDesired,
                    args_.distalInitialPermanence,
                    args_.distalMaxSynapsesPerSegment,
                    distalInputWinnerPrevious);
    }
  }


  void learnDistalDendritesPredicted( const SDR_sparse_t &predictedActive,
                                      const SDR & distalInputActivePrevious,
                                      const SDR & distalInputWinnerPrevious,
                                            SDR_sparse_t &winner)
  {
    for( const auto cell : predictedActive ) {
      for( const auto segment : distalConnections.segmentsForCell( cell )) {
        if( numActiveConnectedSynapsesForSegment_[segment] >= args_.distalSegmentThreshold ) {
          learnDistalSegment( segment, distalInputActivePrevious, distalInputWinnerPrevious );
        }
      }
      winner.push_back( cell ); // All predicted active cells are winner cells.
    }
  }


  void learnDistalDendritesUnpredicted(
      const SDR    &distalInputActivePrevious,
      const SDR    &distalInputWinnerPrevious,
      SDR_sparse_t &unpredictedActive,
      UInt          numWinners,
      SDR_sparse_t &winner)
  {
    // Promote some of the unpredicted active cells to winners / learning.
    NTA_ASSERT( unpredictedActive.size() >= numWinners);

    // First find all matching segments.
    // MatchData is pair of (numActivePotentialSynapsesForSegment_[segment], segment)
    typedef pair<SynapseIdx, Segment> MatchData;
    vector<MatchData> matches;
    // Find best match for each cell.
    for( const auto cell : unpredictedActive ) {
      Segment bestSegment;
      int     bestMatch   = -1;
      for( const auto segment : distalConnections.segmentsForCell( cell )) {
        if( numActivePotentialSynapsesForSegment_[segment] >= args_.distalSegmentMatch ) {
          if( (int) numActivePotentialSynapsesForSegment_[segment] > bestMatch ) {
            bestMatch   = (int) numActivePotentialSynapsesForSegment_[segment];
            bestSegment = segment;
          }
        }
      }
      if( bestMatch != -1 ) {
        matches.emplace_back( (SynapseIdx) bestMatch, bestSegment );
      }
    }

    if( matches.size() > numWinners ) {
      // Too many matching segments, use only the best matches.
      const auto bestMatchingSegmentCompare =
          [&](const MatchData &A, const MatchData &B) -> bool
              { return A.first > B.first; };
      std::nth_element( matches.begin(),
                        matches.begin() + numWinners,
                        matches.end(),
                        bestMatchingSegmentCompare );
      matches.resize( numWinners );
    }

    for( const MatchData &match : matches ) {
      const auto cell = distalConnections.cellForSegment( match.second );
      winner.push_back( cell );
      learnDistalSegment( match.second, distalInputActivePrevious, distalInputWinnerPrevious );
    }
    // cerr << matches.size() << " / " << numWinners << endl;
    numWinners -= matches.size();

    if( numWinners > 0u ) {
      // Initializse new segments on the cells with the fewest distal segments.
      numWinners = min( numWinners, (UInt) unpredictedActive.size() );
      const auto least_used = [&](const UInt &A, const UInt &B) -> bool
          { return distalConnections.numSegments( A ) < distalConnections.numSegments( B ); };

      // TODO: Random tie breaker to this!
      // TODO: Exclude matching segments!
      std::nth_element( unpredictedActive.begin(),
                        unpredictedActive.begin() + numWinners,
                        unpredictedActive.end(),
                        least_used);

      for( auto cellIter = unpredictedActive.begin();
                cellIter != unpredictedActive.begin() + numWinners;
                ++cellIter )
      {
        winner.push_back( *cellIter );
        auto segment = createDistalSegment( *cellIter );
        learnDistalSegment( segment, distalInputActivePrevious, distalInputWinnerPrevious );
      }
    }
  }


  void learnDistalDendritesMispredicted(
            const UInt    areaStart,
            const UInt    areaEnd,
            SDR_sparse_t &predictedActive,
            SDR_sparse_t &unpredictedActive,
            const SDR    &distalInputActivePrevious)
  {
    if( args_.distalMispredictDecrement == 0.0f )
        { return; }
    std::sort( predictedActive.begin(),   predictedActive.end() );
    std::sort( unpredictedActive.begin(), unpredictedActive.end() );
    auto predictedActiveIter   = predictedActive.begin();
    auto unpredictedActiveIter = unpredictedActive.begin();
    // TODO: Sentinels!
    for( auto cell = areaStart; cell < areaEnd; ++cell ) {
      if( cell == *predictedActiveIter ) {
        predictedActiveIter++;
      }
      else if( cell == *unpredictedActiveIter ) {
        unpredictedActiveIter++;
      }
      else {
        // Matching segments, potential synapses.
        for( const auto segment : distalConnections.segmentsForCell( cell )) {
          if( numActivePotentialSynapsesForSegment_[segment] >= args_.distalSegmentMatch ) {
            distalConnections.adaptSegment( segment, distalInputActivePrevious,
                                      -args_.distalMispredictDecrement, 0.0f);
          }
        }
      }
    }
  }


  void computeInhibitionArea(
            const UInt    areaStart,
            const UInt    areaEnd,
            const bool    learn,
            const SDR&    distalInputActive,
            const SDR&    distalInputWinner,
            SDR_sparse_t &active,
            SDR_sparse_t &winner) {

    // Inhibition areas are contiguous blocks of cells, find the size of it.
    const auto areaSize = areaEnd - areaStart;
    // Compare the cell indexes by their feed-forward / proximal overlap.
    auto compare = [&](const UInt &a, const UInt &b) -> bool
                    { return cellOverlaps_[a] > cellOverlaps_[b]; };

    // Proximal competition, Qualifying round of competition.  This does not
    // activate cells, but rather disqualifies cells which clearly lost the
    // competition for feed-forward / proximal input.
    const UInt maxDesired = round( areaSize *
                  max( args_.maxDepolarizedSparsity, args_.maxBurstSparsity) );
    SDR_sparse_t qualifiedCells;  qualifiedCells.reserve( areaSize );
    for( auto cell = areaStart; cell < areaEnd; ++cell )
        { qualifiedCells.push_back( cell ); }
    // Do a partial sort to divide the winners from the losers.  This sort is
    // faster than a regular sort because it stops after it partitions the
    // elements about the Nth element, with all elements on their correct side
    // of the Nth element.
    std::nth_element( qualifiedCells.begin(),
                      qualifiedCells.begin() + maxDesired,
                      qualifiedCells.end(),
                      compare);
    // Remove cells which lost the competition.
    qualifiedCells.resize( maxDesired );
    // Apply activation threshold to proximal segments.
    applyProximalSegmentThreshold( qualifiedCells, args_.proximalSegmentThreshold );

    // Sort qualified cells by feed-forward / proximal overlap.  This allows for
    // running the competition by iterating through the sorted list.
    std::sort( qualifiedCells.begin(), qualifiedCells.end(), compare );

    // Run the first round of competition, for predicted / deplarized cells.
    const UInt predictedNumDesired = round( args_.maxDepolarizedSparsity * areaSize );
    const auto &predictiveCellsDense = predictiveCells_.getDense();
    // Also split the cells into predicted & unpredicted lists.  Make big lists
    // of activations, and then selectively remove the loser cells from them.
    SDR_sparse_t predictedActive;
    SDR_sparse_t unpredictedActive;
    for( const auto cell : qualifiedCells ) {
      if( predictiveCellsDense[cell] ) {
        if( predictedActive.size() < predictedNumDesired ) {
          // Activate this predicted / depolarized cell right now.
          predictedActive.push_back( cell );
          active.push_back( cell );
        }
        else {
          // Have reached maximum allowed of predicted active cells.  We can
          // guarentee that there will be no unpredictedActive cells, so don't
          // keep looking for them.
          break;
        }
      }
      else {
        // Maybe activate this cell if the predictive cells miss their quota and
        // this cell wins the competitions.
        unpredictedActive.push_back( cell );
      }
    }

    // Run the second round of competition, to meet the quota of active cells.
    const UInt minActive = round( args_.minSparsity * areaSize );
    UInt unpredictedNumDesiredActive;
    UInt unpredictedNumDesiredWinners;
    if( predictedActive.size() >= minActive ) {
      // Predictive Regime.  Nothing to do.
      unpredictedNumDesiredActive  = 0u;
      unpredictedNumDesiredWinners = 0u;
      rawAnomaly_ = 0.0f;
    }
    else {
      // Burst Regime.  Determine how many unpredicted cells should activate and
      // how many should learn / win.
      const Real percentBurst = (Real) (minActive - predictedActive.size()) / minActive;
      const UInt slope = round(args_.maxBurstSparsity * areaSize) - minActive;
      const UInt totalActive = (percentBurst * slope) + minActive;
      unpredictedNumDesiredActive  = totalActive - predictedActive.size();
      unpredictedNumDesiredWinners = minActive   - predictedActive.size();
      rawAnomaly_ = percentBurst;
    }
    // Activate unpredicted cells.
    unpredictedActive.resize( min( unpredictedNumDesiredActive, (UInt) unpredictedActive.size() ));
    for( const auto cell : unpredictedActive ) {
      active.push_back( cell );
    }
    unpredictedNumDesiredWinners = min( unpredictedNumDesiredWinners, (UInt) unpredictedActive.size() );

    if( learn &&
            // Don't learn if the distal dendrites are disabled, will crash.
            args_.distalMaxSegments           > 0u &&
            args_.distalMaxSynapsesPerSegment > 0u &&
            args_.distalAddSynapses           > 0u ) {
      learnDistalDendritesPredicted( predictedActive, distalInputActive, distalInputWinner, winner );
      learnDistalDendritesUnpredicted( distalInputActive, distalInputWinner,
                                unpredictedActive, unpredictedNumDesiredWinners, winner );
      learnDistalDendritesMispredicted( areaSize, areaEnd, predictedActive, unpredictedActive, distalInputActive );
    }
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

} // End namespace column_pooler
} // End namespace algorithmn
} // End namespace nupic
