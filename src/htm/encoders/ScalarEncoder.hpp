/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2016, Numenta, Inc.
 *               2019, David McDougall
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
 * Define the ScalarEncoder
 */

#ifndef NTA_ENCODERS_SCALAR
#define NTA_ENCODERS_SCALAR

#include <htm/types/Types.hpp>
#include <htm/encoders/GenericEncoder.hpp>

namespace htm {

  /**
   * These four (4) members define the total number of bits in the output:
   *      size,
   *      radius,
   *      category,
   *      resolution.
   *
   * These are mutually exclusive and only one of them should be non-zero when
   * constructing the encoder.
   */
  struct ScalarEncoderParameters: public BaseParameters
  {
    /**
     * Members "minimum" and "maximum" define the range of the input signal.
     * These endpoints are inclusive.
     */
    Real64 minimum = 0.0f;
    Real64 maximum = 0.0f;

    /**
     * Member "clipInput" determines whether to allow input values outside the
     * range [minimum, maximum].
     * If true, the input will be clipped into the range [minimum, maximum].
     * If false, inputs outside of the range will raise an error.
     */
    bool clipInput = false;

    /**
     * Member "periodic" controls what happens near the edges of the input
     * range.
     *
     * If true, then the minimum & maximum input values are the same and the
     * first and last bits of the output SDR are adjacent.  The contiguous
     * block of 1's wraps around the end back to the beginning.
     *
     * If false, then minimum & maximum input values are the endpoints of the
     * input range, are not adjacent, and activity does not wrap around.
     */
    bool periodic = false;

    /**
     * Member "category" means that the inputs are enumerated categories.
     * If true then this encoder will only encode unsigned integers, and all
     * inputs will have unique / non-overlapping representations.
     */
    bool category = false;

    /**
     * Member "activeBits" is the number of true bits in the encoded output SDR.
     * The output encodings will have a contiguous block of this many 1's.
     */
    UInt activeBits = 0u;

    /**
     * Member "sparsity" is an alternative way to specify the member "activeBits".
     * Sparsity requires that the size to also be specified.
     * Specify only one of: activeBits or sparsity.
     */
    Real sparsity = 0.0f;

    /**
     * Member "size" is the total number of bits in the encoded output SDR.
     */
    UInt size = 0u;

    /**
     * Member "radius" Two inputs separated by more than the radius have
     * non-overlapping representations. Two inputs separated by less than the
     * radius will in general overlap in at least some of their bits. You can
     * think of this as the radius of the input.
     */
    Real64 radius = 0.0f;

    /**
     * Member "resolution" Two inputs separated by greater than, or equal to the
     * resolution are guaranteed to have different representations.
     */
    Real64 resolution = 0.0f;
  };

  /**
   * Encodes a real number as a contiguous block of 1's.
   *
   * Description:
   * The ScalarEncoder encodes a numeric (floating point) value into an array
   * of bits. The output is 0's except for a contiguous block of 1's. The
   * location of this contiguous block varies continuously with the input value.
   *
   * To inspect this run:
   * $ python -m htm.examples.encoders.scalar_encoder --help
   */
  class ScalarEncoder : public GenericEncoder
  {
  public:
    ScalarEncoder();  // Note: encoder cannot be used until parameters are passed to initialize( );
    ScalarEncoder( const ScalarEncoderParameters &parameters );
    ScalarEncoder(ArWrapper &ar); // for Cereal load_ar( )

    void initialize( const ScalarEncoderParameters &parameters );
    void initialize(BaseParameters *params) override { // for use by generic EncoderRegion
      initialize(*(ScalarEncoderParameters *)params);
    }

    std::string getName() const override { return "ScalarEncoder"; }
    // A discription of the fields in the RDSE_Parameters structure.
    ParameterDescriptor getDescriptor() override {
      ParameterDescriptor desc = {
          NTA_BasicType_Real64, // expected input type
          1,                    // expected input array size (0 means variable)
          (char *)&args_,       // pointer to parameter structure (for getters)
          sizeof(args_),        // sizeof parameter structure
          // list of fields in the parameter structure
          {FIELD(minimum),  FIELD(maximum),    FIELD(clipInput), 
           FIELD(periodic), FIELD(category),   FIELD(activeBits), 
           FIELD(sparsity),  FIELD(size),      FIELD(radius),     
           FIELD(resolution)}};
      return desc;
    }


    const ScalarEncoderParameters &parameters = args_;

    void encode(Real64 input, SDR &output);

    // Special generic call for use by EncoderRegion.
    // For this encoder, cast the inputPtr to a Real64* and dereference it.
    void encode(void *inputPtr, size_t count, SDR &output) override {
      NTA_CHECK(count == 1) << "Unexpected input count for RDSE encoder.";
      Real64 input = *((Real64 *)inputPtr);
      encode(input, output);
    }

    CerealAdapter;  // see Serializable.hpp
    // FOR Cereal Serialization
    template<class Archive>
    void save_ar(Archive& ar) const {
      std::string name = "ScalarEncoder";
      ar(cereal::make_nvp("name", name));
      ar(cereal::make_nvp("minimum", args_.minimum));
      ar(cereal::make_nvp("maximum", args_.maximum));
      ar(cereal::make_nvp("clipInput", args_.clipInput));
      ar(cereal::make_nvp("periodic", args_.periodic));
      ar(cereal::make_nvp("category", args_.category));
      ar(cereal::make_nvp("activeBits", args_.activeBits));
      ar(cereal::make_nvp("sparsity", args_.sparsity));
      ar(cereal::make_nvp("size", args_.size));
      ar(cereal::make_nvp("radius", args_.radius));
      ar(cereal::make_nvp("resolution", args_.resolution));
    }
  
    // FOR Cereal Deserialization
    template<class Archive>
    void load_ar(Archive& ar) {
      std::string name;
      ar(cereal::make_nvp("name", name));
      ar(cereal::make_nvp("minimum", args_.minimum));
      ar(cereal::make_nvp("maximum", args_.maximum));
      ar(cereal::make_nvp("clipInput", args_.clipInput));
      ar(cereal::make_nvp("periodic", args_.periodic));
      ar(cereal::make_nvp("category", args_.category));
      ar(cereal::make_nvp("activeBits", args_.activeBits));
      ar(cereal::make_nvp("sparsity", args_.sparsity));
      ar(cereal::make_nvp("size", args_.size));
      ar(cereal::make_nvp("radius", args_.radius));
      ar(cereal::make_nvp("resolution", args_.resolution));
    }

    ~ScalarEncoder() override {};

    bool operator==(const GenericEncoder &other) const override;
    inline bool operator!=(const ScalarEncoder &other) const {
      return !operator==(other);
    }


  private:
    ScalarEncoderParameters args_;
  };   // end class ScalarEncoder

  std::ostream & operator<<(std::ostream & out, const ScalarEncoder &self);

}      // end namespace htm
#endif // end NTA_ENCODERS_SCALAR
