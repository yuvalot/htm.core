
#ifndef NTA_spatial_pooler_extended_HPP
#define NTA_spatial_pooler_extended_HPP

#include <nupic/algorithms/SpatialPooler.hpp>

using namespace std;

namespace nupic {
namespace algorithms {
namespace spatial_pooler_extended {

using namespace nupic::algorithms::spatial_pooler;

class SpatialPoolerExtended : public SpatialPooler
{
public:
  SpatialPoolerExtended(
    const vector<UInt> inputDimensions,
    const vector<UInt> columnDimensions,
    UInt  potentialRadius         = 16u,
    Real  potentialPct            = 0.5f,
    bool  wrapAround              = true,
    Real  localAreaDensity        = 0.015f,
    UInt  stimulusThreshold       = 0u,
    Real  synPermInactiveDec      = 0.008f,
    Real  synPermActiveInc        = 0.05f,
    Real  synPermConnected        = 0.1f,
    UInt  dutyCyclePeriod         = 1000u,
    Real  minPctOverlapDutyCycles = 0.001f,
    UInt  seed                    = 0u,
    UInt  spVerbosity             = 0u);

    void initialize(
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
            UInt                seed,
            UInt                spVerbosity);

  // TOOD: UInt initMapColumn_(UInt column) const override;
  // TODO: initMapPotential


  void boostOverlaps_(
    const vector<UInt> &overlaps, vector<Real> &boosted) const override;

  void inhibitColumnsGlobal_(const vector<Real> &overlaps, Real density,
                             vector<UInt> &activeColumns) const override;
};

}      // end namespace spatial_pooler_extended
}      // end namespace algorithms
}      // end namespace nupic
#endif // end NTA_spatial_pooler_extended_HPP
