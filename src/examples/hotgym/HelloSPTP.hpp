// header file for Hotgym, HelloSPTP
#ifndef NTA_EXAMPLES_HOTGYM_
#define NTA_EXAMPLES_HOTGYM_

#include <htm/types/Types.hpp>
#include <htm/os/Timer.hpp>

namespace examples {

using htm::Real64;
using htm::UInt;
using htm::Timer;

class BenchmarkHotgym {

public:	
  Real64 run(
    UInt EPOCHS = 5000,
    bool useSPlocal=false, //can toggle which (long running) components are tested, default all
    bool useSPglobal=true,
    bool useTM=true,
    const UInt COLS = 9000, // number of columns in SP, TP
    const UInt DIM_INPUT = 9000,
    const UInt CELLS = 16 // cells per column in TP
  );

  //timers
  Timer tInit, tAll, tRng, tEnc, tSPloc, tSPglob, tTM, tAnLikelihood;
};

} //-ns
#endif //header
