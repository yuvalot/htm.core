# SYNOPSIS 

This is an example REST web server and client implemented using a header-only package 
from [`cpp-httplib`](https://github.com/yhirose/cpp-httplib). This can be built and ran 
as a REST server to instantiate a Network class, configure it and run it as a NetworkAPI 
application.

This is a light-weight server.  If you need a full production REST server we suggest 
replacing this with one implemented from Boost.Beast.

# FEATURES

* Handles REST interface for htm.core library
* HTTP 1.0 / 1.1
* http

# DEPENDENCIES
   htm.core library

# USAGE

To run the server, 
  ./server [port [network_interface]]
     port defaults to 8050
	 network_interface defaults to "127.0.0.1".
  
To run the client,
  ./client [ip_address [port]]
     ip_address defaults to "localhost".
	 port defaults to 8050
	 
	 
