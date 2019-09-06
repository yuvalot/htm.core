/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2019, Numenta, Inc.
 *
 * Author: David Keeney, 2019   dkeeney@gmail.com
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
 * Defines ENCODERRegion, a generic Region implementation for any encoder that subclasses BaseEncoder.
 */

#ifndef NTA_ENCODERREGION_HPP
#define NTA_ENCODERREGION_HPP

#include <string>
#include <vector>

#include <htm/engine/RegionImpl.hpp>
#include <htm/ntypes/Value.hpp>
#include <htm/types/Serializable.hpp>
#include <htm/encoders/GenericEncoder.hpp>

namespace htm {
/**
 * A network region that encapsulates all of the encoders.
 *
 * @b Description
 * An EncoderRegion encapsulates any encoder based on BaseEncoder class, 
 * connecting it to the Network API. As a network runs, the client will 
 * specify new encoder inputs by either setting the "sensedValue" parameter 
 * or using an Input. 
 *
 * On each compute, the encoder will call the configured encoder and output its result.
 */
class EncoderRegion : public RegionImpl, Serializable {
public:
  EncoderRegion(const ValueMap &params, Region *region);
  EncoderRegion(ArWrapper &wrapper, Region *region);

  virtual ~EncoderRegion() override;

  // NOTE: The EncoderRegion has special handling so it can load the encoder
  //       to complete the spec.
  static Spec *createSpec(std::shared_ptr<GenericEncoder> encoder);
  static Spec* createSpec() {
    auto ns = new Spec;

    ns->description = "EncoderRegion. This is a placeholder spec.  The EncoderRegion "
                      "is a plugin container for actual encoders. The real spec "
                      "is found under 'EncoderRegion:<encoder name>`. ";
    return ns;
  }


  
  virtual Int32 getParameterInt32(const std::string &name,
                                  Int64 index) override;
  virtual UInt32 getParameterUInt32(const std::string &name,
                                    Int64 index) override;
  virtual Int64 getParameterInt64(const std::string &name,
                                  Int64 index) override;
  virtual UInt64 getParameterUInt64(const std::string &name,
                                    Int64 index) override;
  virtual Real32 getParameterReal32(const std::string &name,
                                    Int64 index) override;
  virtual Real64 getParameterReal64(const std::string &name,
                                    Int64 index) override;
  virtual bool getParameterBool(const std::string &name, Int64 index) override;

  virtual void setParameterInt32(const std::string &name, Int64 index,
                                 Int32 value) override;
  virtual void setParameterUInt32(const std::string &name, Int64 index,
                                  UInt32 value) override;
  virtual void setParameterInt64(const std::string &name, Int64 index,
                                 Int64 value) override;
  virtual void setParameterUInt64(const std::string &name, Int64 index,
                                  UInt64 value) override;
  virtual void setParameterReal32(const std::string &name, Int64 index,
                                  Real32 value) override;
  virtual void setParameterReal64(const std::string &name, Int64 index,
                                  Real64 value) override;
  virtual void setParameterBool(const std::string &name, Int64 index,
                                bool value) override;


  virtual std::string getParameterString(const std::string &name, Int64 index) override;
  virtual void initialize() override;

  void compute() override;

  virtual Dimensions askImplForOutputDimensions(const std::string &name) override;

  CerealAdapter;  // see Serializable.hpp
  // FOR Cereal Serialization
  template<class Archive>
  void save_ar(Archive& ar) const {
    ar(CEREAL_NVP(sensedValue_));
    ar(cereal::make_nvp("encoder", encoder_));
  }
  // FOR Cereal Deserialization
  // NOTE: the Region Implementation must have been allocated
  //       using the RegionImplFactory so that it is connected
  //       to the Network and Region objects. This will populate
  //       the region_ field in the Base class.
  template<class Archive>
  void load_ar(Archive& ar) {
    ar(CEREAL_NVP(sensedValue_));
    ar(cereal::make_nvp("encoder", encoder_));
    setDimensions(encoder_->dimensions); 
  }



  bool operator==(const RegionImpl &other) const override;
  inline bool operator!=(const RegionImpl &other) const override {
    return !operator==(other);
  }

private:
  std::shared_ptr<char> sensedValue_;  // could be any NTA_BasicType numeric type.

  std::shared_ptr<GenericEncoder> encoder_;
  char *args_;  // a pointer to the beginning of the encoder's parameter structure.
  std::shared_ptr<Spec> spec_;
  ParameterDescriptor desc_;

};
} // namespace htm

#endif // NTA_ENCODERREGION_HPP
