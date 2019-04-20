
import nupic.bindings.encoders
CoordinateEncoder           = nupic.bindings.encoders.CoordinateEncoder
CoordinateEncoderParameters = nupic.bindings.encoders.CoordinateEncoderParameters

if __name__ == "__main__":
    import numpy as np
    from nupic.bindings.sdr import *

    P = CoordinateEncoderParameters()
    P.size          = 2500
    P.activeBits    = 75
    P.numDimensions = 2
    P.resolution    = 1
    P.seed          = 42
    C = CoordinateEncoder( P )

    arena = 200
    M = Metrics( [P.size], 999999 )
    rf = np.ones((arena, arena, P.size))
    for x in range(arena):
      for y in range(arena):
        Z = SDR( P.size )
        C.encode( [x, y], Z )
        M.addData( Z )
        rf[x,y, :] = Z.dense

    print(M)

    import matplotlib.pyplot as plt
    for i in range(12):
        plt.subplot(3, 4, i + 1)
        plt.imshow( 255 * rf[:,:,i], interpolation='nearest' )

    plt.show()
