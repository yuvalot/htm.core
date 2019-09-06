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
 * Defines the base class for all encoders.
 */

#ifndef NTA_GENERIC_ENCODER
#define NTA_GENERIC_ENCODER

#include <htm/ntypes/BasicType.hpp>
#include <htm/types/Sdr.hpp>
#include <string>

namespace htm {

struct BaseParameters {
};

// This structure is populated with the FIELD( ) macro
typedef struct {
  std::string name;
  int offset;
  NTA_BasicType type;
  std::string default_value;
} ParameterDescriptorFields;
typedef struct {
  NTA_BasicType expectedInputType;
  size_t expectedInputSize;
  char *parameterStruct;
  size_t parameterSize;
  std::map<std::string, ParameterDescriptorFields> parameters;
} ParameterDescriptor;

#define FIELD(n)                                        \
  { #n,                                                 \
    { #n,                                               \
      (int)((char *)&args_.n - (char *)&args_),         \
      BasicType::getType(typeid(args_.n)),              \
      std::to_string(args_.n)}                          \
  }


/**
 * Base class for all encoders that can be used as a plugin for EncoderRegion.
 * An encoder converts a value to a sparse distributed representation.
 *
 * Subclasses must implement method encode and Serializable interface.
 * Subclasses can optionally implement method reset.
 *
 * There are several critical properties which all encoders must have:
 *
 * 1) Semantic similarity:  Similar inputs should have high overlap.  Overlap
 * decreases smoothly as inputs become less similar.  Dissimilar inputs have
 * very low overlap so that the output representations are not easily confused.
 *
 * 2) Stability:  The representation for an input does not change during the
 * lifetime of the encoder.
 *
 * 3) Sparsity: The output SDR should have a similar sparsity for all inputs and
 * have enough active bits to handle noise and subsampling.
 *
 * Reference: https://arxiv.org/pdf/1602.05925.pdf
 */
class GenericEncoder : public Serializable {
public:
  virtual std::string getName() const = 0;

  /**
   * Members dimensions & size describe the shape of the encoded output SDR.
   * This is the total number of bits in the result.
   */
  const std::vector<UInt> &dimensions = dimensions_;
  const UInt &size = size_;

  virtual void reset() {}

  // for use by generic EncoderRegion to
  // generically pass parameter structure to encoder
  virtual void initialize(BaseParameters *ptrToParameters) = 0; 
  virtual ParameterDescriptor getDescriptor() = 0; // parameter descriptors
  virtual void encode(void *input, size_t input_count,
                      SDR &output) = 0; // untyped encode( ) call.

  virtual ~GenericEncoder() {}

  // overridden by including the macro CerealAdapter in subclass.
  virtual void cereal_adapter_save(ArWrapper &a) const {};
  virtual void cereal_adapter_load(ArWrapper &a){};

  // encoders should override
  virtual bool operator==(const GenericEncoder &other) const { return true; }

protected:
  GenericEncoder() {
    // initialize with dummy dimensions.  
    // Must set real dimensions later by calling init_base() again.
    std::vector<UInt> dim({0});
    init_base(dim);
  }

  GenericEncoder(const std::vector<UInt> dimensions) { 
    init_base(dimensions); 
  }

  void init_base(const std::vector<UInt> dimensions) {
    dimensions_ = dimensions;
    size_ = SDR(dimensions).size;
  }




private:
    std::vector<UInt> dimensions_;
    UInt              size_;
};






} // end namespace htm
#endif // NTA_GENERIC_ENCODER
