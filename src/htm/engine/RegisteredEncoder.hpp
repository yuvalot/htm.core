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
 * Definition of the RegisteredEncoder class
 *
 * A RegisteredEncoder is a base class of an object that can instantiate
 * an encoder plugin (a subclass of BaseEncoder) and get its spec.
 *
 * Each Programming language interface to this library must create a subclass
 * of RegisteredEncoder which will handle the EncoderRegion to encoder plugin instantiation.
 * For example, here are the subclasses which implement plugin interfaces:
 *   - RegisteredEncoderCpp for C++ builtin plugins (subclasses of BaseEncoder).
 *   - RegisteredEncoderPy  for Python implemented encoder plugins (PyBaseEncoder)
 *
 * the subclasses of RegistedEncoder must perform the following:
 *    1) Be Registered with the CPP engine using
 *              Network::registerEncoderCPP(name, your_subclass);
 *       It only needs to be registed once even if multiple EncoderRegions will use
 *       an instance of the same plugin. The 'name' used in this registration
 *       is the 'encoder type' when creating an EncodeRegion.
 *       It is like declaring the type of the encoder plugin.
 *       As a convention, the name used by C++ plugins will be the class name.
 *       The name for Python plugins should start with 'py_'. Those for CSharp
 *       will start with 'cs_', etc.
 *
 *    2) Instantiate the encoder plugin and return its pointer when createEncoder()
 *       is called.  Instantiate using the default constructor for the encoder.
 *
 *    3) Instantiate and deserialize the plugin when deserializeEncoder is called,
 *       returning its pointer. This gets called when the EncoderRegion is asked to
 *       deserialize.
 */

#ifndef NTA_REGISTERED_ENCODER_HPP
#define NTA_REGISTERED_ENCODER_HPP

#include <string>
#include <htm/encoders/GenericEncoder.hpp>

namespace htm
{

  class RegisteredEncoder {
  public:
    // 'classname' is name of the BaseEncoder subclass that is to be instantiated for this encoder plugin.
    // 'module' is the name (or path) to the shared library in which 'classname' resides.  Empty if staticly linked.
    RegisteredEncoder(const std::string& classname, const std::string& module=""){
		  classname_ = classname;
		  module_ = module;
    }

    virtual ~RegisteredEncoder() {}

    virtual std::shared_ptr<GenericEncoder> createEncoder() = 0;

    virtual std::shared_ptr<GenericEncoder> deserializeEncoder(ArWrapper &wrapper) = 0;

	virtual std::string className() { return classname_; }
	virtual std::string moduleName() { return module_; }

  protected:
	  std::string classname_;
	  std::string module_;

  };


}

#endif // NTA_REGISTERED_ENCODER_HPP
