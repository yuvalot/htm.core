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
import matplotlib.pyplot as plt
import matplotlib as mpl

from htm import SDR, Metrics
from htm.bindings.encoders import CoordinateEncoder, CoordinateEncoderParameters
from htm.bindings.encoders import ScalarEncoder, ScalarEncoderParameters
from htm.bindings.algorithms import ColumnPooler, NoTopology


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


default_parameters = {'gc': {
        'fatigueRate': 0.02479904687946749,
        'sparsity': 0.16159121243882169,
        'period': 15452,
        'potentialPool': 0.9653887215973943,
        'proximalDecrement': 0.00041767455516643185,
        'proximalIncrement': 0.0063514371595179385,
        'proximalSegmentThreshold': 4,
        'proximalSegments': 3,
        'proximalSynapseThreshold': 0.2665465883854693,
        'stabilityRate': 0.9599504061401867,
        'proximalMinConnections': 0,
        'proximalMaxConnections': 1,
        'distalMaxSegments': 32,
        'distalMaxSynapsesPerSegment': 64,
        'distalSegmentThreshold': 18,
        'distalSegmentMatch': 11,
        'distalAddSynapses': 27,
        'distalInitialPermanence': .45,
        'distalIncrement': .01,
        'distalDecrement': .01,
        'distalMispredictDecrement': .001,
        'distalSynapseThreshold': .5,
}}


def main(parameters=default_parameters, argv=None, verbose=True):

  parser = argparse.ArgumentParser()
  parser.add_argument('--steps', type=int, default = 1000 * 1000,)
  parser.add_argument('--debug', action='store_true')
  args = parser.parse_args(argv)

  # Hold these parameters constant from swarming.
  parameters['gc'].update({
    'cellsPerInhibitionArea' : 100 if args.debug else 500,
  })

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
  gcm.distalInputDimensions       = [hd.size + gcm.cellsPerInhibitionArea]
  gcm = ColumnPooler(gcm)

  enc_sdr = enc.encode(env.position)
  hd_act  = hd.encode( env.angle )
  anomaly = []
  def compute(learn=True):
    enc.encode( env.position, enc_sdr )
    hd.encode(  env.angle,    hd_act )
    distalActive = SDR(gcm.parameters.distalInputDimensions).concatenate(SDR(gcm.activeCells).flatten(), hd_act)
    gcm.compute(
            proximalInputActive   = enc_sdr,
            distalInputActive     = distalActive,
            learn                 = learn)
    anomaly.append( gcm.rawAnomaly )

  print("Learning for %d steps ..."%args.steps)
  gcm.reset()
  pc_metrics = Metrics(enc.dimensions, args.steps)
  gc_metrics = Metrics(gcm.cellDimensions, args.steps)
  for step in range(args.steps):
    if verbose and step % 10000 == 0:
      print("Step %d"%step)
    env.move()
    compute()
    pc_metrics.addData( enc_sdr )
    gc_metrics.addData( gcm.activeCells )
  print("Place Cell Metrics", str(pc_metrics))
  print("")
  print("Grid Cell Metrics", str(gc_metrics))
  print("")
  print("Proximal", gcm.proximalConnections)
  print("")
  print("Distal", gcm.distalConnections)
  print("")
  print("Anomaly Mean", np.mean(anomaly), "std", np.std(anomaly))
  print("")
  if verbose:
      with open("grid_cell_prox.connections", "wb") as f:
          f.write( gcm.proximalConnections.save() )
      with open("grid_cell_distal.connections", "wb") as f:
          f.write( gcm.distalConnections.save() )

  # Show how the agent traversed the environment.
  if args.steps <= 100000:
    env.plot_course(show=False)
  else:
    print("Not going to plot course, too long.")

  # Measure Grid Cell Receptive Fields.
  print("Evaluating Grid Cells ...")
  enc_samples = random.sample(range(enc.parameters.size), 12 if verbose else 0)
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
      gc_rfs[gc_idx][position] = SDR(gcm.activeCells).dense[0, gc_idx]

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
    align_pts = []
    for idx in np.argsort(-half[ maxima ])[ 1 : 4 ]:
      x_coord, y_coord = maxima[0][idx], maxima[1][idx]
      # Fix coordinate scale & offset.
      x_coord = (x_coord - dim/2 + .5) / zoom
      y_coord = (dim/2 - y_coord - .5) / zoom
      align_pts.append( ( x_coord, y_coord ) )
    alignment.append(align_pts)

  # Analyze the best examples of grid cells, by gridness scores.
  if verbose:
    # Select exactly 20 cells to display.
    gc_num_samples = 20
  else:
    # Top 20% of grid cells.
    gc_num_samples = int(round(len(gridness) * .20))
  gc_samples = np.argsort(gridness)[ -gc_num_samples : ]

  # Get the selected data.
  gridness_all = gridness[:] # Save all gridness scores for histogram.
  gc_samples   = sorted(gc_samples, key=lambda x: gridness[x])
  gc_rfs       = [ gc_rfs[idx]    for idx in gc_samples ]
  xcor         = [ xcor[idx]      for idx in gc_samples ]
  gridness     = [ gridness[idx]  for idx in gc_samples ]
  alignment    = [ alignment[idx] for idx in gc_samples ]
  score = np.mean(gridness)
  print("Score:", score)

  if verbose:
    # Show the Input/Encoder Receptive Fields.
    plt.figure("Input Receptive Fields")
    nrows = int(len(enc_rfs) ** .5)
    ncols = math.ceil((len(enc_rfs)+.0) / nrows)
    for subplot_idx, rf in enumerate(enc_rfs):
      plt.subplot(nrows, ncols, subplot_idx + 1)
      plt.imshow(rf, interpolation='nearest')

    # Show Histogram of gridness scores.
    plt.figure("Histogram of Gridness Scores")
    plt.hist( gridness_all, bins=28, range=[-.3, 1.1] )
    plt.ylabel("Number of cells")
    plt.xlabel("Gridness score")

    # Show the Grid Cells Receptive Fields.
    plt.figure("Grid Cell Receptive Fields")
    nrows = int(len(gc_rfs) ** .5)
    ncols = math.ceil((len(gc_rfs)+.0) / nrows)
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
    alignment_flat = [];
    [alignment_flat.extend(pts) for pts in alignment]
    # Replace duplicate points with larger points in the image.
    coord_set = list(set(alignment_flat))
    defaultDotSz = mpl.rcParams['lines.markersize'] ** 2
    scales = [alignment_flat.count(c) * defaultDotSz for c in coord_set]
    x_coords, y_coords = zip(*coord_set)
    if x_coords and y_coords:
      plt.scatter( x_coords, y_coords, scales )
    else:
      plt.scatter( [], [] )
      print("No alignment points found!")

    plt.show()
  return score

if __name__ == "__main__":
  main()
