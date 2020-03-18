/* ---------------------------------------------------------------------
 * HTM Community Edition of NuPIC
 * Copyright (C) 2013-2020, Numenta, Inc.
 *
 * author: David Keeney, dkeeney@gmail.com, Feb 2020
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
//
// Test the REST API facility.
// This test will start up a server in a new thread and then use the original
// thread to act as the client.
//

#include "gtest/gtest.h"

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>

#include <httplib.h>
#include <examples/rest/server_core.hpp>

namespace testing {

using namespace htm;


static int port = 8050;
static bool verbose = false; // turn this on to print extra stuff for debugging the test.
#define VERBOSE  if (verbose)  std::cerr << "[          ] "
#define EPOCHS 3


static std::string dump_headers(const Headers &headers) {
  std::string s;
  char buf[BUFSIZ];

  for (const auto &x : headers) {
    snprintf(buf, sizeof(buf), "  %s: %s\n", x.first.c_str(), x.second.c_str());
    s += buf;
  }

  return s;
}

static std::string log(const Request &req, const Response &res) {
  std::string s;
  char buf[BUFSIZ];
  s += "================================\n";
  snprintf(buf, sizeof(buf), "%s %s %s", req.method.c_str(), req.version.c_str(), req.path.c_str());
  s += buf;
  std::string query;
  for (auto it = req.params.begin(); it != req.params.end(); ++it) {
    const auto &x = *it;
    snprintf(buf, sizeof(buf), "%c%s=%s", (it == req.params.begin()) ? '?' : '&', x.first.c_str(), x.second.c_str());
    query += buf;
  }
  s += query + "\n";
  s += dump_headers(req.headers);
  if (!req.body.empty())
    s += "body: " + req.body + "\n";
  s += "--------------------------------\n";
  snprintf(buf, sizeof(buf), "%d %s\n", res.status, res.version.c_str());
  s += buf;
  s += dump_headers(res.headers);
  if (!res.body.empty()) {
    s += "body: " + std::string(res.body);
  }
  s += "\n";
  return s;
}


static void serverThread() {
  // This starts the NetworkAPI REST server
  std::string net_interface = "127.0.0.1";
  try {
    RESTserver server;
    if (verbose) server.set_logger([](const Request &req, const Response &res) { std::cout << log(req, res); });
    server.listen(port, net_interface.c_str());
    // will exit listen loop when the "GET /stop" message is received.
  } catch (Exception &e) {
    ASSERT_TRUE(false) << "REST Server Error: " << e.getMessage();
  }
}

TEST(RESTapiTest, example) {
  // A test similar to the Client Example.
  std::thread threadObj(serverThread); // start REST server
  std::this_thread::sleep_for(std::chrono::seconds(1)); // give server time to start

  // Client thread.
  const httplib::Params noParams;
  char message[1000];

  httplib::Client client("127.0.0.1", port);
  client.set_timeout_sec(30);

  // request "Hello World" to see if we are able to connect to the server.
  auto res = client.Get("/hi");
  ASSERT_TRUE(res) << "No response from server.";
  ASSERT_EQ(res->status,200) << "Unexpected status returned: " << res->status << std::endl;
  EXPECT_STREQ(res->body.c_str(), "Hello World!\n") << "Response to GET /hi request";

  // Configure a NetworkAPI example
  // See Network.configure() for syntax.
  //     Simple situation    Encoder  ==>  SP  ==>  TM
  //     Compare this to the napi_sine example.
  std::string config = R"(
   {network: [
       {addRegion: {name: "encoder", type: "RDSEEncoderRegion", params: {size: 1000, sparsity: 0.2, radius: 0.03, seed: 2019, noise: 0.01}}},
       {addRegion: {name: "sp", type: "SPRegion", params: {columnCount: 2048, globalInhibition: true}}},
       {addRegion: {name: "tm", type: "TMRegion", params: {cellsPerColumn: 8, orColumnOutputs: true}}},
       {addLink:   {src: "encoder.encoded", dest: "sp.bottomUpIn"}},
       {addLink:   {src: "sp.bottomUpOut", dest: "tm.bottomUpIn"}}
    ]})";


  res = client.Post("/network", config, "application/json");
  ASSERT_TRUE(res && res->status/100 == 2 && res->body.size() == 5) << "Failed Response to POST /network request.";
  std::string id = res->body.substr(0,4);

  // Send GET parameter message to retreive "tm.cellsPerColumn" parameter from the tm region.
  snprintf(message, sizeof(message), "/network/%s/region/tm/param/cellsPerColumn", id.c_str());
  res = client.Get(message);
  ASSERT_TRUE(res && res->status/100 == 2) << " GET param message failed.";
  EXPECT_TRUE(trim(res->body) == "8") << "Response to GET param request";


  // execute
  float x = 0.00f;
  for (size_t e = 0; e < EPOCHS; e++) {
    // -- sine wave, 0.01 radians per iteration   (Note: first iteration is for x=0.01, not 0)
    x += 0.01f; // step size for fn(x)
    double s = std::sin(x);

    // Send set parameter message to feed "sensedValue" parameter data into RDSE encoder for this iteration.
    snprintf(message, sizeof(message), "/network/%s/region/encoder/param/sensedValue?data=%.02f", id.c_str() , s);
    res = client.Put(message, noParams);
    ASSERT_TRUE(res && res->status / 100 == 2) << " PUT param message failed.";
    EXPECT_STREQ(trim(res->body).c_str(), "OK") << "Response to PUT param request";

    // Execute an iteration
    snprintf(message, sizeof(message), "/network/%s/run", id.c_str());
    res = client.Get(message);
    EXPECT_STREQ(trim(res->body).c_str(), "OK") << "Response to GET run";
  }

  // Retreive the final anomaly score from the TM object, 'tm.anomaly'.
  snprintf(message, sizeof(message), "/network/%s/region/tm/output/anomaly", id.c_str());
  res = client.Get(message);
  ASSERT_TRUE(res && res->status / 100 == 2) << " GET output message failed.";
  EXPECT_STREQ(trim(res->body).c_str(), "{type: \"Real32\",data: [1]}")
      << "Response to GET output request (The Anomaly Score)";
  res = client.Get("/stop"); // stop the server.

  threadObj.join(); // wait until server thread has stopped.
}
} // namespace testing