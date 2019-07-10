# ----------------------------------------------------------------------
# Numenta Platform for Intelligent Computing (NuPIC)
# Copyright (C) 2019, David McDougall
#
# The following terms and conditions apply:
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

import unittest
import argparse
import numpy as np
import random
import itertools
import math
import scipy.signal
import time

from htm import SDR, Metrics
from htm.encoders.rdse import RDSE, RDSE_Parameters
from htm.algorithms import ColumnPooler, NoTopology

class columnPoolerTest(unittest.TestCase):
    def testGrids1D(self):
      env_size = 1000

      enc = RDSE_Parameters()
      enc.size          = 2500
      enc.activeBits    = 75
      enc.radius        = 1
      enc = RDSE(enc)

      gcm = ColumnPooler.defaultParameters
      gcm.stabilityRate               = 1 - 0.05
      gcm.fatigueRate                 = 0.05 / 3
      gcm.proximalInputDimensions     = (enc.size,)
      gcm.inhibitionDimensions        = (1,)
      gcm.cellsPerInhibitionArea      = 200
      gcm.proximalSynapseThreshold    = 0.25
      gcm.sparsity                    = .2
      gcm.proximalIncrement           = 0.005
      gcm.proximalDecrement           = 0.0008
      gcm.proximalSegments            = 1
      gcm.proximalSegmentThreshold    = 10
      gcm.period                      = env_size
      gcm.potentialPool               = NoTopology( 0.95 )
      gcm.verbose                     = True
      gcm.distalAddSynapses           = 0
      gcm.distalInputDimensions       = (1,)
      gcm = ColumnPooler(gcm)

      gc_metrics = Metrics(gcm.cellDimensions, env_size)
      gcm.reset()
      for position in range( env_size ):
        gc_sdr = SDR( gcm.cellDimensions )
        gcm.compute( enc.encode(position), True, gc_sdr)
        gc_metrics.addData( gc_sdr )
      print("Grid Cell Metrics", str(gc_metrics))

      assert( gc_metrics.activationFrequency.min() > .20 *  .50 )
      assert( gc_metrics.activationFrequency.max() < .20 * 1.50 )
      assert( gc_metrics.overlap.min()  > .45 )
      assert( gc_metrics.overlap.max()  < .95 )

      assert( gc_metrics.overlap.mean() > .70 )
      assert( gc_metrics.overlap.mean() < .80 )
      assert( gc_metrics.overlap.std()  < .10 )
