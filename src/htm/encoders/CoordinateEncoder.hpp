/* ---------------------------------------------------------------------
 * Numenta Platform for Intelligent Computing (NuPIC)
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
 *
 * http://numenta.org/licenses/
 * --------------------------------------------------------------------- */

/** @file
 * Define the CoordinateEncoder & CoordinateEncoderParameters
 */

#ifndef NTA_ENCODERS_COORD
#define NTA_ENCODERS_COORD

#include <htm/types/Sdr.hpp>
#include <htm/types/Types.hpp>
#include <htm/utils/Random.hpp>
#include <htm/types/Serializable.hpp>
#include <htm/encoders/RandomDistributedScalarEncoder.hpp>

namespace htm {

/*
 * TODO: DOCUMENTATION
 */
struct CoordinateEncoderParameters : public RDSE_Parameters {
  /*
   * TODO: DOCUMENTATION
   */
  UInt numDimensions;
};

/*
 * TODO: DOCUMENTATION
 TODO: EXPLAIN how this works...
 */
class CoordinateEncoder : public BaseEncoder<const std::vector<Real64> &>
{
public:
  CoordinateEncoder() {}
  CoordinateEncoder( const CoordinateEncoderParameters &parameters );
  void initialize( const CoordinateEncoderParameters &parameters );

  const CoordinateEncoderParameters &parameters = args_;

  void encode(const std::vector<Real64> &coordinates, SDR &output) override;


  CerealAdapter;

  template<class Archive>
  void save_ar(Archive & ar) const
  {
    ar(neighborhood_); // TODO: Finish this!
  }

  template<class Archive>
  void load_ar(Archive & ar)
  {
    ar(neighborhood_); // TODO: Finish this!
  }

  ~CoordinateEncoder() override {};

private:
  CoordinateEncoderParameters args_;
  SDR neighborhood_;
};     // End class CoordinateEncoder
}      // End namespace
#endif // End ifdef NTA_ENCODERS_COORD
