/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2019, Numenta, Inc.
 *
 * Author: David Keeney, Nov. 2019   dkeeney@gmail.com
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
 * Implementation of a network region that handles direct input to a region.
 */

#ifndef NTA_INPUTREGION_HPP
#define NTA_INPUTREGION_HPP

#include <string>
#include <vector>

#include <htm/engine/RegionImpl.hpp>
#include <htm/ntypes/Value.hpp>
#include <htm/types/Serializable.hpp>

namespace htm {

/**
 * A network region that handles direct input to a region.
 *
 * @b Description
 * The InputRegion is a region implementation that acts as a source of data flow for a 
 * link that obtains data from an app. Conceptually the user declares a link which 
 * declares the source region as "INPUT".  This will implicitly cause an InputRegion
 * to be declared. The InputRegion acts as the placeholder for data being provided 
 * from the app which the link will pass to the target input
 */
 
class InputRegion : public RegionImpl, Serializable {
public:
  InputRegion(const ValueMap &params, Region *region);
  InputRegion(ArWrapper &wrapper, Region *region);

  virtual ~InputRegion() override {}

  static Spec *createSpec();
  
  virtual void initialize() override;

  void compute() override;


  CerealAdapter;  // see Serializable.hpp
  // FOR Cereal Serialization
  template<class Archive>
  void save_ar(Archive& ar) const {
  }
  // FOR Cereal Deserialization
  // NOTE: the Region Implementation must have been allocated
  //       using the RegionImplFactory so that it is connected
  //       to the Network and Region objects. This will populate
  //       the region_ field in the Base class.
  template<class Archive>
  void load_ar(Archive& ar) {
  }

private:

};

} // namespace

#endif // NTA_INPUTREGION_HPP