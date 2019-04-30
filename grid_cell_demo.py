#!/usr/bin/python3
# Reproduction study of the (Kropff & Treves, 2008) grid cell model.
# Written by David McDougall, 2019
import argparse
import numpy as np
import random
import itertools
import math
import scipy.signal
import scipy.ndimage
import scipy.stats
from scipy.ndimage.filters import maximum_filter
import time
import matplotlib.pyplot as plt
import matplotlib as mpl
import cv2

from nupic.bindings.sdr import SDR, Metrics
from nupic.bindings.encoders import CoordinateEncoder, CoordinateEncoderParameters
from nupic.bindings.encoders import ScalarEncoder, ScalarEncoderParameters
from nupic.bindings.algorithms import ColumnPooler, NoTopology


class Environment(object):
  """
  Environment is a 2D square, in first quadrant with corner at origin.
  """
  def __init__(self, size):
    self.size     = size
    self.position = (size/2, size/2)
    self.speed    = 2.0 ** .5
    self.angle    = 0
    self.course   = []

  def in_bounds(self, position):
    x, y = position
    if True:
      # Circle arena
      radius = self.size / 2.
      hypot  = (x - radius) ** 2 + (y - radius) ** 2
      return hypot < radius ** 2
    else:
      # Square arena
      x_in = x >= 0 and x < self.size
      y_in = y >= 0 and y < self.size
      return x_in and y_in

  def move(self):
    max_rotation = 2 * math.pi / 20
    self.angle += random.uniform(-max_rotation, max_rotation)
    self.angle = (self.angle + 2 * math.pi) % (2 * math.pi)
    vx = self.speed * math.cos(self.angle)
    vy = self.speed * math.sin(self.angle)
    x, y = self.position
    new_position = (x + vx, y + vy)
    
    if self.in_bounds(new_position):
      self.position = new_position
      self.course.append(self.position)
    else:
      # On failure, recurse and try again.
      assert(self.in_bounds(self.position))
      self.angle = random.uniform(0, 2 * math.pi)
      self.move()

  def plot_course(self, show=True):
    plt.figure("Path")
    plt.ylim([0, self.size])
    plt.xlim([0, self.size])
    x, y = zip(*self.course)
    plt.plot(x, y, 'k-')
    if show:
      plt.show()


if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument('--train_time', type=int, default = 1000 * 1000,)
  parser.add_argument('--debug', action='store_true')
  args = parser.parse_args()

  # Setup
  env = Environment(size = 200)

  enc = CoordinateEncoderParameters()
  enc.numDimensions = 2
  enc.size          = 2500
  enc.activeBits    = 75
  # enc.radius        = 5  # TODO Radius unimplemented
  enc.resolution    = 1 # TODO: Find an equivalent for radius 5
  enc = CoordinateEncoder(enc)
  assert(enc.parameters.activeBits > 50)

  # Head direction cells
  hd = ScalarEncoderParameters()
  hd.minimum    = 0
  hd.maximum    = 2 * math.pi
  hd.periodic   = True
  hd.activeBits = 20
  hd.radius     = (2 * math.pi) / 6
  hd = ScalarEncoder( hd )

  gcm = ColumnPooler.defaultParameters
  gcm.stabilityRate               = 1 - 0.05
  gcm.fatigueRate                 = 0.05 / 3
  gcm.proximalInputDimensions     = (enc.size,)
  gcm.inhibitionDimensions        = (1,)
  gcm.cellsPerInhibitionArea      = 500 if not args.debug else 50
  gcm.minSparsity                 = .05
  gcm.maxDepolarizedSparsity      = .2
  gcm.maxBurstSparsity            = .3
  gcm.proximalSynapseThreshold    = 0.25
  gcm.proximalIncrement           = 0.005
  gcm.proximalDecrement           = 0.0008
  gcm.proximalSegments            = 1
  gcm.proximalSegmentThreshold    = 0
  gcm.period                      = 10000
  gcm.potentialPool               = NoTopology( 0.95 )
  gcm.verbose                     = True
  gcm.distalInputDimensions       = [hd.size + gcm.cellsPerInhibitionArea]
  gcm.distalMaxSegments           =  32
  gcm.distalMaxSynapsesPerSegment =  64
  gcm.distalAddSynapses           =  27
  gcm.distalSegmentThreshold      =  18
  gcm.distalSegmentMatch          =  5 # 11
  gcm.distalSynapseThreshold      =  .5
  gcm.distalInitialPermanence     =  .49 # .3
  gcm.distalIncrement             =  .01
  gcm.distalDecrement             =  .01
  gcm.distalMispredictDecrement   =  .001
  gcm = ColumnPooler(gcm)

  enc_sdr = enc.encode(env.position)
  hd_act  = hd.encode( env.angle )
  def compute(learn=True):
    enc.encode( env.position, enc_sdr )
    hd.encode(  env.angle,    hd_act )
    distalActive = SDR(gcm.parameters.distalInputDimensions).concatenate(hd_act, gcm.activeCells.flatten())
    distalWinner = SDR(gcm.parameters.distalInputDimensions).concatenate(hd_act, gcm.winnerCells.flatten())
    gcm.compute(
            proximalInputActive   = enc_sdr,
            distalInputActive     = distalActive,
            distalInputLearning   = distalWinner,
            learn                 = learn)


  print("Training for %d cycles ..."%args.train_time)
  gcm.reset()
  gc_metrics = Metrics(gcm.cellDimensions, args.train_time)
  for step in range(args.train_time):
    if step % 10000 == 0:
      print("Cycle %d"%step)
    env.move()
    compute()
    gc_metrics.addData( gcm.activeCells )
  print("Grid Cell Metrics", str(gc_metrics))

  with open("grid_cell_prox.connections", "wb") as f:
      f.write( gcm.proximalConnections.save() )
  with open("grid_cell_distal.connections", "wb") as f:
      f.write( gcm.distalConnections.save() )

  print("Testing ...")
  enc_num_samples = 12
  gc_num_samples  = 20

  # Show how the agent traversed the environment.
  if args.train_time <= 100000:
    env.plot_course(show=False)
  else:
    print("Not going to plot course, too long.")

  # Measure Grid Cell Receptive Fields.
  enc_samples = random.sample(range(enc.parameters.size), enc_num_samples)
  gc_samples  = list(range(np.product(gcm.cellDimensions)))
  enc_rfs = [np.zeros((env.size, env.size)) for idx in enc_samples]
  gc_rfs  = [np.zeros((env.size, env.size)) for idx in gc_samples]
  for position in itertools.product(range(env.size), range(env.size)):
    if not env.in_bounds(position):
      continue
    env.position = position
    gcm.reset()
    compute(learn=False)
    for rf_idx, enc_idx in enumerate(enc_samples):
      enc_rfs[rf_idx][position] = enc_sdr.dense[enc_idx]
    for rf_idx, gc_idx in enumerate(gc_samples):
      gc_rfs[rf_idx][position] = gcm.activeCells.flatten().dense[gc_idx]

  # Look for the grid properties.
  xcor     = []
  gridness = []
  zoom = .25 if args.debug else .5
  dim  = int(env.size * 2 * zoom - 1)
  circleMask = cv2.circle( np.zeros([dim,dim]), (dim//2,dim//2), dim//2, [1], -1)
  for rf in gc_rfs:
    # Shrink images before the expensive auto correlation.
    rf = scipy.ndimage.zoom(rf, zoom, order=1)
    X  = scipy.signal.correlate2d(rf, rf)
    # Crop to a circle.
    X *= circleMask
    xcor.append( X )
    # Correlate the xcor with a rotation of itself.
    rot_cor = {}
    for angle in (30, 60, 90, 120, 150):
      rot = scipy.ndimage.rotate( X, angle, order=1, reshape=False )
      r, p = scipy.stats.pearsonr( X.reshape(-1), rot.reshape(-1) )
      rot_cor[ angle ] = r
    gridness.append( + (rot_cor[60] + rot_cor[120]) / 2.
                     - (rot_cor[30] + rot_cor[90] + rot_cor[150]) / 3.)

  # Show Histogram of gridness scores.
  plt.figure("Histogram of Gridness Scores")
  plt.hist( gridness, bins=28, range=[-.3, 1.1] )
  plt.ylabel("Number of cells")
  plt.xlabel("Gridness score")

  # Show locations of the first 3 maxima of each x-correlation.
  plt.figure("Spacing & Orientation")
  x_coords = []
  y_coords = []
  for X in xcor:
    half   = X[ : , : dim//2 + 1 ]
    maxima = maximum_filter( half, size=10 ) == half
    maxima = np.logical_and( maxima, half > 0 ) # Filter out areas which are all zeros.
    maxima = np.nonzero( maxima )
    for idx in np.argsort(-half[ maxima ])[ 1 : 4 ]:
      x_coords.append( maxima[0][idx] )
      y_coords.append( maxima[1][idx] )
  # Fix coordinate scale & offset.
  x_coords = (np.array(x_coords) - dim/2 + .5) / zoom
  y_coords = (dim/2 - np.array(y_coords) - .5) / zoom
  # Replace duplicate points with larger points in the image.
  coords    = list(zip(x_coords, y_coords))
  coord_set = list(set(coords))
  x_coords, y_coords = zip(*coord_set)
  defaultDotSz = mpl.rcParams['lines.markersize'] ** 2
  scales = [coords.count(c) * defaultDotSz for c in coord_set]
  plt.scatter( x_coords, y_coords, scales)

  # Select some interesting cells to display.
  if True:
      # Show the best & worst grid cells.
      argsort = np.argsort(gridness)
      gc_samples = list( argsort[ : gc_num_samples // 2] )
      gc_samples.extend( argsort[ -(gc_num_samples - len(gc_samples)) : ])
  elif False:
      # Show the best grid cells.
      gc_samples = np.argsort(gridness)[ -gc_num_samples : ]
  else:
      #  Show random sample of grid cells.
      gc_samples = random.sample(range(len(gc_rfs)), gc_num_samples)
  gc_rfs   = [ gc_rfs[idx]   for idx in gc_samples ]
  xcor     = [ xcor[idx]     for idx in gc_samples ]
  gridness = [ gridness[idx] for idx in gc_samples ]

  # Show the Input/Encoder Receptive Fields.
  if enc_num_samples > 0:
    plt.figure("Input Receptive Fields")
    nrows = int(enc_num_samples ** .5)
    ncols = math.ceil((enc_num_samples+.0) / nrows)
    for subplot_idx, rf in enumerate(enc_rfs):
      plt.subplot(nrows, ncols, subplot_idx + 1)
      plt.imshow(rf, interpolation='nearest')

  # Show the Grid Cells Receptive Fields.
  if gc_num_samples > 0:
    plt.figure("Grid Cell Receptive Fields")
    nrows = int(gc_num_samples ** .5)
    ncols = math.ceil((gc_num_samples+.0) / nrows)
    for subplot_idx, rf in enumerate(gc_rfs):
      plt.subplot(nrows, ncols, subplot_idx + 1)
      plt.title("Gridness score %g"%gridness[subplot_idx])
      plt.imshow(rf, interpolation='nearest')

    # Show the autocorrelations of the grid cell receptive fields.
    plt.figure("Grid Cell RF Autocorrelations")
    for subplot_idx, X in enumerate(xcor):
      plt.subplot(nrows, ncols, subplot_idx + 1)
      plt.title("Gridness score %g"%gridness[subplot_idx])
      plt.imshow(X, interpolation='nearest')

  plt.show()
