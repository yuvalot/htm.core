
""" Unit tests for retina encoder. """

import unittest
import os
import numpy as np

from htm.encoders.eye import Eye

class EyeEncoderTest(unittest.TestCase):
  """ Unit tests for Eye encoder class. """

  def testBasicUsage(self):
    eye = Eye()
    eye.reset()
    FILE=os.path.join('py','tests','encoders','ronja_the_cat.jpg')
    eye.new_image(FILE)
    eye.scale = 0.5
    eye.center_view()
    for _ in range(10):
      pos,rot,sc = eye.small_random_movement()
      (sdrParvo, sdrMagno) = eye.compute(pos,rot,sc)
      try:
        eye.plot(500)
      except: 
        pass
    print("Sparsity parvo: {}".format(len(eye.parvo_sdr.sparse)/np.product(eye.parvo_sdr.dimensions)))
    print("Sparsity magno: {}".format(len(eye.magno_sdr.sparse)/np.product(eye.magno_sdr.dimensions)))

  
