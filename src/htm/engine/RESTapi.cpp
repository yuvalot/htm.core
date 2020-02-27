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

  // re-use the same id if we can.
  std::map<std::string, ResourceContext>::iterator itr = resource_.find(old_id);
  if (itr == rest.resource_.end()) {
    // not found, create a new context id
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

std::string RESTapi::put_input_request(const std::string &id, const std::string &input, const std::string &data) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    Array a;
    a.fromJSON(data);

    std::vector<std::string> names = split(input);
    NTA_CHECK(names.size() == 2) << "Expected input argument of request to be 'region'.'input'.";
    itr->second.net->getRegion(names[0])->setInputData(names[1], a);

    return "OK";
  }
  catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
}

std::string RESTapi::get_input_request(const std::string &id, const std::string &input) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    std::vector<std::string> names = split(input);
    NTA_CHECK(names.size() == 2) << "Expected input argument of request to be 'region'.'input'.";
    const Array &b = itr->second.net->getRegion(names[0])->getInputData(names[1]);

    std::string response = b.toJSON();
    return response;
  } catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
}

std::string RESTapi::get_output_request(const std::string &id, const std::string &output_name) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    std::string response;
    if (!output_name.empty()) {
      std::vector<std::string> names = split(output_name, '.');
      NTA_CHECK(names.size() == 2) << "Expected output name argument of request to be 'region'.'output name'.";
      const Array &b = itr->second.net->getRegion(names[0])->getOutputData(names[1]);
      response = b.toJSON();
    }
    return response;
  } catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
}

std::string RESTapi::put_param_request(const std::string &id, const std::string &param, const std::string &data) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    std::vector<std::string> names = split(param, '.');
    NTA_CHECK(names.size() == 2) << "Expected set param argument of request to be 'region'.'param'.  Found '" + param+"'.";

    itr->second.net->getRegion(names[0])->setParameterJSON(names[1], data);

    return "OK";
  } catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
}

std::string RESTapi::get_param_request(const std::string &id, const std::string &param) {
  try {
    auto itr = resource_.find(id);
    NTA_CHECK(itr != resource_.end()) << "Context for resource '" + id + "' not found.";
    itr->second.t = time(0);

    std::string response;
    if (!param.empty()) {
      std::vector<std::string> names = split(param, '.');
      NTA_CHECK(names.size() == 2) << "Expected get param argument of request to be 'region'.'param name'.";

      response = itr->second.net->getRegion(names[0])->getParameterJSON(names[1]);
    }
    return response;
  } catch (Exception &e) {
    return std::string("ERROR: ") + e.getMessage();
  }
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
