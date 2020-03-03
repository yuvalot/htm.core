//
//  Copyright (c) 2019 Yuji Hirose. All rights reserved.
//  MIT License
//

// Example taken from https://github.com/yhirose/cpp-httplib/blob/master/example/server.cc
// with minor mods to call NetworkAPI by David Keeney, Feb 2020

// This is written as an header-only library so that the server can be constructed
// as a stand-alone REST web server executable or incorporated in another program
// (such as a unit test) running under a separate thread.
//
// The key structure for the resources are as follows:
//   /network/<id>
//   /network/<id>/region/<name>
//   /network/<id>/region/<name>/param/<name>
//   /network/<id>/region/<name>/input/<name>
//   /network/<id>/region/<name>/output/<name>
//   /network/<id>/link/<name>
//
// The expected protocol for the NetworkAPI application is as follows:
//
//  POST /network?id=<previous id>
//       Create a new Network class object as a resource identified by id.
//       The id field is optional. If given a new network object will replace that id.
//       The body of the POST is JSON formatted configuration string
//       Returns a new id for the created resouce or an Error message.
//  PUT  /network/<id>/region/<region name/param/<param name>?data=<JSON encoded data>
//       Set the value of a region's parameter. The <data> could also be in the body.
//  GET  /network/<id>/region/<region name>/param/<param name>
//       Get the value of a region's parameter.
//  PUT  /network/<id>/region/<region name>/input/<input name>?data=<JSON encoded array>
//       Set the value of a region's input. The <data> could also be in the body.
//  GET  /network/<id>/region/<region name>/input/<input name>
//       Get the value of a region's input. Returns a JSON encoded array.
//  GET  /network/<id>/region/<region name>/output/<output name>
//       Get the value of a region's output. Returns a JSON encoded array.
//  GET  /network/<id>/run?iterations=<iterations>
//       Execute all regions in phase order. Repeat <iterations> times.
//  GET  /network/<id>/region/<region name>/command?data=<command>
//       Execute a predefined command on a region. <command> must start with the
//       command name followed by the arguments.
//       The data could also be in the body.
//
//  GET  /hi
//       Respond with "Hello World\n" as a way to check client to server connection.
//  GET  /stop
//       Stop the server.  All resources are released.


#include <chrono>
#include <cstdio>
#include <httplib.h>
#include <htm/engine/Network.hpp>
#include <htm/engine/RESTapi.hpp>

#define SERVER_CERT_FILE "./cert.pem"
#define SERVER_PRIVATE_KEY_FILE "./key.pem"
#define DEFAULT_PORT 8050  // default port to listen on
#define DEFAULT_INTERFACE "127.0.0.1"

using namespace httplib;
using namespace htm;

class RESTserver {


public:
  RESTserver() {
    
    /*** Register all of the handlers ***/

    //  GET  /hi    ==>  "Hello World!\n"
    svr.Get("/hi", [](const Request & /*req*/, Response &res) { 
      res.set_content("Hello World!\n", "text/plain"); 
      });
    if (!svr.is_valid()) {
      NTA_THROW << "server could not be created...\n";
    }


    //  POST /network?id=<previous id>
    //  Configure a Network resource (with JSON configuration in POST body) ==> token
    //  Note: the id parameter is optional.  If given it will re-use (replace) a previous id.
    svr.Post("/network", [&](const Request &req, Response &res) {
      std::string id;
      auto itr = req.params.find("id");
      if (itr != req.params.end())
        id = itr->second;
      RESTapi *interface = RESTapi::getInstance();
      std::string token = interface->create_network_request(id, req.body);
      res.set_content(token+"\n", "text/plain");
    });
    
    //  PUT  /network/<id>/region/<region name>/param/<param name>?data=<JSON encoded data>
    // Set the value of a ReadWrite Parameter on a Region.  Alternatively, the data could be in the body.
    svr.Put("/network/.*/region/.*/param/.*", [&](const Request &req, Response &res) {
      std::vector<std::string> flds = split(req.path, '/');
      std::string id = flds[2];
      std::string region_name = flds[4];
      std::string param_name = flds[6];
      std::string data = res.body;
      auto ix = req.params.find("data");
      if (ix != req.params.end())
        data = ix->second;
      
      RESTapi *interface = RESTapi::getInstance();
      std::string result = interface->put_param_request(id, region_name, param_name, data);
      res.set_content(result + "\n", "text/plain");
    });

    //  GET  /network/<id>/region/<region name>/param/<param name>
    //     Get the value of a Parameter on a Region.
    svr.Get("/network/.*/region/.*/param/.*", [](const Request &req, Response &res) {
      std::vector<std::string> flds = split(req.path, '/');
      std::string id = flds[2];
      std::string region_name = flds[4];
      std::string param_name = flds[6];

      RESTapi *interface = RESTapi::getInstance();
      std::string result = interface->get_param_request(id, region_name, param_name);
      res.set_content(result + "\n", "text/plain");
    });

    //  PUT  /network/<id>/region/<region name>/input/<input name>?data=<url encoded JSON data>
    //       Set the value of a region's input. The <data> could also be in the body.
    //  The data is a JSON encoded Array object which includes the type specifier;
    svr.Put("/network/.*/region/.*/input/.*", [](const Request &req, Response &res) {
      std::vector<std::string> flds = split(req.path, '/');
      std::string id = flds[2];
      std::string region_name = flds[4];
      std::string input_name = flds[6];
      std::string data = res.body;
      auto ix = req.params.find("data");
      if (ix != req.params.end())
        data = ix->second;

      RESTapi *interface = RESTapi::getInstance();
      std::string result = interface->put_input_request(id, region_name, input_name, data);
      res.set_content(result + "\n", "text/plain");
    });

    //  GET  /network/<id>/region/<region name>/input/<input name>
    //       Get the value of a region's input. Returns a JSON ecoded Array object.
    svr.Get("/network/.*/region/.*/input/.*", [](const Request &req, Response &res) {
      std::vector<std::string> flds = split(req.path, '/');
      std::string id = flds[2];
      std::string region_name = flds[4];
      std::string input_name = flds[6];

      RESTapi *interface = RESTapi::getInstance();
      std::string result = interface->get_input_request(id, region_name, input_name);
      res.set_content(result + "\n", "text/plain");
    });

    //  GET  /network/<id>/region/<region name>/output/<output name>
    // Get a specific Output of a region. Returns a JSON encoded Array object.
    svr.Get("/network/.*/region/.*/output/.*", [](const Request &req, Response &res) {
      std::vector<std::string> flds = split(req.path, '/');
      std::string id = flds[2];
      std::string region_name = flds[4];
      std::string output_name = flds[6];

      RESTapi *interface = RESTapi::getInstance();
      std::string result = interface->get_output_request(id, region_name, output_name);
      res.set_content(result + "\n", "text/plain");
    });


    // GET /network/<id>/run?iterations=<iterations>
    //    Execute the NetworkAPI <iterations> times.
    //           iterations are optional; defaults to 1.
    svr.Get("/network/.*/run", [](const Request &req, Response &res) {
      std::vector<std::string> flds = split(req.path, '/');
      std::string id = flds[2];
      std::string iterations = "1";
      auto ix = req.params.find("iterations");
      if (ix != req.params.end())
        iterations = ix->second;

      RESTapi *interface = RESTapi::getInstance();
      std::string result = interface->run_request(id, iterations);
      res.set_content(result+"\n", "text/plain");
    });

    //  GET  /network/<id>/region/<region name>/command?data=<command>
    //       Execute a predefined command on a region. <command> must start with the
    //       command name followed by the arguments.
    //       The data could also be in the body.
    svr.Get("/network/.*/region/.*/command", [](const Request &req, Response &res) {
      std::vector<std::string> flds = split(req.path, '/');
      std::string id = flds[2];
      std::string region_name = flds[4];
      std::string command = req.body;
      auto ix = req.params.find("data");
      if (ix != req.params.end())
        command = ix->second;

      RESTapi *interface = RESTapi::getInstance();
      std::string result = interface->command_request(id, region_name, command);
      res.set_content(result + "\n", "text/plain");
    });



    //  GET /stop  
    //    Halt the server.
    svr.Get("/stop",  [&](const Request & /*req*/, Response & /*res*/) { svr.stop(); });


    // What to do if there is an error in the server.
    svr.set_error_handler([](const Request & /*req*/, Response &res) {
      const char *fmt = "ERROR Status: %d";
      char buf[BUFSIZ];
      snprintf(buf, sizeof(buf), fmt, res.status);
      res.set_content(buf, "text/html");
    });

  }


  // How to perform logging.
  inline void set_logger(httplib::Logger logger) {
      svr.set_logger(logger);
  }

  inline void listen(int port, std::string net_interface) {
      // Enter the server listen loop.
      svr.listen(net_interface.c_str(), port);
  }

private:
  #ifdef CPPHTTPLIB_OPENSSL_SUPPORT
    SSLServer svr(SERVER_CERT_FILE, SERVER_PRIVATE_KEY_FILE);
  #else
    Server svr;
  #endif
  
};
