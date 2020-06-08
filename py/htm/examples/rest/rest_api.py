from urllib.parse import urlencode
import requests
import json


class NetworkError(Exception):
  pass


def process_response(rsp):
  if rsp.status_code != requests.codes.ok:
    raise NetworkError('HTTP Error')

  text = rsp.text.strip()

  if text[0:6] == 'ERROR:':
    raise NetworkError(text[6:].strip())

  if text == 'OK':
    return text

  idx = text.find('[')

  if idx == -1:
    return text

  data = text[idx + 1:].replace(']}', '')

  return json.loads('[' + data + ']')


class Network(object):

  def __init__(self,
               config,
               id=None,
               host='http://127.0.0.1:8050',
               verbose=False):
    self.config = config
    self.id = id
    self.host = host
    self.verbose = verbose

  def api(self, uri, query=None):
    if query:
      return "{}/network{}?{}".format(self.host, uri, urlencode(query))

    return "{}/network{}".format(self.host, uri)

  def api1(self, uri, query=None):
    return self.api('/{}{}'.format(self.id, uri), query)

  def create(self):
    query = {}
    if self.id:
      query['id'] = self.id
    url = self.api('', query)
    if self.verbose:
      print('POST {}; body: {}'.format(url, self.config))
    rsp = requests.post(url, data=self.config)
    id = process_response(rsp)

    if self.verbose:
      print('Resource ID: ' + id)

    if self.id and self.id != id:
      raise NetworkError('Network id not match')
    else:
      self.id = id

  def put_region_param(self, region_name, param_name, data):
    url = self.api1('/region/{}/param/{}'.format(region_name, param_name),
                    {'data': data})
    if self.verbose:
      print('PUT {}'.format(url))
    rsp = requests.put(url)
    return process_response(rsp)

  def get_region_param(self, region_name, param_name):
    url = self.api1('/region/{}/param/{}'.format(region_name, param_name))
    if self.verbose:
      print('GET {}'.format(url))
    rsp = requests.get(url)
    return process_response(rsp)

  def put_region_input(self, region_name, input_name, data):
    url = self.api1('/region/{}/input/{}'.format(region_name, input_name),
                    {'data': data})
    if self.verbose:
      print('PUT {}'.format(url))
    rsp = requests.put(url)
    return process_response(rsp)

  def get_region_input(self, region_name, input_name):
    url = self.api1('/region/{}/input/{}'.format(region_name, input_name))
    if self.verbose:
      print('GET {}'.format(url))
    rsp = requests.get(url)
    return process_response(rsp)

  def get_region_output(self, region_name, output_name):
    url = self.api1('/region/{}/output/{}'.format(region_name, output_name))
    if self.verbose:
      print('GET {}'.format(url))
    rsp = requests.get(url)
    return process_response(rsp)

  def delete_region(self, region_name):
    url = self.api1('/region/{}'.format(region_name))
    if self.verbose:
      print('DELETE {}'.format(url))
    rsp = requests.delete(url)
    return process_response(rsp)

  def delete_link(self, source_name, dest_name):
    url = self.api1('/link/{}/{}'.format(source_name, dest_name))
    if self.verbose:
      print('DELETE {}'.format(url))
    rsp = requests.delete(url)
    return process_response(rsp)

  def delete_all(self):
    url = self.api1('/ALL')
    if self.verbose:
      print('DELETE {}'.format(url))
    rsp = requests.delete(url)
    return process_response(rsp)

  def run(self, iterations=None):
    query = None if iterations is None else {'iterations': iterations}
    url = self.api1('/run', query)
    if self.verbose:
      print('GET {}'.format(url))
    rsp = requests.get(url)
    return process_response(rsp)

  def execute(self, region_name, command):
    url = self.api1('/region/{}/command'.format(region_name), {'data': command})
    if self.verbose:
      print('GET {}'.format(url))
    rsp = requests.get(url)
    return process_response(rsp)


def get_classifer_predict(net, region_name):
  pred = net.get_region_output(region_name, 'predicted')
  titles = net.get_region_output(region_name, 'titles')
  pdf = net.get_region_output(region_name, 'pdf')

  return {'title': titles[pred[0]], 'prob': pdf[pred[0]]}
