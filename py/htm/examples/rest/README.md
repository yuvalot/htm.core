<img src="http://numenta.org/87b23beb8a4b7dea7d88099bfb28d182.svg" alt="NuPIC Logo" width=100/>

## Rest api client library

htm_rest_api.py   A easy way to use `rest_server` api.

### To use

```python
from htm_rest_api import NetworkConfig, NetworkREST

host = 'http://127.0.0.1:8050'
verbose = True
```

1. Initial the `NetworkREST` with `json` or `yaml` string.

```python
config = '''
{network: [
   {addRegion: {name: "encoder", type: "RDSEEncoderRegion", params: {size: 1000, sparsity: 0.2, radius: 0.03, seed: 2019, noise: 0.01}}},
   {addRegion: {name: "sp", type: "SPRegion", params: {columnCount: 2048, globalInhibition: true}}},
   {addRegion: {name: "tm", type: "TMRegion", params: {cellsPerColumn: 8}}},
   {addLink:   {src: "encoder.encoded", dest: "sp.bottomUpIn"}},
   {addLink:   {src: "sp.bottomUpOut", dest: "tm.bottomUpIn"}},
]}'''

net = NetworkREST(config, host = host, verbose = verbose)
```

2. Initial the `NetworkREST` with `NetworkConfig`.

```python
config = NetworkConfig()
encoder = config.add_region('encoder', 'RDSEEncoderRegion', {
  'size': 1000,
  'sparsity': 0.2,
  'radius': 0.03,
  'seed': 2019,
  'noise': 0.01
})
sp = config.add_region('sp', 'SPRegion', {
  'columnCount': 2048,
  'globalInhibition': True
})
tm = config.add_region('tm', 'TMRegion', {'cellsPerColumn': 8})

config.add_link(encoder, sp, 'encoded', 'bottomUpIn')
config.add_link(sp, tp, 'bottomUpOut', 'bottomUpIn')

net = NetworkREST(str(config), host = host, verbose = verbose)
```

3. Create network

```python
net.create()
```

4. Put a param to Region.

```python
net.put_region_param(encoder.name, 'sensedValue', 0.02)
```

5. Run the network.

```python
net.run(1)
```

6. Get the Region output.

```python
score = net.get_region_output(tm.name, 'anomaly')[0]
```


## Rest client Example

client.py contains a simple `sin(x)` example use rest client.

#### To run

```python
python client.py
```

## Rest client hotgym Example

hotgym.py contains a simple hotgym example use rest client.

#### To run

```python
python hotgym.py
```
