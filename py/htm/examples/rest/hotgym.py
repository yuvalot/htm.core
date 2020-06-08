import csv
import datetime
import os
import numpy as np
import math

from rest_api import Link, Region, NetworkConfig, Network, get_classifer_predict

_EXAMPLE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_INPUT_FILE_PATH = os.path.join(_EXAMPLE_DIR, "gymdata.csv")

default_parameters = {
    # there are 2 (3) encoders: "value" (RDSE) & "time" (DateTime weekend, timeOfDay)
    'enc': {
        "value": {
            'resolution': 0.88,
            'size': 700,
            'sparsity': 0.02
        },
        "time": {
            'timeOfDay': (30, 1),
            'weekend': 21
        }
    },
    'sp': {
        'boostStrength': 3.0,
        'columnCount': 1638,
        'localAreaDensity': 0.04395604395604396,
        'potentialPct': 0.85,
        'synPermActiveInc': 0.04,
        'synPermConnected': 0.13999999999999999,
        'synPermInactiveDec': 0.006
    },
    'tm': {
        'activationThreshold': 17,
        'cellsPerColumn': 13,
        'initialPerm': 0.21,
        'maxSegmentsPerCell': 128,
        'maxSynapsesPerSegment': 64,
        'minThreshold': 10,
        'newSynapseCount': 32,
        'permanenceDec': 0.1,
        'permanenceInc': 0.1
    }
}


def main(parameters=default_parameters, argv=None, verbose=True):
  if verbose:
    import pprint
    print("Parameters:")
    pprint.pprint(parameters, indent=4)
    print("")

  # Read the input file.
  records = []
  with open(_INPUT_FILE_PATH, "r") as fin:
    reader = csv.reader(fin)
    headers = next(reader)
    next(reader)
    next(reader)
    for record in reader:
      records.append(record)

  config = NetworkConfig()
  # Make the Encoders.  These will convert input data into binary representations.
  dateRegion = Region(
      'dateEncoder', 'DateEncoderRegion', {
          'timeOfDay_width': parameters["enc"]["time"]["timeOfDay"][0],
          'timeOfDay_radius': parameters["enc"]["time"]["timeOfDay"][1],
          'weekend_width': parameters["enc"]["time"]["weekend"]
      })

  config.add_region(dateRegion)

  scalarRegion = Region(
      'scalarEncoder', 'RDSEEncoderRegion', {
          'size': parameters["enc"]["value"]["size"],
          'sparsity': parameters["enc"]["value"]["sparsity"],
          'resolution': parameters["enc"]["value"]["resolution"]
      })
  config.add_region(scalarRegion)

  # Make the HTM.  SpatialPooler & TemporalMemory & associated tools.
  spParams = parameters["sp"]
  spRegion = Region(
      'sp', 'SPRegion',
      dict(
          columnCount=spParams['columnCount'],
          potentialPct=spParams["potentialPct"],
          globalInhibition=True,
          localAreaDensity=spParams["localAreaDensity"],
          synPermInactiveDec=spParams["synPermInactiveDec"],
          synPermActiveInc=spParams["synPermActiveInc"],
          synPermConnected=spParams["synPermConnected"],
          boostStrength=spParams["boostStrength"],
          wrapAround=True))

  config.add_region(spRegion)

  tmParams = parameters["tm"]
  tmRegion = Region(
      'tm', 'TMRegion',
      dict(
          columnCount=spParams['columnCount'],
          cellsPerColumn=tmParams["cellsPerColumn"],
          activationThreshold=tmParams["activationThreshold"],
          initialPermanence=tmParams["initialPerm"],
          connectedPermanence=spParams["synPermConnected"],
          minThreshold=tmParams["minThreshold"],
          maxNewSynapseCount=tmParams["newSynapseCount"],
          permanenceIncrement=tmParams["permanenceInc"],
          permanenceDecrement=tmParams["permanenceDec"],
          predictedSegmentDecrement=0.0,
          maxSegmentsPerCell=tmParams["maxSegmentsPerCell"],
          maxSynapsesPerSegment=tmParams["maxSynapsesPerSegment"]))

  config.add_region(tmRegion)

  clsrRegion = Region('clsr', 'ClassifierRegion', {'learn': True})
  config.add_region(clsrRegion)

  config.add_link(Link(dateRegion.name, spRegion.name, 'encoded', 'bottomUpIn'))
  config.add_link(
      Link(scalarRegion.name, spRegion.name, 'encoded', 'bottomUpIn'))
  config.add_link(
      Link(spRegion.name, tmRegion.name, 'bottomUpOut', 'bottomUpIn'))
  config.add_link(
      Link(tmRegion.name, clsrRegion.name, 'bottomUpOut', 'pattern'))
  config.add_link(Link(scalarRegion.name, clsrRegion.name, 'bucket', 'bucket'))

  net = Network(str(config), verbose=verbose)

  net.create()

  # Iterate through every datum in the dataset, record the inputs & outputs.
  inputs = []
  anomaly = []
  anomalyProb = []
  predictions = []
  for count, record in enumerate(records):

    # Convert date string into Python date object.
    dateString = datetime.datetime.strptime(record[0], "%m/%d/%y %H:%M")
    # Convert data value string into float.
    consumption = float(record[1])
    inputs.append(consumption)

    # Call the encoders to create bit representations for each value.  These are SDR objects.
    net.put_region_param(dateRegion.name, 'sensedTime',
                         int(dateString.timestamp()))
    net.put_region_param(scalarRegion.name, 'sensedValue', consumption)

    # Predict what will happen, and then train the predictor based on what just happened.
    net.run()
    pred = get_classifer_predict(net, clsrRegion.name)
    pred['anomaly'] = net.get_region_output(tmRegion.name, 'anomaly')[0]
    if pred.get('title'):
      predictions.append(pred['title'])
    else:
      predictions.append(float('nan'))

    anomaly.append(pred['anomaly'])

  ## # Calculate the predictive accuracy, Root-Mean-Squared
  accuracy = 0
  accuracy_samples = 0

  for idx, inp in enumerate(inputs):
    val = predictions[idx]
    if not math.isnan(val):
      accuracy += (inp - val)**2
      accuracy_samples += 1

  accuracy = (accuracy / accuracy_samples)**.5
  print("Predictive Error (RMS):", accuracy)

  # Show info about the anomaly (mean & std)
  print("Anomaly Mean", np.mean(anomaly))
  print("Anomaly Std ", np.std(anomaly))

  # Plot the Predictions and Anomalies.
  if verbose:
    try:
      import matplotlib.pyplot as plt
    except:
      print("WARNING: failed to import matplotlib, plots cannot be shown.")
      return -accuracy

    plt.subplot(2, 1, 1)
    plt.title("Predictions")
    plt.xlabel("Time")
    plt.ylabel("Power Consumption")
    plt.plot(
        np.arange(len(inputs)), inputs, 'red', np.arange(len(inputs)),
        predictions, 'blue')
    plt.legend(labels=('Input', '1 Step Prediction, Shifted 1 step'))

    plt.subplot(2, 1, 2)
    plt.title("Anomaly Score")
    plt.xlabel("Time")
    plt.ylabel("Power Consumption")
    inputs = np.array(inputs) / max(inputs)
    plt.plot(
        np.arange(len(inputs)),
        inputs,
        'red',
        np.arange(len(inputs)),
        anomaly,
        'blue',
    )
    plt.legend(labels=('Input', 'Anomaly Score'))
    plt.show()

  return -accuracy


if __name__ == "__main__":
  main()
