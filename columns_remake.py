#!/usr/bin/python3

"""
https://www.frontiersin.org/articles/10.3389/fncir.2017.00081/full
Simulation Results

In this section, we describe simulation results that illustrate the performance
of our network model. The network structure consists of one or more cortical
columns, each with two layers, as described earlier (Figure 1). In the first set
of simulations the input layer of each column consists of 150 mini-columns, with
16 cells per mini-column, for a total of 2,400 cells. The output layer of each
column consists of 4,096 cells, which are not arranged in mini-columns. The
output layer contains inter-column and intra-column connections via the distal
basal dendrites of each cell. The output layer also projects back to the apical
dendrites of the input layer within the same column. All connections are
continuously learned and adjusted during the training process.

We trained the network on a library of up to 500 objects (Figure 2A). Each
object consists of 10 sensory features chosen from a library of 5 to 30 possible
features. Each feature is assigned a corresponding location on the object. Note
that although each object consists of a unique set of features/locations, any
given feature or feature/location is shared across several objects. As such, a
single sensation by a single column is insufficient to unambiguously identify an
object.

The set of active cells in the output layer represents the objects that are
recognized by the network. During inference we say that the network
unambiguously recognizes an object when the representation of the output layer
overlaps significantly with the representation for correct object and not for
any other object. Complete details of object construction and recognition are
described in Materials and Methods.

In the following paragraphs we first describe network convergence, using single
and multi-column networks. We then discuss the capacity of the network.
"""

import numpy as np
import random
from nupic.bindings.algorithms import *
import nupic.bindings.math as nm


def dataset(nFeatures, nLocations, nObjects):
    feature_library = [SDR(2400) for f in range(nFeatures)]
    [X.randomize(10 / 2400) for X in feature_library]

    location_library = [SDR(2400) for f in range(nLocations)]
    [X.randomize(10 / 2400) for X in location_library]

    object_library = []
    for i in range(nObjects):
        object_library.append([
            (random.choice(feature_library), random.choice(location_library))
                for X in range(10)])

    return object_library


def L4():
    L4_sp = SpatialPooler(
        inputDimensions                = [2400],
        columnDimensions               = [150],
        potentialRadius                = 2400 * 99,
        potentialPct                   = .80,
        globalInhibition               = True,
        localAreaDensity               = -1,
        numActiveColumnsPerInhArea     = 10,
        stimulusThreshold              = 3,
        synPermInactiveDec             = .0005,
        synPermActiveInc               = .0001,
        synPermConnected               = .1,
        minPctOverlapDutyCycle         = 0,
        dutyCyclePeriod                = 1000,
        boostStrength                  = 0,
        seed                           = 0,
        spVerbosity                    = 1,
        wrapAround                     = 1)

    L4_tm = TemporalMemory(
        columnDimensions          = [150],
        cellsPerColumn            = 16,
        activationThreshold       = 6,
        initialPermanence         = 0.51,
        connectedPermanence       = 0.60,
        minThreshold              = 4,
        maxNewSynapseCount        = 9,
        permanenceIncrement       = .1,
        permanenceDecrement       = .02,
        predictedSegmentDecrement = 0,
        seed                      = 0,
        maxSegmentsPerCell        = 64,
        maxSynapsesPerSegment     = 64,
        checkInputs               = True,
        extra                     = 2400)

    return L4_sp, L4_tm


def L23(nExternal):
    # def myTopo(cell, pp, rng):
    #     print("0x%x"%id(pp))
    #     pp.randomize( .8 )
    params = ColumnPooler.defaultParameters
    params.cellsPerInhibitionArea       = 4096
    params.inhibitionDimensions         = [1]
    params.sparsity                     = .01
    # params.potentialPool                = myTopo # NoTopology( .80 ) # FIXME THIS IS BROKEN!!!!!
    params.proximalIncrement            = .1
    params.proximalDecrement            = .01
    params.proximalInputDimensions      = [150 * 16]
    params.proximalSynapseThreshold     = .6
    params.proximalSegmentThreshold     = 3
    params.proximalSegments             = 3
    params.distalIncrement              = .1
    params.distalDecrement              = .001
    params.distalMispredictDecrement    = .0
    params.distalSynapseThreshold       = .5
    params.distalAddSynapses            = 0 # 27
    params.distalSegmentThreshold       = 18
    params.distalSegmentMatch           = 10
    params.distalMaxSynapsesPerSegment  = 128
    params.distalMaxSegments            = 128
    params.distalInputDimensions        = [nExternal]
    params.period                       = 1000
    params.fatigue_rate                 = .0
    params.stability_rate               = .95
    params.seed                         = 0
    params.verbose                      = True
    return ColumnPooler(params)


def basic_test():
    # Skip L4, instead generate plausible L4 activity
    features = [SDR(150 * 16) for i in range(100)]
    [l4.randomize( .02 ) for l4 in features]
    objects = []
    for obj in range(100):
        objects.append( random.sample(features, 10) )

    l23  = L23(nExternal = 0)

    sdrc = SDRClassifier( steps = [0], alpha = 0.001, actValueAlpha = .3 )
    sdrc.compute(0, [SDR(l23.cellDimensions).size-1],    # Initialize the table.
        bucketIdx = [len(objects)-1],
        actValue = [len(objects)-1.],
        category = True,
        learn=True, infer=False)
    autoinc = 1

    l23_act   = SDR([ 1, 4096 ])
    l23_stats = SDR_Metrics(l23_act, 999999999)

    def compute(obj, learn):
        l4_act = random.choice(obj)
        l23.compute(l4_act, learn, l23_act)

    def reset():
        l23.reset()

    # Train
    for x in range(10):
        index = list(range(len(objects)))
        random.shuffle(index)
        for lbl in index:
            obj = objects[lbl]
            reset()
            for q in range(10):
                compute(obj, learn=True)
                sdrc.compute(autoinc, l23_act.sparse,
                    bucketIdx = [lbl],
                    actValue  = [lbl + 0.],
                    category  = True,
                    learn=True, infer=False)
                autoinc += 1
        print(".", end='', flush=True)
    print()

    print("L23", str(l23_stats))
    print()

    if False:
        print("Turning off stability for testing...")
        testArgs = l23.parameters
        testArgs.stability_rate = 0
        l23.setParameters( testArgs )

    # Test
    score = 0.
    score_samples = 0
    for lbl in range(len(objects)):
        obj = objects[lbl]
        reset()
        for i in range(5):
            compute(obj, learn=False)
        inference = sdrc.compute(autoinc, l23_act.sparse,
            bucketIdx = [],
            actValue  = [],
            category  = True,
            learn=False, infer=True)['']
        autoinc += 1
        if lbl == np.argmax(inference):
            score += 1
        score_samples += 1
    if score_samples > 0:
        print("SCORE:", 100 * score / score_samples, "%")

if __name__ == '__main__':
    basic_test()
