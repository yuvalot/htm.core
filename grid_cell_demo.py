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


default_parameters = {
  'gc' : {
    'stabilityRate'            : 1 - 0.05,
    'fatigueRate'              : 0.05 / 3,
    'proximalSynapseThreshold' : 0.25,
    'proximalIncrement'        : 0.005,
    'proximalDecrement'        : 0.0008,
    'proximalSegmentThreshold' : 0,
    'proximalSegments'         : 1,
    'period'                   : 10000,
    'cellsPerInhibitionArea'    : 500,
    'minSparsity'               : .05,
    'maxDepolarizedSparsity'    : .1,
    'maxBurstSparsity'          : .3,
    "potentialPool"             : 0.95
  }
}


def main(parameters=default_parameters, argv=None, verbose=True):

  parser = argparse.ArgumentParser()
  parser.add_argument('--train_time', type=int, default = 1000 * 1000,)
  parser.add_argument('--debug', action='store_true')
  args = parser.parse_args(argv)

  # Setup
  env = Environment(size = 200)

  enc = CoordinateEncoderParameters()
  enc.numDimensions = 2
  enc.size          = 2500
  enc.activeBits    = 75
  # enc.radius        = 5  # TODO Radius unimplemented
  enc.resolution    = 1
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
  for param, value in parameters['gc'].items():
    if param not in ['potentialPool']:
      setattr( gcm, param, value )
  gcm.proximalInputDimensions     = (enc.size,)
  gcm.inhibitionDimensions        = (1,)
  gcm.potentialPool               = NoTopology( parameters['gc']['potentialPool'] )
  gcm.verbose                     = True
  gcm.proximalMinConnections      = 0.0
  gcm.proximalMaxConnections      = 1.0
  gcm.distalInputDimensions       = [hd.size + gcm.cellsPerInhibitionArea]
  gcm.distalMaxSegments           = 0 # 32
  gcm.distalMaxSynapsesPerSegment = 0 # 64
  gcm.distalAddSynapses           = 0 # 27
  gcm.distalSegmentThreshold      = 0 # 18
  gcm.distalSegmentMatch          = 0 # 11
  gcm.distalSynapseThreshold      = 0 # .5
  gcm.distalInitialPermanence     = 0 # .3
  gcm.distalIncrement             = 0 # .01
  gcm.distalDecrement             = 0 # .01
  gcm.distalMispredictDecrement   = 0 # .001
  gcm = ColumnPooler(gcm)

  enc_sdr = enc.encode(env.position)
  hd_act  = hd.encode( env.angle )
  anomaly = []
  def compute(learn=True):
    enc.encode( env.position, enc_sdr )
    hd.encode(  env.angle,    hd_act )
    # DISABLED DISTAL DENDRITES!
    distalActive = SDR(gcm.parameters.distalInputDimensions) # .concatenate(gcm.activeCells.flatten(), hd_act)
    distalWinner = SDR(gcm.parameters.distalInputDimensions) # .concatenate(gcm.winnerCells.flatten(), hd_act)
    gcm.compute(
            proximalInputActive   = enc_sdr,
            distalInputActive     = distalActive,
            distalInputLearning   = distalWinner,
            learn                 = learn)
    anomaly.append( gcm.rawAnomaly )

  print("Learning for %d cycles ..."%args.train_time)
  gcm.reset()
  gc_metrics = Metrics(gcm.cellDimensions, args.train_time)
  for step in range(args.train_time):
    if step % 10000 == 0:
      print("Cycle %d"%step)
    env.move()
    compute()
    gc_metrics.addData( gcm.activeCells )
  print("Grid Cell Metrics", str(gc_metrics))

  print()
  print("Proximal", gcm.proximalConnections)
  with open("grid_cell_prox.connections", "wb") as f:
      f.write( gcm.proximalConnections.save() )

  print()
  print("Distal", gcm.distalConnections)
  with open("grid_cell_distal.connections", "wb") as f:
      f.write( gcm.distalConnections.save() )

  print("Anomaly Mean", np.mean(anomaly), "std", np.std(anomaly))

  print("Evaluating Grid Cells ...")
  enc_num_samples = 12
  gc_num_samples  = 20

  # Show how the agent traversed the environment.
  if args.train_time <= 100000:
    env.plot_course(show=False)
  else:
    print("Not going to plot course, too long.")

  # Measure Grid Cell Receptive Fields.
  enc_samples = random.sample(range(enc.parameters.size), enc_num_samples)
  enc_rfs     = [np.zeros((env.size, env.size)) for idx in enc_samples]
  gc_rfs      = [np.zeros((env.size, env.size)) for idx in range(gcm.size)]
  for position in itertools.product(range(env.size), range(env.size)):
    if not env.in_bounds(position):
      continue
    env.position = position
    gcm.reset()
    compute(learn=False)
    for rf_idx, enc_idx in enumerate(enc_samples):
      enc_rfs[rf_idx][position] = enc_sdr.dense[enc_idx]
    for gc_idx in range(gcm.size):
      gc_rfs[gc_idx][position] = gcm.activeCells.flatten().dense[gc_idx]

  # Look for the grid properties.
  xcor      = []
  gridness  = []
  alignment = []
  zoom = .25 if args.debug else .5
  dim  = int(env.size * 2 * zoom - 1) # Dimension of the X-Correlation.
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
    # Find alignment points, the local maxima in the x-correlation.
    half   = X[ : , : dim//2 + 1 ]
    maxima = maximum_filter( half, size=10 ) == half
    maxima = np.logical_and( maxima, half > 0 ) # Filter out areas which are all zeros.
    maxima = np.nonzero( maxima )
    for idx in np.argsort(-half[ maxima ])[ 1 : 4 ]:
      x_coord, y_coord = maxima[0][idx], maxima[1][idx]
      # Fix coordinate scale & offset.
      x_coord = (x_coord - dim/2 + .5) / zoom
      y_coord = (dim/2 - y_coord - .5) / zoom
      alignment.append( ( x_coord, y_coord ) )

  if verbose:
    import matplotlib.pyplot as plt
    import matplotlib as mpl
    # Show the Input/Encoder Receptive Fields.
    if len(enc_rfs) > 0:
      plt.figure("Input Receptive Fields")
      nrows = int(len(enc_rfs) ** .5)
      ncols = math.ceil((len(enc_rfs)+.0) / nrows)
      for subplot_idx, rf in enumerate(enc_rfs):
        plt.subplot(nrows, ncols, subplot_idx + 1)
        plt.imshow(rf, interpolation='nearest')

    # Show Histogram of gridness scores.
    plt.figure("Histogram of Gridness Scores")
    plt.hist( gridness, bins=28, range=[-.3, 1.1] )
    plt.ylabel("Number of cells")
    plt.xlabel("Gridness score")

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
    gc_rfs    = [ gc_rfs[idx]    for idx in gc_samples ]
    xcor      = [ xcor[idx]      for idx in gc_samples ]
    gridness  = [ gridness[idx]  for idx in gc_samples ]
    alignment = [ alignment[idx] for idx in gc_samples ]

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

      # Show locations of the first 3 maxima of each x-correlation.
      plt.figure("Spacing & Orientation")
      # Replace duplicate points with larger points in the image.
      coord_set = list(set(alignment))
      x_coords, y_coords = zip(*coord_set)
      defaultDotSz = mpl.rcParams['lines.markersize'] ** 2
      scales = [alignment.count(c) * defaultDotSz for c in coord_set]
      plt.scatter( x_coords, y_coords, scales)

    plt.show()

  # Score is the average of the top 5% of gridness scores.
  gridness.sort()
  score = np.mean(gridness[ -int(round(len(gridness) * .05)) :])
  print("Score:", score)
  return score

if __name__ == "__main__":
  main()
