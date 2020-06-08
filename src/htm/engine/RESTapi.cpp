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
Implementation of the RESTapi class
*/
#include <htm/engine/RESTapi.hpp>
#include <htm/engine/Network.hpp>

#define RESOURCE_TIMEOUT 86400    // one day

using namespace htm;

// Global values (singletons)
static RESTapi rest;
static unsigned int next_id = 1;

RESTapi::RESTapi() {}
RESTapi::~RESTapi() { }

RESTapi* RESTapi::getInstance() { return &rest; }

std::string RESTapi::get_new_id_(const std::string &old_id) {
  std::string id = old_id;

  // re-use the same id if we can or use a provide one.
  std::map<std::string, ResourceContext>::iterator itr;
  if (id.empty()) {
    // id is empty, create a new context id
    while (rest.resource_.size() < UINT16_MAX - 1) {
      unsigned int id_nbr = next_id++;
      if (id_nbr == 0)
        id_nbr = next_id++; // allow integer wrap of the id without a 0 value.
      char buf[10];
      std::snprintf(buf, sizeof(buf), "%4.04x", id_nbr);
      id = buf;

      // Make sure this new session id is not in use.
      itr = resource_.find(id);
      if (itr == resource_.end() || (itr->second.t < time(0) - RESOURCE_TIMEOUT)) {
        // This is one we can use
        break;
      }
    };
  }
  return id;
}



std::string RESTapi::create_network_request(const std::string &old_id, const std::string &config) {
  try {
    std::string id = get_new_id_(old_id);
    ResourceContext obj;
    obj.id = id;
    obj.t = time(0);
    obj.net.reset(new htm::Network);  // Allocate a Network object.

    obj.net->configure(config);
    resource_[id] = obj;
    return id;
  } catch (Exception& e) {
    return std::string("ERROR: ") + e.getMessage();
  } catch (std::exception& e) {
    return std::string("ERROR: ") + e.what();
  } catch (...) {
    return std::string("ERROR: Unknown Exception.");
  }
}

std::string RESTapi::put_input_request(const std::string &id, 
                                       const std::string &region_name,
                                       const std::string &input_name, 
                                       const std::string &data) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    Array a;
    a.fromJSON(data);

    itr->second.net->getRegion(region_name)->setInputData(input_name, a);

    return "OK";
  }
  catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
}

std::string RESTapi::get_input_request(const std::string &id, 
                                       const std::string &region_name,
                                       const std::string &input_name) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    const Array &b = itr->second.net->getRegion(region_name)->getInputData(input_name);

    std::string response = b.toJSON();
    return response;
  } catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
}

std::string RESTapi::get_output_request(const std::string &id, 
                                        const std::string &region_name,
                                        const std::string &output_name) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    std::string response;
    const Array &b = itr->second.net->getRegion(region_name)->getOutputData(output_name);
    response = b.toJSON();
    
    return response;
  } catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
}

std::string RESTapi::put_param_request(const std::string &id, 
                                       const std::string &region_name,
                                       const std::string &param_name, 
                                       const std::string &data) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    itr->second.net->getRegion(region_name)->setParameterJSON(param_name, data);

    return "OK";
  } catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
}

std::string RESTapi::get_param_request(const std::string &id, 
                                       const std::string &region_name,
                                       const std::string &param_name) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    std::string response;
    response = itr->second.net->getRegion(region_name)->getParameterJSON(param_name);
    
    return response;
  } catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
}

std::string RESTapi::delete_region_request(const std::string &id, const std::string &region_name) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    std::string response = "OK";
    itr->second.net->removeRegion(region_name);

    return response;
  } catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
}

std::string RESTapi::delete_link_request(const std::string &id, 
                                         const std::string &source_name,
                                         const std::string &dest_name) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    std::vector<std::string> args;
    args = split(source_name, '.');
    NTA_CHECK(args.size() == 2) << "Expected syntax <region>.<output> for source name. Found " << source_name;
    std::string source_region = args[0];
    std::string source_output = args[1];

    args = split(dest_name, '.');
    NTA_CHECK(args.size() == 2) << "Expected syntax <region>.<input> for destination name. Found " << dest_name;
    std::string dest_region = args[0];
    std::string dest_input = args[1];

    std::string response = "OK";
    itr->second.net->removeLink(source_region, dest_region, source_output, dest_input);
    return response;

  } catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
}

std::string RESTapi::delete_network_request(const std::string &id) {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    
    resource_.erase(itr);
    return "OK";
}


std::string RESTapi::run_request(const std::string &id, const std::string &iterations) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    int iter = 1;
    if (!iterations.empty()) {
      iter = std::strtol(iterations.c_str(), nullptr, 10);
    }
    itr->second.net->run(iter);
    return "OK";
  }
  catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
}

std::string RESTapi::command_request(const std::string& id, 
                                     const std::string& region_name,
                                     const std::string& command) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    std::string response;
    std::vector<std::string> args;
    args = split(command, ' ');
    response = itr->second.net->getRegion(region_name)->executeCommand(args);

    return response;
  } catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
}
