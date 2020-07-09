/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2019, Numenta, Inc.
 *
 * Author: David Keeney, July 2020, dkeeney@gmail.com
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
 * This is not a normal region.  It does not need to be declared with an addRegion
 * and is inferred when the name is used in a link.
 * It facilitates the ability to directly set data into
 * another region's input.  
 *
 *To use this, the user will declare a Link in the configuration which uses the 
 * source region name of "INPUT" and a source name of their choosing.
 * The destination region and destination input name is the target input to be set.
 * The parameter 'dim' will specify the dimensions for the data.
 *
 * At runtime, between iterations, a C++ or Python app can then call 
 *       net.setInputData( "INPUT", source_name, dest_region, dest_input, a)
 *            where dest_region and dest_input are the names for the target and
 *            'a' is an Array object containing the data to set.
 * or on the REST api
 *      PUT  /network/<id>/region/INPUT/input/<source name>
 *                          The <data> will be in the body.
 * The source name will identify the Link to use to direct the data to the target region's input. 
 */
#include <memory>

#include <htm/engine/Input.hpp>
#include <htm/engine/Output.hpp>
#include <htm/engine/Region.hpp>
#include <htm/engine/Spec.hpp>
#include <htm/ntypes/Array.hpp>
#include <htm/utils/Log.hpp>


#include <htm/regions/InputRegion.hpp>


InputRegion::InputRegion(const ValueMap &params, Region *region) : RegionImpl(region) {}
InputRegion::InputRegion(ArWrapper &wrapper, Region *region){ }

virtual ~InputRegion::InputRegion() override { }

/*static*/ Spec *InputRegion::createSpec() {
  Spec *ns = new Spec();
  ns->parseSpec(R"(
  {name: "InputRegion"
  } )");
  return ns;
}
  
void InputRegion::initialize() override { }

void InputRegion::compute() override{ }

