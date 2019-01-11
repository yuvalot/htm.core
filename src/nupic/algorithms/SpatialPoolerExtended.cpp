
#include <nupic/algorithms/SpatialPoolerExtended.hpp>
using namespace nupic::algorithms::spatial_pooler_extended;

SpatialPoolerExtended::SpatialPoolerExtended(
        const vector<UInt>  inputDimensions,
        const vector<UInt>  columnDimensions,
        UInt                potentialRadius,
        Real                potentialPct,
        bool                wrapAround,
        Real                localAreaDensity,
        UInt                stimulusThreshold,
        Real                synPermInactiveDec,
        Real                synPermActiveInc,
        Real                synPermConnected,
        UInt                dutyCyclePeriod,
        Real                minPctOverlapDutyCycles,
        Int                 seed,
        UInt                spVerbosity)
    : SpatialPooler(
        /* inputDimensions */               inputDimensions,
        /* columnDimensions */              columnDimensions,
        /* potentialRadius */               potentialRadius,
        /* potentialPct */                  potentialPct,
        /* globalInhibition */              true,
        /* localAreaDensity */              localAreaDensity,
        /* numActiveColumnsPerInhArea */    -1,
        /* stimulusThreshold */             stimulusThreshold,
        /* synPermInactiveDec */            synPermInactiveDec,
        /* synPermActiveInc */              synPermActiveInc,
        /* synPermConnected */              synPermConnected,
        /* minPctOverlapDutyCycles */       minPctOverlapDutyCycles,
        /* dutyCyclePeriod */               dutyCyclePeriod,
        /* boostStrength */                 0.0f,
        /* seed */                          seed,
        /* spVerbosity */                   spVerbosity,
        /* wrapAround */                    wrapAround)
{
    activeDutyCycles_.assign(numColumns_, localAreaDensity_);
}

// TODO: initialize

// TODO: Rework potential pools

void SpatialPoolerExtended::boostOverlaps_(const vector<UInt> &overlaps, //TODO use Eigen sparse vector here
                                   vector<Real> &boosted) const {
  const Real denominator = 1.0f / log2( localAreaDensity_ );
  for (UInt i = 0; i < numColumns_; i++) {
    // Apply tiebreakers before boosting, so that boosting is applied to the
    // tiebreakers.  This is important so that the tiebreakers don't hurt the
    // entropy of the result by biasing some mini-columns to activte more often
    // than others.
    boosted[i] = (overlaps[i] + tieBreaker_[i])
                 * log2(activeDutyCycles_[i]) * denominator;
  }
}

void SpatialPoolerExtended::inhibitColumnsGlobal_(const vector<Real> &overlaps,
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

  // Add a tiebreaker to the overlaps so that the output is deterministic.
  vector<Real> overlaps_(overlaps.begin(), overlaps.end());
  for(UInt i = 0; i < numColumns_; i++)
    overlaps_[i] += tieBreaker_[i];

  // Compare the column indexes by their overlap.
  auto compare = [&overlaps_](const UInt &a, const UInt &b) -> bool
    {return overlaps_[a] > overlaps_[b];};

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
