from urllib.parse import urlencode
import requests
import json


class NetworkRESTError(Exception):
  pass


def request(method, url, data=None, verbose=False):
  if verbose:
    if data:
      print('{} {}; body: {}'.format(method, url, data))
    else:
      print('{} {}'.format(method, url))
  rsp = requests.request(method, url, data=data)
  if rsp.status_code != requests.codes.ok:
    raise NetworkRESTError('HTTP Error')

  text = rsp.text.strip()

  if text[0:6] == 'ERROR:':
    raise NetworkRESTError(text[6:].strip())

  if text == 'OK':
    return text

  idx = text.find('[')

  if idx == -1:
    return text

  data = text[idx + 1:].replace(']}', '')

  return json.loads('[' + data + ']')


class NetworkREST(object):

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
    id = request('POST', url, self.config, verbose=self.verbose)

    if self.verbose:
      print('Resource ID: ' + id)

    if self.id and self.id != id:
      raise NetworkRESTError('Network id not match')
    else:
      self.id = id

  def put_region_param(self, region_name, param_name, data):
    url = self.api1('/region/{}/param/{}'.format(region_name, param_name),
                    {'data': data})
    return request('PUT', url, verbose=self.verbose)

  def get_region_param(self, region_name, param_name):
    url = self.api1('/region/{}/param/{}'.format(region_name, param_name))
    return request('GET', url, verbose=self.verbose)

  def put_region_input(self, region_name, input_name, data):
    url = self.api1('/region/{}/input/{}'.format(region_name, input_name),
                    {'data': data})
    return request('PUT', url, verbose=self.verbose)

  def get_region_input(self, region_name, input_name):
    url = self.api1('/region/{}/input/{}'.format(region_name, input_name))
    return request('GET', url, verbose=self.verbose)

  def get_region_output(self, region_name, output_name):
    url = self.api1('/region/{}/output/{}'.format(region_name, output_name))
    return request('GET', url, verbose=self.verbose)

  def delete_region(self, region_name):
    url = self.api1('/region/{}'.format(region_name))
    return request('DELETE', url, verbose=self.verbose)

  def delete_link(self, source_name, dest_name):
    url = self.api1('/link/{}/{}'.format(source_name, dest_name))
    return request('DELETE', url, verbose=self.verbose)

  def delete_all(self):
    url = self.api1('/ALL')
    return request('DELETE', url, verbose=self.verbose)

  def run(self, iterations=None):
    query = None if iterations is None else {'iterations': iterations}
    url = self.api1('/run', query)
    return request('GET', url, verbose=self.verbose)

  def execute(self, region_name, command):
    url = self.api1('/region/{}/command'.format(region_name), {'data': command})
    return request('GET', url, verbose=self.verbose)


def get_classifer_predict(net, region_name):
  pred = net.get_region_output(region_name, 'predicted')
  if not pred:
    return {}
  titles = net.get_region_output(region_name, 'titles')
  pdf = net.get_region_output(region_name, 'pdf')

  return {'title': titles[pred[0]], 'prob': pdf[pred[0]]}


class Region(dict):

  def __init__(self, name, type, params={}):
    self['name'] = name
    self['type'] = type
    self['params'] = params
    self.__dict__ = self


class Link(dict):

  def __init__(self, source, dest, source_output, dest_input):
    self.source = source
    self.dest = dest
    self.source_output = source_output
    self.dest_input = dest_input

    self['src'] = '{}.{}'.format(self.source, self.source_output)
    self['dest'] = '{}.{}'.format(self.dest, self.dest_input)


class NetworkConfig(object):

  def __init__(self):
    self.regions = []
    self.links = []

  def has_region(self, name):
    for region in self.regions:
      if region.name == name:
        return True

    return False

  def add_region(self, region):
    if self.has_region(region.name):
      raise NetworkRESTError('Region {} is already exists.'.format(region.name))

    self.regions.append(region)

  def add_link(self, link):
    if not self.has_region(link.source):
      raise NetworkRESTError('Region {} is not found.'.format(link.source))
    if not self.has_region(link.dest):
      raise NetworkRESTError('Region {} is not found.'.format(link.dest))

    self.links.append(link)

  def __str__(self):
    network = []

    for region in self.regions:
      network.append({'addRegion': dict(region)})
    for link in self.links:
      network.append({'addLink': dict(link)})

    return json.dumps({'network': network}, indent=2)
