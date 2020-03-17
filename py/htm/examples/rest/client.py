#!/usr/bin/python
# ----------------------------------------------------------------------
# HTM Community Edition of NuPIC
# Copyright (C) 2020, Numenta, Inc.
#
# author: David Keeney, dkeeney@gmail.com, March 2020
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero Public License version 3 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU Affero Public License for more details.
#
# You should have received a copy of the GNU Affero Public License
# along with this program.  If not, see http://www.gnu.org/licenses.
# ----------------------------------------------------------------------
#
# USAGE:
#     client.py [host [port]]
#        host default: localhost
#        port default: 8050
#

import socket               # Import socket module
import sys, getopt

def send(self, message, body)
  

def main(argv):
  socket = socket.socket()    # Create a socket object
  host = socket.gethostname() # Get local machine name, the default.
  port = 8050                 # The default port
  if len(sys.argv) >= 2;
    host = sys.argv[1]
  if len(sys.argv) >= 3;
    port = sys.argv[2]



  socket.connect((host, port))
  
  s.sendall(msg)
print s.recv(1024)
s.close()    

if __name__ == "__main__":
   main(argv)
      