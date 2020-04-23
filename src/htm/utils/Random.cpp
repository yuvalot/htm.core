/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2013, Numenta, Inc.
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
    Random Number Generator implementation
*/
#include <iostream> // for istream, ostream
#include <chrono>   // for random seeds

#include <htm/utils/Log.hpp>
#include <htm/utils/Random.hpp>

using namespace htm;

bool Random::operator==(const Random &o) const {
  return seed_ == o.seed_ && \
	 steps_ == o.steps_ && \
	 gen == o.gen;
}

std::random_device rd; //HW RNG, undeterministic, platform dependant. Use only for seeding rng if random seed wanted (seed=0)

Random::Random(UInt64 seed) {
  if (seed == 0) {
      const unsigned int static_seed = rd();
      std::mt19937 static_gen(static_seed);
      NTA_INFO << "Random seed: " << static_seed;

    seed_ = static_gen(); //generate random value from HW RNG
  } else {
    seed_ = seed;
  }
  NTA_CHECK(seed_ != 0) << "Random: if seed is zero at this point, there is a logic error";
  gen.seed(static_cast<unsigned int>(seed_)); //seed the generator
  steps_ = 0;
}


namespace htm {
std::ostream &operator<<(std::ostream &outStream, const Random &r) {
  outStream << "random-v2" << " ";
  outStream << r.seed_ << " ";
  outStream << r.steps_ << " ";
  outStream << "endrandom-v2" << " ";
  return outStream;
}


std::istream &operator>>(std::istream &inStream, Random &r) {
  std::string version;

  inStream >> version;
  NTA_CHECK(version == "random-v2") << "Random() deserializer -- found unexpected version string '"
              << version << "'";
  inStream >> r.seed_;
  r.gen.seed(static_cast<unsigned int>(r.seed_)); //reseed
  inStream >> r.steps_;
  r.gen.discard(r.steps_); //advance n steps
  //FIXME we could de/serialize directly RNG gen, it should be multi-platform according to standard, 
  //but on OSX CI it wasn't (25/11/2018). So "hacking" the above instead. 
  std::string endtag;
  inStream >> endtag;
  NTA_CHECK(endtag == "endrandom-v2") << "Random() deserializer -- found unexpected end tag '" << endtag  << "'";
  inStream.ignore(1);

  return inStream;
}

// helper function for seeding RNGs across the plugin barrier
UInt32 GetRandomSeed() {
  return htm::Random().getUInt32();
}
} // namespace htm
