import sys
import requests
import subprocess
import unittest
import os
from time import sleep
import math

REPO_DIR = os.path.dirname(
  os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.realpath(__file__)))))

REST_SERVER = os.path.join(REPO_DIR, "build", "Release", "bin", "rest_server")

REST_EXAMPLE_DIR = os.path.join(REPO_DIR, "py", "htm", "examples", "rest")

sys.path.append(REST_EXAMPLE_DIR)

from htm_rest_api import NetworkREST, NetworkRESTBase

HOST = 'http://127.0.0.1:8050'

EPOCHS = 3


class HtmRestApiTest(unittest.TestCase):
  def setUp(self):
    self._process = subprocess.Popen([REST_SERVER, '8050', '127.0.0.1'])
    sleep(0.2)

  def testNetworkRESTBaseExample(self):
    rsp = requests.get(HOST + '/hi')
    self.assertEqual(rsp.status_code, requests.codes.ok)
    self.assertEqual(rsp.text, 'Hello World!\n')

    config = '''
        {network: [
            {addRegion: {name: "encoder", type: "RDSEEncoderRegion", params: {size: 1000, sparsity: 0.2, radius: 0.03, seed: 2019, noise: 0.01}}},
            {addRegion: {name: "sp", type: "SPRegion", params: {columnCount: 2048, globalInhibition: true}}},
            {addRegion: {name: "tm", type: "TMRegion", params: {cellsPerColumn: 8, orColumnOutputs: true}}},
            {addLink:   {src: "encoder.encoded", dest: "sp.bottomUpIn"}},
            {addLink:   {src: "sp.bottomUpOut", dest: "tm.bottomUpIn"}}
        ]}'''

    net = NetworkRESTBase(config, host=HOST, verbose=True)

    net.create()

    r = net.get_region_param('tm', 'cellsPerColumn')
    self.assertEqual(r, '8')
    # iterate EPOCHS times
    x = 0.00
    for e in range(EPOCHS):
      # Data is a sine wave,    (Note: first iteration is for x=0.01, not 0)
      x += 0.01  # advance one step, 0.01 radians
      s = math.sin(x)

      # Send set parameter message to feed "sensedValue" parameter data into RDSE encoder for this iteration.
      r = net.put_region_param('encoder', 'sensedValue', '{:.2f}'.format(s))
      self.assertEqual(r, 'OK')

      # Execute one iteration of the Network object
      r = net.run(1)
      self.assertEqual(r, 'OK')

    # Retreive the final anomaly score from the TM object's 'anomaly' output.
    score = net.get_region_output('tm', 'anomaly')
    self.assertEqual(score, [1])

    print('Anomaly Score: ' + str(score))
    #Note: Anomaly score will be 1 until there have been enough iterations to 'learn' the pattern.

  def testNetworkRESTBaseDelete(self):
    config = '''
        {network: [
            {addRegion: {name: "encoder", type: "RDSEEncoderRegion", params: {size: 1000, sparsity: 0.2, radius: 0.03, seed: 2019, noise: 0.01}}},
            {addRegion: {name: "sp", type: "SPRegion", params: {columnCount: 2048, globalInhibition: true}}},
            {addRegion: {name: "tm", type: "TMRegion", params: {cellsPerColumn: 8, orColumnOutputs: true}}},
            {addLink:   {src: "encoder.encoded", dest: "sp.bottomUpIn"}},
            {addLink:   {src: "sp.bottomUpOut", dest: "tm.bottomUpIn"}}
        ]}'''

    net = NetworkRESTBase(config, host=HOST, verbose=True)

    net.create()

    # Now delete the second link.
    r = net.delete_link('sp.bottomUpOut', 'tm.bottomUpIn')
    self.assertEqual(r, 'OK')

    # Delete a region
    r = net.delete_region('tm')
    self.assertEqual(r, 'OK')

    # Delete a Network
    r = net.delete_all()
    self.assertEqual(r, 'OK')

  def testNetworkRESTExample(self):
    rsp = requests.get(HOST + '/hi')
    self.assertEqual(rsp.status_code, requests.codes.ok)
    self.assertEqual(rsp.text, 'Hello World!\n')

    net = NetworkREST(host=HOST, verbose=True)

    encoder = net.add_region('encoder', 'RDSEEncoderRegion', {
      'size': 1000,
      'sparsity': 0.2,
      'radius': 0.03,
      'seed': 2019,
      'noise': 0.01
    })

    sp = net.add_region('sp', 'SPRegion', {
      'columnCount': 2048,
      'globalInhibition': True
    })

    tm = net.add_region('tm', 'TMRegion', {
      'cellsPerColumn': 8,
      'orColumnOutputs': True
    })

    link1 = net.add_link(encoder, sp, 'encoded', 'bottomUpIn')
    link2 = net.add_link(sp, tm, 'bottomUpOut', 'bottomUpIn')

    net.create()

    r = tm.param('cellsPerColumn')
    self.assertEqual(r, '8')
    # iterate EPOCHS times
    x = 0.00
    for e in range(EPOCHS):
      # Data is a sine wave,    (Note: first iteration is for x=0.01, not 0)
      x += 0.01  # advance one step, 0.01 radians
      s = math.sin(x)

      # Send set parameter message to feed "sensedValue" parameter data into RDSE encoder for this iteration.
      r = encoder.param('sensedValue', '{:.2f}'.format(s))
      self.assertEqual(r, 'OK')

      # Execute one iteration of the Network object
      r = net.run(1)
      self.assertEqual(r, 'OK')

    # Retreive the final anomaly score from the TM object's 'anomaly' output.
    score = tm.output('anomaly')
    self.assertEqual(score, [1])

    print('Anomaly Score: ' + str(score))
    #Note: Anomaly score will be 1 until there have been enough iterations to 'learn' the pattern.

  def testNetworkRESTDelete(self):
    net = NetworkREST(host=HOST, verbose=True)

    encoder = net.add_region('encoder', 'RDSEEncoderRegion', {
      'size': 1000,
      'sparsity': 0.2,
      'radius': 0.03,
      'seed': 2019,
      'noise': 0.01
    })

    sp = net.add_region('sp', 'SPRegion', {
      'columnCount': 2048,
      'globalInhibition': True
    })

    tm = net.add_region('tm', 'TMRegion', {
      'cellsPerColumn': 8,
      'orColumnOutputs': True
    })

    link1 = net.add_link(encoder, sp, 'encoded', 'bottomUpIn')
    link2 = net.add_link(sp, tm, 'bottomUpOut', 'bottomUpIn')

    net.create()

    # Now delete the second link.
    r = link2.destory()
    self.assertEqual(r, 'OK')

    # Delete a region
    r = tm.destory()
    self.assertEqual(r, 'OK')

    # Delete a Network
    r = net.delete_all()
    self.assertEqual(r, 'OK')

  def tearDown(self):
    try:
      r = requests.get('{}/stop'.format(HOST))
    except:
      pass
    sleep(0.01)
    self._process.terminate()
    sleep(0.1)

    self._process.wait(1)


if __name__ == "__main__":
  unittest.main()
