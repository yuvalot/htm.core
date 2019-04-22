/* ---------------------------------------------------------------------
 * Copyright (C) 2018-2019, David McDougall.
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
 * ----------------------------------------------------------------------
 */

/**
 * Solving the MNIST dataset with Spatial Pooler.
 *
 * This consists of a simple black & white image encoder, a spatial pool, and an
 * SDR classifier.  The task is to recognise images of hand written numbers 0-9.
 * This should score at least 95%.
 */

#include <algorithm>
#include <cstdint> //uint8_t
#include <iostream>
#include <vector>

#include <nupic/algorithms/SpatialPooler.hpp>
#include <nupic/algorithms/ColumnPooler.cpp>
#include <nupic/algorithms/SDRClassifier.hpp>
#include <nupic/utils/SdrMetrics.hpp>

#include <mnist/mnist_reader.hpp> // MNIST data itself + read methods, namespace mnist::

namespace examples {

using namespace std;
using namespace nupic;
namespace column_pooler = nupic::algorithms::column_pooler;

using nupic::algorithms::spatial_pooler::SpatialPooler;
using namespace nupic::algorithms::column_pooler;
using nupic::algorithms::connections::Permanence;
using nupic::algorithms::sdr_classifier::Classifier;
using nupic::algorithms::sdr_classifier::argmax;


class MNIST {

  private:
    bool spNotCp;
    SpatialPooler sp;
    column_pooler::ColumnPooler cp;
    sdr::SDR input;
    sdr::SDR columns;
    Classifier clsr;
    mnist::MNIST_dataset<std::vector, std::vector<uint8_t>, uint8_t> dataset;

  public:
    UInt verbosity = 1;
    const UInt train_dataset_iterations = 1u;


void setup(bool spNotCp = false)
{
  input.initialize({28, 28});

  this->spNotCp = spNotCp;
  if( spNotCp ) {
    sp.initialize(
      /* inputDimensions */             input.dimensions,
      /* columnDimensions */            {28, 28}, //mostly affects speed, to some threshold accuracy only marginally
      /* potentialRadius */             5u,
      /* potentialPct */                0.5f,
      /* globalInhibition */            false,
      /* localAreaDensity */            0.20f,  //% active bits, //quite important variable (speed x accuracy)
      /* numActiveColumnsPerInhArea */  -1,
      /* stimulusThreshold */           6u,
      /* synPermInactiveDec */          0.005f,
      /* synPermActiveInc */            0.01f,
      /* synPermConnected */            0.4f,
      /* minPctOverlapDutyCycles */     0.001f,
      /* dutyCyclePeriod */             1402,
      /* boostStrength */               2.5f, //boosting does help
      /* seed */                        93u,
      /* spVerbosity */                 1u,
      /* wrapAround */                  false); //wrap is false for this problem

      columns.initialize({sp.getNumColumns()});
  }
  else
  {
    Parameters params;
    params.proximalInputDimensions     = input.dimensions;
    params.distalInputDimensions       = {0};
    params.inhibitionDimensions        = {10, 10};
    params.cellsPerInhibitionArea      = 200;
    params.minSparsity                 = .015;
    params.maxBurstSparsity            = .015;
    params.potentialPool               = DefaultTopology(.90, 4., false);
    params.proximalSegments            = 1;
    params.proximalSegmentThreshold    = 3;
    params.proximalIncrement           = .032;
    params.proximalDecrement           = .00928;
    params.proximalSynapseThreshold    = .422;
    params.proximalInitialPermanence   = defaultProximalInitialPermanence(0.422f, 0.5f);
    params.distalMaxSegments           = 0;
    params.distalMaxSynapsesPerSegment = 0;
    params.distalSegmentThreshold      = 0;
    params.distalSegmentMatch          = 0;
    params.distalAddSynapses           = 0;
    params.distalIncrement             = 0;
    params.distalDecrement             = 0;
    params.distalMispredictDecrement   = 0;
    params.distalSynapseThreshold      = 0;
    params.stabilityRate               = 0;
    params.fatigueRate                 = 0;
    params.period                      = 1402;
    params.seed                        = 0;
    params.verbose                     = verbosity;
    cp.initialize( params );

    columns.initialize(cp.cellDimensions);
  }

  clsr.initialize( /* alpha */ .001);

  dataset = mnist::read_dataset<std::vector, std::vector, uint8_t, uint8_t>(string("../ThirdParty/mnist_data/mnist-src/")); //from CMake
}

void train()
{
  if(verbosity)
    cout << "Training for " << (train_dataset_iterations * dataset.training_labels.size())
         << " cycles ..." << endl;
  size_t i = 0;

  sdr::Metrics inputStats(input,    1402);
  sdr::Metrics columnStats(columns, 1402);

  for(auto epoch = 0u; epoch < train_dataset_iterations; epoch++) {
    NTA_INFO << "epoch " << epoch;
    // Shuffle the training data.
    vector<UInt> index( dataset.training_labels.size() );
    for(auto s = 0u; s < dataset.training_labels.size(); s++)
      index[s] = s;

    Random().shuffle( index.begin(), index.end() );

    for(const auto idx : index) { // index = order of label (shuffeled)
      // Get the input & label
      const auto image = dataset.training_images.at(idx);
      const UInt label  = dataset.training_labels.at(idx);

      // Compute & Train
      input.setDense( image );
      if( spNotCp )
        sp.compute(input, true, columns);
      else {
        cp.reset();
        cp.compute(input, true, columns);
      }

      clsr.learn(columns, {label});
      if( verbosity && (++i % 1000 == 0) ) cout << "." << flush;
    }
    if( verbosity ) cout << endl;
  }
  cout << "epoch ended" << endl;
  cout << "inputStats "  << inputStats << endl;
  cout << "columnStats " << columnStats << endl;
  cout << cp.proximalConnections << endl;
}

void test() {
  // Test
  Real score = 0;
  UInt n_samples = 0;
  if(verbosity)
    cout << "Testing for " << dataset.test_labels.size() << " cycles ..." << endl;
  for(UInt i = 0; i < dataset.test_labels.size(); i++) {
    // Get the input & label
    const auto image  = dataset.test_images.at(i);
    const UInt label  = dataset.test_labels.at(i);

    // Compute
    input.setDense( image );
    if( spNotCp )
      sp.compute(input, false, columns);
    else {
      cp.reset();
      cp.compute(input, false, columns);
    }

    if( argmax( clsr.infer( columns ) ) == label)
        score += 1;
    n_samples += 1;
    if( verbosity && i % 1000 == 0 ) cout << "." << flush;
  }
  if( verbosity ) cout << endl;
  cout << "Score: " << 100.0 * score / n_samples << "% " << endl;
}

};  // End class MNIST
}   // End namespace examples

int main(int argc, char **argv) {
  examples::MNIST m;
  m.setup();
  m.train();
  m.test();

  return 0;
}

