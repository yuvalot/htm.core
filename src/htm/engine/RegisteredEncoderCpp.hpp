/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2015, Numenta, Inc.
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
 * Definition of the RegisteredEncoderCpp
 * Provides the plugin interface for the Cpp implemented encoders.
 * This is a subclass of RegisteredEncoder; the base class of an object that can instantiate
 * a plugin (a subclass of BaseRegion) and return its pointer.
 *
 */

#ifndef NTA_REGISTERED_ENCODER_CPP_HPP
#define NTA_REGISTERED_ENCODER_CPP_HPP

#include <string>
#include <htm/engine/RegisteredEncoder.hpp>

namespace htm
{

  template <class T>
  class RegisteredEncoderCpp: public RegisteredEncoder {
    public:
      RegisteredEncoderCpp(const std::string& classname="", const std::string& module="")
	  		: RegisteredEncoder(classname, module) {
	  }

      ~RegisteredEncoderCpp() override {
      }

      std::shared_ptr<GenericEncoder> createEncoder() override {
        return std::shared_ptr<T>(new T());
      }

      std::shared_ptr<GenericEncoder> deserializeEncoder(ArWrapper &wrapper) override {
        return std::shared_ptr<T>(new T(wrapper));
      }

  };

}

#endif // NTA_REGISTERED_ENCODER_CPP_HPP
