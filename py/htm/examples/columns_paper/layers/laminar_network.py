# ----------------------------------------------------------------------
# Numenta Platform for Intelligent Computing (NuPIC)
# Copyright (C) 2016, Numenta, Inc.  Unless you have an agreement
# with Numenta, Inc., for a separate license for this software code, the
# following terms and conditions apply:
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
#
# http://numenta.org/licenses/
# ----------------------------------------------------------------------
"""
Overall factory methods to create L2L4 networks of multiple layers and for
experimenting with different laminar structures.
"""
from htm.bindings.engine_internal import Network
from htm.examples.columns_paper.layers.l2_l4_network_creation import (
  createL4L2Column, createMultipleL4L2Columns)


def createNetwork(networkConfig):
  """
  Create and initialize the specified network instance.

  @param networkConfig: (dict) the configuration of this network.
  @return network: (Network) The actual network
  """

  network = Network()

  if networkConfig["networkType"] == "L4L2Column":
    return createL4L2Column(network, networkConfig, "_0")
  elif networkConfig["networkType"] == "MultipleL4L2Columns":
    return createMultipleL4L2Columns(network, networkConfig)


def printNetwork(network):
  """
  Given a network, print out regions sorted by phase
  """
  print("The network has",len(list(network.regions.values())),"regions")
  for p in range(network.getMaxPhase()):
    print("=== Phase",p)
    for region in list(network.regions.values()):
      if network.getPhases(region.name)[0] == p:
        print("   ",region.name)
