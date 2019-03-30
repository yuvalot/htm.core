/* ----------------------------------------------------------------------
 * Numenta Platform for Intelligent Computing (NuPIC)
 * Copyright (C) 2019, David McDougall
 * The following terms and conditions apply:
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Affero Public License for more details.
 *
 * You should have received a copy of the GNU Affero Public License
 * along with this program.  If not, see http://www.gnu.org/licenses.
 *
 * http://numenta.org/licenses/
 * ---------------------------------------------------------------------- */

#include <bindings/suppress_register.hpp>  //include before pybind11.h
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <nupic/encoders/CoordinateEncoder.hpp>

namespace py = pybind11;

// using namespace std;
using namespace nupic::encoders;

namespace nupic_ext
{
  void init_CoordinateEncoder(py::module& m)
  {
    py::class_<CoordinateEncoderParameters, RDSE_Parameters>
                            py_CoordEncParams(m, "CoordinateEncoderParameters");

    py_CoordEncParams.def(py::init<>());

    py_CoordEncParams.def_readwrite("numDimensions", &CoordinateEncoderParameters::numDimensions);

    py::class_<CoordinateEncoder> py_CoordinateEncoder(m, "CoordinateEncoder",
R"(
TODO: DOCSTRINGS!
)");

    py_CoordinateEncoder.def(py::init<CoordinateEncoderParameters>());

    // TODO Accessors for parameters

    py_CoordinateEncoder.def("encode", &CoordinateEncoder::encode);

    // py_CoordinateEncoder.def("encode", []
    // (CoordinateEncoder &self, vector<Real> value) {
    // auto sdr = new sdr::SDR({ self.size });
    // self.encode( value, *sdr );
    // return sdr;
    // });
  }
}
