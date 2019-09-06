/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
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
 * --------------------------------------------------------------------- */

/** @file
 * Unit tests for the EncoderRegion
 */

#include "gtest/gtest.h"
#include <htm/engine/Network.hpp>
#include <htm/encoders/RandomDistributedScalarEncoder.hpp>
#include <htm/encoders/ScalarEncoder.hpp>
#include <htm/types/Sdr.hpp>
#include <string>
#include <vector>

using namespace htm;

// Encode some data by calling the encoders directly then
// we can compare the results when encoding it using NetworkAPI.

TEST(EncoderRegionTest, testConstructRDSE) {
  // Setup some data to test against.
  // Encode some data using the RDSE as an algorithm.
  SDR A({30000u});
  RDSE_Parameters P;
  P.size = 30000u;
  P.sparsity = 0.05f;
  P.resolution = 1.23f;
  P.seed = 1;
  RDSE R(P);
  R.encode(3, A);

  // Run the encoder using NetworkAPI
  Network net;
  std::shared_ptr<Region> r1 = net.addRegion("encoder", "EncoderRegion:RDSE",
       "{size: 30000, sparsity: 0.05, resolution: 1.23, seed: 1}");
  r1->setParameterReal64("sensedValue", 3.0);
  net.run(1);

  // Now, see if we get the same results
  Array C = r1->getOutputData("encoded");
  ASSERT_EQ(A, C.getSDR());
}

TEST(EncoderRegionTest, testConstructScaler) {
  // Setup some data to test against.
  SDR A({30000u});
  ScalarEncoderParameters P;
  P.maximum = 10.0;
  P.size = A.size;
  P.sparsity = 0.05f;
  ScalarEncoder R(P);
  R.encode(3, A);

  // Run the encoder using NetworkAPI
  Network net;
  std::shared_ptr<Region> r1 =
      net.addRegion("encoder", "EncoderRegion:ScalarEncoder",
                    "{size: 30000, maximum: 10, sparsity: 0.05}");
  r1->setParameterReal64("sensedValue", 3.0);
  net.run(1);

  // Now, see if we get the same results
  Array C = r1->getOutputData("encoded");
  ASSERT_EQ(A, C.getSDR());
}

TEST(EncoderRegionTest, testMultiEncoder) {
  // More than one encoder can contribute to an SDR passed to SP.
  // The encoders are connected in a Fan-in configuration.
  // In this case, encoder1 contributes twice as much weight as encoder2
  // because of the ratio of their element widths.
  // Any number of independent values can be combined in this manner.
  Network net;
  std::shared_ptr<Region> r1 = net.addRegion("encoder1", "EncoderRegion:ScalarEncoder",
    "{size: 600, maximum: 10, sparsity: 0.05}");
  std::shared_ptr<Region> r2 = net.addRegion("encoder2", "EncoderRegion:RDSE", 
    "{size: 300, sparsity: 0.05, resolution: 1.23}");
  std::shared_ptr<Region> sp = net.addRegion("sp", "SPRegion", "{dim: [10,10,3]}");
  net.link("encoder1", "sp", "", "", "encoded", "bottomUpIn");
  net.link("encoder2", "sp", "", "", "encoded", "bottomUpIn");

  r1->setParameterReal64("sensedValue", 3.0);
  r2->setParameterReal64("sensedValue", 6.0);
  net.run(1);

  SDR inSDR = sp->getInputData("bottomUpIn").getSDR();
  EXPECT_EQ(inSDR.size, 900);  // Size is width of both encoder outputs.

  SDR outSDR = sp->getOutputData("bottomUpOut").getSDR();
  EXPECT_EQ(outSDR.size, 300);
}

TEST(EncoderRegionTest, testSerialize) {
  // Run the encoder using NetworkAPI
  Network net;
  std::shared_ptr<Region> r1 = net.addRegion("encoder", "EncoderRegion:ScalarEncoder",
      "{size: 300, maximum: 10, sparsity: 0.05}");
  r1->setParameterReal64("sensedValue", 3.0);
  net.run(1);

  std::stringstream ss;
  net.save(ss);

  Network net2;
  net2.load(ss);

  ASSERT_EQ(net, net2);
}
