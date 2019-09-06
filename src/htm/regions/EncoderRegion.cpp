/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2019, Numenta, Inc.
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
 * Implementation of the ScalarSensor Region
 */

#include <htm/regions/EncoderRegion.hpp>

#include <htm/engine/Input.hpp>
#include <htm/engine/Output.hpp>
#include <htm/engine/Region.hpp>
#include <htm/engine/RegionImplFactory.hpp>
#include <htm/engine/Spec.hpp>
#include <htm/ntypes/Array.hpp>
#include <htm/ntypes/BasicType.hpp>
#include <htm/utils/Log.hpp>

namespace htm {

EncoderRegion::EncoderRegion(const ValueMap &params, Region *region)
    : RegionImpl(region) {

  // instantiate the encoder plugin
  // An encoder instance would have been created when the factory
  // created the spec but that instance is discarded.
  // Get the spec that was previously created and cached
  // then create and initialize the encoder to use for this region instance.
  std::string encoderName =
      region->getType().substr(std::string("EncoderRegion:").length());
  RegionImplFactory &factory = RegionImplFactory::getInstance();
  spec_ = factory.getSpec(region->getType());
  encoder_ = factory.createEncoder(encoderName);
  desc_ = encoder_->getDescriptor();

  // populate the parameter structure
  char *args = new char[desc_.parameterSize];
  std::memset(args, 0, desc_.parameterSize);
  for (auto iter : desc_.parameters) {
    auto fld = iter.second;
    params.assign(fld.name, args + fld.offset, fld.type);
  }
  // Initialize the encoder using the parameters.
  encoder_->initialize((BaseParameters*)args);
  delete[] args;

  // Populate a variable to hold the sensedValue parameter.
  // This will be passed into the encode( ) function if no Link is connected
  // to the "value" Input.
  std::shared_ptr<char> val(
      new char[BasicType::getSize(desc_.expectedInputType)],
      std::default_delete<char[]>());
  sensedValue_ = val;
  params.assign("sensedValue", sensedValue_.get(), desc_.expectedInputType);
}

EncoderRegion::EncoderRegion(ArWrapper &wrapper, Region *region)
    : RegionImpl(region) {
  cereal_adapter_load(wrapper);
}

void EncoderRegion::initialize() {
  // Normally a region will create the algorithm here, at the point
  // when the dimensions and parameters are known. But in this case
  // it is the encoder that determines the dimensions so it must be
  // allocated in the constructor.
}

Dimensions EncoderRegion::askImplForOutputDimensions(const std::string &name) {

  if (name == "encoded") {

    // get the dimensions determined by the encoder (see parameter.size).
    Dimensions encDim(encoder_->dimensions); // get dimensions from encoder
    return encDim;
  }
  // for any other output name, let RegionImpl handle it.
  return RegionImpl::askImplForOutputDimensions(name);
}

void EncoderRegion::compute() {
  // get the output buffer.
  SDR &output = getOutput("encoded")->getData().getSDR();

  // Get the input value.
  // - if there is a link connected for "values" Input, use that as source.
  // - otherwise use the sensedValue parameter as the source.
  Input *input = getInput("values");
  if (input->hasIncomingLinks()) {
    Array &data = input->getData();
    encoder_->encode(data.getBuffer(), data.getCount(), output);
  } else {
    encoder_->encode(sensedValue_.get(), 1, output);
  }
}

EncoderRegion::~EncoderRegion() {}

/* static */ Spec *
EncoderRegion::createSpec(std::shared_ptr<GenericEncoder> encoder) {
  // Note: this should return a shared_ptr<Spec> but to change it would break
  // the API.

  Spec *ns = new Spec();

  auto desc = encoder->getDescriptor();
  ns->description = encoder->getName();

  /* ----- parameters ----- */

  // Add all of the encoder parameters as CreateAccess.
  for (auto p : desc.parameters) {
    ns->parameters.add(p.second.name,
                       ParameterSpec(p.second.name, p.second.type,
                                     1,  // elementCount -- a single value
                                     "", // constraints
                                     p.second.default_value,
                                     ParameterSpec::CreateAccess));
  }

  ns->parameters.add("encoder",
                     ParameterSpec("The name of the encoder.",
                                   NTA_BasicType_Byte,
                                   0,  // elementCount
                                   "", // constraints
                                   encoder->getName(), // defaultValue
                                   ParameterSpec::ReadOnlyAccess));

  ns->parameters.add("sensedValue",
                     ParameterSpec("Scalar input  (for backward compatability)",
                                   desc.expectedInputType,
                                   1,  // elementCount
                                   "", // constraints
                                   "0", // defaultValue
                                   ParameterSpec::ReadWriteAccess));

  /* ----- inputs  ----- */
  ns->inputs.add("values",
                 InputSpec("The input vector.",    // description
                           desc.expectedInputType, // type
                           0,                      // count.
                           false,                  // required?
                           false,                  // isRegionLevel,
                           true                    // isDefaultInput
                           ));
  ns->inputs.add("reset",
                 InputSpec("The reset signal.", // description
                           NTA_BasicType_Bool,  // type
                           1,                   // count.
                           false,               // required?
                           false,               // isRegionLevel,
                           false                // isDefaultInput
                           ));

  /* ----- outputs ----- */
  ns->outputs.add("encoded", OutputSpec("Encoded value", NTA_BasicType_SDR,
                                        0,    // elementCount
                                        true, // isRegionLevel
                                        true  // isDefaultOutput
                                        ));

  return ns;
}

//    The getters
#define getParameter(T, name)                                                  \
  T EncoderRegion::getParameter##T(const std::string &name, Int64 index) {     \
    if (name == "sensedValue" && desc_.expectedInputType == NTA_BasicType_##T) \
      return *((T *)(sensedValue_.get()));                                     \
    auto iter = desc_.parameters.find(name);                                   \
    if (iter == desc_.parameters.end() ||                                      \
        iter->second.type != NTA_BasicType_##T)                                \
      return this->RegionImpl::getParameter##T(name, index);                   \
    return *((T *)(desc_.parameterStruct + iter->second.offset));              \
  }
getParameter(Real64, name);
getParameter(Real32, name);
getParameter(Int32, name);
getParameter(UInt32, name);
getParameter(Int64, name);
getParameter(UInt64, name);
getParameter(Bool, name);

//    the setters
#define setParameter(T, name, value)                                           \
  void EncoderRegion::setParameter##T(const std::string &name, Int64 index,    \
                                      T value) {                               \
    if (name == "sensedValue" && desc_.expectedInputType == NTA_BasicType_##T) \
      *((T *)(sensedValue_.get())) = value;                                    \
    else {                                                                     \
      this->RegionImpl::setParameter##T(name, index, value);                   \
    }                                                                          \
  }
setParameter(Real64, name, value);
setParameter(Real32, name, value);
setParameter(Int32, name, value);
setParameter(UInt32, name, value);
setParameter(Int64, name, value);
setParameter(UInt64, name, value);
setParameter(Bool, name, value);

std::string EncoderRegion::getParameterString(const std::string &name,
                                              Int64 index) {
  if (name == "encoder")
    return encoder_->getName();
  return this->RegionImpl::getParameterString(name, index);
}

bool EncoderRegion::operator==(const RegionImpl &other) const {
  if (other.getType() != getType())
    return false;
  // encoders the same
  EncoderRegion &o = (EncoderRegion &)other;
  if (o.encoder_->getName() != encoder_->getName())
    return false;
  GenericEncoder *selfEncoder = encoder_.get();
  GenericEncoder *otherEncoder = o.encoder_.get();
  return (*(otherEncoder) == *(selfEncoder));
}

} // namespace htm
