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

using namespace nupic;

namespace nupic_ext
{
    void init_CoordinateEncoder(py::module& m)
    {
        py::class_<CoordinateEncoder> py_CoordinateEncoder(m, "CoordinateEncoder",
R"(
TODO: DOCSTRINGS!
)");

        py_CoordinateEncoder.def(py::init<UInt, Real, UInt, Real, UInt>(),
        // TODO DOCSTRINGS FOR INIT
            py::arg("size"),
            py::arg("sparsity"),
            py::arg("ndim"),
            py::arg("radius"),
            py::arg("seed") = 0u);

        // TODO Accessors for: size, sparsity, ndim, radius

        // TODO DOCSTRINGS FOR ENCODE
        py_CoordinateEncoder.def("encode", &CoordinateEncoder::encode);

        py_CoordinateEncoder.def("encode", []
            (CoordinateEncoder &self, vector<Real> value) {
                auto sdr = new sdr::SDR({ self.size });
                self.encode( value, *sdr );
                return sdr;
        });
    }
}
