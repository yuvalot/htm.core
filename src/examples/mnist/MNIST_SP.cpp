/* ---------------------------------------------------------------------
 * Copyright (C) 2018, David McDougall.
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
#include <nupic/algorithms/ClassifierResult.hpp>
#include <nupic/utils/Random.hpp>
#include <nupic/utils/SdrMetrics.hpp>

#include <mnist/mnist_reader.hpp> // MNIST data itself + read methods, namespace mnist::

namespace examples {

using namespace std;
using namespace nupic;
namespace column_pooler = nupic::algorithms::column_pooler;

using nupic::algorithms::spatial_pooler::SpatialPooler;
using nupic::algorithms::column_pooler::DefaultTopology;
using nupic::algorithms::connections::Permanence;
using nupic::algorithms::sdr_classifier::SDRClassifier;
using nupic::algorithms::cla_classifier::ClassifierResult;


vector<UInt> read_mnist_labels(string path) {
    ifstream file(path);
    if( !file.is_open() ) {
        cerr << "ERROR: Failed to open file " << path << endl;
        exit(1);
    }
    int magic_number     = 0;
    int number_of_labels = 0;
    file.read( (char*) &magic_number,     4);
    file.read( (char*) &number_of_labels, 4);
    if(magic_number != 0x00000801) {
        std::reverse((char*) &magic_number,      (char*) &magic_number + 4);
        std::reverse((char*) &number_of_labels,  (char*) &number_of_labels + 4);
    }
    if(magic_number != 0x00000801) {
        cerr << "ERROR: MNIST data is compressed or corrupt" << endl;
        exit(1);
    }
    vector<UInt> retval;
    for(int i = 0; i < number_of_labels; ++i) {
        unsigned char label = 0;
        file.read( (char*) &label, 1);
        retval.push_back((UInt) label);
    }
    return retval;
}


vector<UInt*> read_mnist_images(string path) {
    ifstream file(path);
    if( !file.is_open() ) {
        cerr << "ERROR: Failed to open file " << path << endl;
        exit(1);
    }
    int magic_number     = 0;
    int number_of_images = 0;
    int n_rows           = 0;
    int n_cols           = 0;
    file.read( (char*) &magic_number,     4);
    file.read( (char*) &number_of_images, 4);
    file.read( (char*) &n_rows,           4);
    file.read( (char*) &n_cols,           4);
    if(magic_number != 0x00000803) {
        std::reverse((char*) &magic_number,      (char*) &magic_number + 4);
        std::reverse((char*) &number_of_images,  (char*) &number_of_images + 4);
        std::reverse((char*) &n_rows,            (char*) &n_rows + 4);
        std::reverse((char*) &n_cols,            (char*) &n_cols + 4);
    }
    if(magic_number != 0x00000803) {
        cerr << "ERROR: MNIST data is compressed or corrupt" << endl;
        exit(1);
    }
    NTA_ASSERT(n_rows == 28);
    NTA_ASSERT(n_cols == 28);
    UInt img_size = n_rows * n_cols;
    vector<UInt*> retval;
    for(int i = 0; i < number_of_images; ++i) {
        auto data_raw = new unsigned char[img_size];
        file.read( (char*) data_raw, img_size);
        // Copy the data into an array of UInt's
        auto data = new UInt[img_size];
        // auto data = new UInt[2 * img_size];
        // Apply a threshold to the image, yielding a B & W image.
        for(UInt pixel = 0; pixel < img_size; pixel++) {
            data[pixel] = data_raw[pixel] >= 128 ? 1 : 0;
            // data[2 * pixel] = data_raw[pixel] >= 128 ? 1 : 0;
            // data[2 * pixel + 1] = 1 - data[2 * pixel];
        }
        retval.push_back(data);
        delete[] data_raw;
    }
    return retval;
}


int main(int argc, char **argv) {
  UInt verbosity = 1;
  auto train_dataset_iterations = 1u;
  // int opt;
  // while ( (opt = getopt(argc, argv, "tv")) != -1 ) {  // for each option...
  //   switch ( opt ) {
  //     case 't':
  //         train_dataset_iterations += 1;
  //       break;
  //     case 'v':
  //         verbosity = 1;
  //       break;
  //     case '?':
  //         cerr << "Unknown option: '" << char(optopt) << "'!" << endl;
  //       break;
  //   }
  // }

  SDR input({28, 28});
  column_pooler::Parameters params   = column_pooler::DefaultParameters;
  params.proximalInputDimensions     = input.dimensions;
  params.distalInputDimensions       = {1};
  params.inhibitionDimensions        = {10, 10};
  params.cellsPerInhibitionArea      = 120;
  params.sparsity                    = .015;
  params.potentialPool               = column_pooler::DefaultTopology(.9, 4., false);
  params.proximalSegments            = 1;
  params.proximalSegmentThreshold    = 3, // 7, // 14;
  params.proximalIncrement           = .032;
  params.proximalDecrement           = .00928;
  params.proximalSynapseThreshold    = .422;
  params.distalMaxSegments           = 0;
  params.distalMaxSynapsesPerSegment = 0;
  params.distalSegmentThreshold      = 0;
  params.distalSegmentMatch          = 0;
  params.distalAddSynapses           = 0;
  params.distalIncrement             = 0;
  params.distalDecrement             = 0;
  params.distalMispredictDecrement   = 0;
  params.distalSynapseThreshold      = 0;
  params.stability_rate              = 0;
  params.fatigue_rate                = 0;
  params.period                      = 1402;
  params.seed                        = 0;
  params.verbose                     = verbosity;
  column_pooler::ColumnPooler htm( params );

  SDR cells( htm.cellDimensions );
  SDR_Metrics columnStats(cells, 1402);

  SDRClassifier clsr(
    /* steps */         {0},
    /* alpha */         .001,
    /* actValueAlpha */ .3,
                        verbosity);

  dataset = mnist::read_dataset<std::vector, std::vector, uint8_t, uint8_t>(string("../ThirdParty/mnist_data/mnist-src/")); //from CMake
}

void train() {
  // Train

  if(verbosity)
    cout << "Training for " << (train_dataset_iterations * dataset.training_labels.size())
         << " cycles ..." << endl;
  size_t i = 0;

  SDR_Metrics inputStats(input,    1402);
  SDR_Metrics columnStats(columns, 1402);

  for(auto epoch = 0u; epoch < train_dataset_iterations; epoch++) {
    NTA_INFO << "epoch " << epoch;
    // Shuffle the training data.
    vector<UInt> index( train_labels.size() );
    for(auto s = 0u; s < train_labels.size(); s++)
        index[s] = s;
    Random(3).shuffle( index.begin(), index.end() );

    for(auto s = 0u; s < train_labels.size(); s++) {
      // Get the input & label
      const auto image = dataset.training_images.at(idx);
      const UInt label  = dataset.training_labels.at(idx);

      // Compute & Train
      input.setDense( image );
      //sp.compute(input, true, columns); //TOGGLE SP/CP computation
      cp.compute(input, true, columns);

      ClassifierResult result;
      clsr.compute(htm.iterationNum, cells.getSparse(),
        /* bucketIdxList */   {label},
        /* actValueList */    {(Real)label},
        /* category */        true,
        /* learn */           true,
        /* infer */           false,
                              &result);
      if( verbosity && (++i % 1000 == 0) ) cout << "." << flush;
    }
    if( verbosity ) cout << endl;
  }
  cout << "epoch ended" << endl;
  cout << inputStats << endl;
  cout << columnStats << endl;
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
    htm.compute(input, false, cells);
    ClassifierResult result;
    clsr.compute(htm.iterationNum, cells.getSparse(),
      /* bucketIdxList */   {},
      /* actValueList */    {},
      /* category */        true,
      /* learn */           false,
      /* infer */           true,
                            &result);
    // Check results
    for(auto iter : result) {
      if( iter.first == 0 ) {
          const auto *pdf = iter.second;
          const auto max  = std::max_element(pdf->cbegin(), pdf->cend());
          const UInt cls  = max - pdf->cbegin();
          if(cls == label)
            score += 1;
          n_samples += 1;
      }
    }
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
