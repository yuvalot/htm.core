/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2021, Numenta, Inc.
 *                     David Keeney, dkeeney@gmail.com
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
 * Implementation of unit tests for the OverlapClassifier
 */

#include <cmath> // isnan
#include <iostream>
#include <limits> // numeric_limits
#include <sstream>
#include <stdio.h>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <htm/algorithms/OverlapClassifier.hpp>
#include <htm/utils/Log.hpp>

using namespace std;
using namespace htm;

namespace testing {


TEST(OverlapClassifierTest, ExampleUsageClassifier)
{
  // Example 1
  // Make a random SDR and associate it with the category B.
  SDR inputData({ 1000u });
      inputData.randomize( 0.02f );
  enum Category { A, B, C, D };
  OverlapClassifier clsr1;
  clsr1.learn(inputData, {Category::B});
  ASSERT_EQ(OverlapClassifier::argmax(clsr1.infer(inputData)), Category::B);

  // Example 2
  // Estimate a scalar value.  The Classifier only accepts categories, so
  // put real valued inputs into bins (AKA buckets) by subtracting the
  // minimum value and dividing by a resolution.
  OverlapClassifier clsr2;
  double scalar = 567.8f;
  double minimum = 500.0f;
  double resolution = 10.0f;
  clsr2.learn( inputData, { (UInt)((scalar - minimum) / resolution) } );
  ASSERT_EQ(OverlapClassifier::argmax(clsr2.infer(inputData)) * resolution + minimum, 560.0f);
}

TEST(OverlapClassifierTest, MultipleCategories) {
  // Test multiple category classification
  OverlapClassifier c;

  // Create a set of random SDR's and have the classifier learn them.
  // The index 0 to 999 is the category
  // the patterns vector contains generated SDR's for each category 0 through 999.
  // The SDR's have random bits with 2% sparsity.
  // There is a slight chance that that some pattern could overlap
  // with another pattern by one or even 2 bits by accedent but more than
  // that would be extreamly unlikely.
  std::vector<SDR> patterns;
  for (size_t i = 0; i < 1000; i++) {
    SDR s({2000});
    s.randomize(0.02f);
    patterns.push_back(s);
  }

  // Now lets have the classifier learn these patterns
  std::vector<UInt> category = {0};  // a one element array
  for (size_t i = 0; i < 1000; i++) {
    category[0] = static_cast<UInt>(i);
    c.learn(patterns[i], category);
  }

  // Test.  Lets pick on category 25
  // pattern 25 should match category 25 at near 100%
  PDF result1 = c.infer(patterns[25]);
  EXPECT_EQ(OverlapClassifier::argmax(result1), 25) << "Learned category for pattern 25 not found.";
  EXPECT_NEAR(result1[25], 1.0f, 0.001f) << "pattern 25 was not 100% probability for category 25.";

  // Try one category with multiple patterns
  // So store pattern 0 and pattern 1 along with pattern 25 for category 25.
  c.learn(patterns[0], {25});    // pattern 0 will match category 0 and category 25
  c.learn(patterns[1], {25});    // pattern 1 will match category 1 and category 25

  // It should show category 25 as a match for patterns 0, 1, and 25.
  // pattern 0 could be category 0 or category 25 so 50% each
  // pattern 1 could be category 1 or category 25 so 50% each
  result1 = c.infer(patterns[25]);
  EXPECT_NEAR(result1[25], 1.0f, 0.001f) << "Learned pattern 25 no longer matchs category 25 at 100%";
  result1 = c.infer(patterns[0]);
  EXPECT_NEAR(result1[25], 0.5f, 0.001f) << "Learned pattern 0 has no match for category 25";
  EXPECT_NEAR(result1[0], 0.5f, 0.001f) << "Learned pattern 0 has no match for category 0";
  result1 = c.infer(patterns[1]);
  EXPECT_NEAR(result1[25], 0.5f, 0.001f) << "Learned pattern 1 has no match for category 25";
  EXPECT_NEAR(result1[1], 0.5f, 0.001f) << "Learned pattern 1 has no match for category 1";

  // Other patterns should not match anything. 
  SDR s_other({2000});
  s_other.randomize(0.02f);  // generate another random pattern
  result1 = c.infer(s_other);
  EXPECT_NEAR(result1[25], 0.0f, 0.001f) << "Any pattern not learned should not match anything.";

}

TEST(OverlapClassifierTest, TwoBitOverlap) { 
  // We want to see the probablilty of a match with 2 bits overlap.
  // Create a set of 50 SDR's which have no overlap. 40 active bits each.
  std::vector<SDR> patterns;
  for (UInt i = 0; i < 50u; i++) {
    SDR s({2000});
    std::vector<UInt> sparse;
    for (UInt j = 0; j < 40u; j++) {
      sparse.push_back(i * 40u + j);
    }

    s.setSparse(sparse);
    patterns.push_back(s);
  }

  OverlapClassifier c;
  // Now lets have the classifier learn 49 of these patterns
  std::vector<UInt> category = {0}; // a one element array
  for (size_t i = 0; i < 49; i++) {
    category[0] = static_cast<UInt>(i);
    c.learn(patterns[i], category);
  }

  // Create an SDR which has only 2 bits that overlap with sdr 25.
  SDR st = patterns[49];
  std::vector<UInt>p = st.getSparse();
  p[0] = patterns[25].getSparse()[0];
  p[1] = patterns[25].getSparse()[1];
  p[2] = patterns[25].getSparse()[2];
  st.setSparse(p);
  
  PDF result1 = c.infer(st);
  EXPECT_NEAR(result1[0], 0.0f, 0.001f) << "Unexpected probablility for a 2 bit overlap";
}


TEST(OverlapClassifierTest, SaveLoad) {
  OverlapClassifier c1;

  // Train a Classifier with a few different things.
  SDR A({100u});  A.randomize(0.10f);
  SDR B({100u});  B.randomize(0.10f);
  SDR C({100u});  C.randomize(0.10f);
  SDR D({100u});  D.randomize(0.10f);
  c1.learn(A, {1u}); 
  c1.learn(B, {2u});
  c1.learn(C, {3u});
  c1.learn(D, {4u});
  const auto c1_out = c1.infer(A);

  // Save and load.
  stringstream ss;
  EXPECT_NO_THROW(c1.save(ss));
  OverlapClassifier c2;
  EXPECT_NO_THROW(c2.load(ss));

  // Expect identical results.
  const auto c2_out = c2.infer( A );
  ASSERT_EQ(c1_out, c2_out);
}




} // end namespace
