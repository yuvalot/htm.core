/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2013-2020, Numenta, Inc.
 *
 * Author: David Keeney, dkeeney@gmail.com   Feb 2020
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
 * REST Interface for the NetworkAPI
 *
 * NOTE: The REST protocol is basically stateless.  Our application is not
 *       stateless because it stores the Network class as a resource and
 *       operations cause changes to the Network class.  But the protocol
 *       is stateless which means that each message contains everything
 *       needed for the server to carry out the operation.  We use the
 *       'id' field as the identifier for the Network class instance that
 *       is to be operated on.
 *
 *       Each "network create" message will create a Network object.  Each subsequent "run"
 *       message from the same client will use the same Network object to perform operations.
 *       Multiple clients can be using the same server at the same time.
 *
 *       For this to happen we need to know which Network object the "run" 
 *       message should apply to. Since the protocol is stateless, the "run" 
 *       messages must carry their own state or context. That context is a 
 *       resouce id that was returned from a previous "network create"  message 
 *       that created a Network context and stashed it in resouces map
 *       indexed by id.
 *
 *       There is a maximum of 65535 active Network class resources.
 *       A Network resource will timeout without activity in 24 hrs.
 *
 * LIMITATIONS:
 *       1) Only built-in C++ regions can be used.  There are plans to
 *          eventually allow connecting to Python regions and dynamically 
 *          loaded C++ regions.
 *       2) Save and Load operations via REST on the Network object have not 
 *          yet been implemented.
 */

#ifndef NTA_REST_API_HPP
#define NTA_REST_API_HPP


#include <htm/engine/Network.hpp>

namespace htm {

class RESTapi
{
public:
  static RESTapi* getInstance();

  /**
   *
   * Constructor for the RESTapi class
   *
   */
  RESTapi();
  ~RESTapi();

  /**
  * @b Description
  * Handler for a "create network resource" request message.
  * This will create a new Network object for this context and configure it.
  *
  * @param id  A client should use a id of "0" on the first call.  All subsequet calls
  * of any message type should use the id returned by this function.
  *
  * @param conf  A YAML or JSON string containing the configuration for the Network class.
  *              See Network::configure() for syntax.
  *
  * @retval On success it returns the id to use with this resource context. 
  *         Otherwise it returns the error message starting with "ERROR".
  */
  std::string create_network_request(const std::string &id, const std::string &conf);

  /**
   * @b Description:
   * Handler for a PUT "input" request message.
   * This will pass the attached data to the specified region's input.
   *
   * @param input       The name of the region, ".", followed by the input name
   *                    for the input that is to receive the data.  If blank, data is ignored.
   *                    For example;  If "encoder" is the name of your encoder
   *                                  configured in the Network class then
   *                                  "encoder.values" will send it to the
   *                                  "values" input of the encoder.
   *
   * @param data        The serialized data itself in JSON format. The following is expected:
   *                      {type: <type>, data: [ <array of elements comma seprated> ]}
   *                       The <type> values are one of the following:
   *                       "Byte",   "Int16",  "UInt16", "Int32",  "UInt32", "Int64",
   *                       "UInt64", "Real32", "Real64", "Handle", "Bool", "SDR", "String"
   *                    The data portion must be a sequence, even if only one element.
   *                    If blank, no data is set.
   *
   * @param id          Identifier for the resource context (a Network class instance).
   *                    Client should pass the id returned by the previous "configure"
   *                    request message.
   *
   * @retval            If success returns "OK".
   *                    Otherwise returns error message starting with "ERROR: ".
   */
  std::string put_input_request(const std::string &id,
                          const std::string &data,
                          const std::string &input);


  /**
   * @b Description:
   * Handler for a PUT "input" request message.
   * This will pass the attached data to the specified region's input.
   *
   * @param input       The name of the region, ".", followed by the input name
   *                    for the input that is to receive the data.  If blank, data is ignored.
   *                    For example;  If "encoder" is the name of your encoder
   *                                  configured in the Network class then
   *                                  "encoder.values" will send it to the
   *                                  "values" input of the encoder.
   *
   * @param id          Identifier for the resource context (a Network class instance).
   *                    Client should pass the id returned by the previous "configure"
   *                    request message.
   *
   * @retval            If success returns JSON encoded data.
   *                    The JSON serialized data format:
   *                      "{type: <type>, data: [ <array of elements comma seprated> ]}"
   *                       The <type> values are one of the following:
   *                       "Byte",   "Int16",  "UInt16", "Int32",  "UInt32", "Int64",
   *                       "UInt64", "Real32", "Real64", "Handle", "Bool", "SDR", "String"
   *                    The data portion is a comma separated sequence, even if only one element.
   *                    Otherwise returns error message starting with "ERROR: ".
   */
  std::string get_input_request(const std::string &id, const std::string &input);



  /**
   * @b Description:
   * Handler for a GET "output" request message.
   * This will capture the specified output values and return a response.
   *
   * @param id  Identifier for the resource context (a Network class instance).
   *            Client should pass the id returned by the previous "configure"
   *            request message.
   *
   *
   * @param output      The name of the region, ".", followed by the output name
   *                    for the output that is to be returned in the response message.
   *                    For example; if "tm" is the name of the TMRegion region and you want the
   *                                 active cells, use an output name of "tm.activeCells".
   *
   * @retval            If success returns the requested data which is an Array object.
   *                    The JSON serialized data format:
   *                      "{type: <type>, data: [ <array of elements comma seprated> ]}"
   *                       The <type> values are one of the following:
   *                       "Byte",   "Int16",  "UInt16", "Int32",  "UInt32", "Int64",
   *                       "UInt64", "Real32", "Real64", "Handle", "Bool", "SDR", "String"
   *                    The data portion is a comma separated sequence, even if only one element.
   *                    Otherwise returns error message starting with "ERROR: ".
   */
  std::string get_output_request(const std::string &id, const std::string &output);


  /**
   * @b Description:
   * Handler for a PUT "param" request message.
   * This will pass the attached data to the specified region's parameter.
   *
   * @param id          Identifier for the resource context (a Network class instance).
   *                    Client should pass the id returned by the previous "configure"
   *                    request message.
   *
   * @param name       The name of the region, ".", followed by the parameter name
   *                    for the parameter that is to receive the data.  Must be a ReadWrite parameter.
   *                    For example;  If "sp" is the name of your SP region
   *                                  configured in the Network class then
   *                                  "sp.potentialRadius" will send it to the
   *                                  "potentialRadius" parameter of the SP with the name "sp".
   *
   * @param data        The serialized data itself in JSON format. The following is expected:
   *                      {type: <type>, data: [ <array of elements comma seprated> ]}
   *                       The <type> values are one of the following:
   *                       "Byte",   "Int16",  "UInt16", "Int32",  "UInt32", "Int64",
   *                       "UInt64", "Real32", "Real64", "Handle", "Bool", "SDR", "String"
   *                    The data portion must be a sequence, even if only one element.
   *                    If blank, no data is set.
   *
   * @retval            If success returns "OK".
   *                    Otherwise returns error message starting with "ERROR: ".
   */
  std::string put_param_request(const std::string &id, const std::string &name, const std::string &data);


  /**
   * @b Description:
   * Handler for a GET "param" request message.
   * This will capture the specified parameter values and return a response.
   *
   * @param id  Identifier for the resource context (a Network class instance).
   *            Client should pass the id returned by the previous "configure"
   *            request message.
   *
   *
   * @param param      The name of the region, ".", followed by the parameter name
   *                    for the parameter that is to be returned in the response message.
   *                    For example; if "tm" is the name of the TMRegion region and you want the
   *                                 value of the "cellsPerColumn" paramter from it, 
   *                                 use an param name of "tm.cellsPerColumn".
   *
   * @retval            If success returns the requested data in JSON format.
   *                    Otherwise returns error message starting with "ERROR: ".
   */
  std::string get_param_request(const std::string &id, const std::string &output);

  /**
   * @b Description:
   * Handler for a "run" request message.
   * This will initialize the Network object if needed and execute the 
   * Network class with the indicated number of iterations. 
   *
   * @param id  Identifier for the resource context (a Network class instance). 
   *            Client should pass the id returned by the previous "configure" 
   *            request message.
   *
   * @param iterations  The number of iterations to run.  Normally this is 1, the default. 
   *
   * @retval            If success returns "OK".
   *                    Otherwise returns error message starting with "ERROR: ".
   */
  std::string run_request(const std::string &id, 
                          const std::string &iterations);


private:
  struct ResourceContext {
    std::string id;               // id for the resource
    time_t t;                     // last access time
    std::shared_ptr<Network> net; // context for this resource instance
  };

  // A map of open resources. 
  std::map<std::string, ResourceContext> resource_;
  std::string get_new_id_(const std::string &old_id);
};

} // namespace htm

#endif // NTA_REST_API_HPP
