/* ---------------------------------------------------------------------
 * Numenta Platform for Intelligent Computing (NuPIC)
 * Copyright (C) 2019, David McDougall
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
 * ---------------------------------------------------------------------
 */

/** @file
 * PyBind11 bindings for ColumnPooler class
 */

#include <bindings/suppress_register.hpp>  //include before pybind11.h
#include <pybind11/pybind11.h>
#include <pybind11/iostream.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>

#include <nupic/algorithms/ColumnPooler.cpp>

#include "bindings/engine/py_utils.hpp"

namespace py = pybind11;
using namespace nupic;
using namespace nupic::algorithms::column_pooler;

namespace nupic_ext
{
  void init_Column_Pooler(py::module& m)
  {
    py::class_<Parameters> py_Parameters(m, "ColumnPoolerParameters");
    py_Parameters.def_readwrite("proximalInputDimensions",      &Parameters::proximalInputDimensions);
    py_Parameters.def_readwrite("distalInputDimensions",        &Parameters::distalInputDimensions);
    py_Parameters.def_readwrite("inhibitionDimensions",         &Parameters::inhibitionDimensions);
    py_Parameters.def_readwrite("cellsPerInhibitionArea",       &Parameters::cellsPerInhibitionArea);
    py_Parameters.def_readwrite("minSparsity",                  &Parameters::minSparsity);
    py_Parameters.def_readwrite("maxBurstSparsity",             &Parameters::maxBurstSparsity);
    py_Parameters.def_readwrite("maxDepolarizedSparsity",       &Parameters::maxDepolarizedSparsity);
    py_Parameters.def_readwrite("potentialPool",                &Parameters::potentialPool);
    py_Parameters.def_readwrite("proximalSegments",             &Parameters::proximalSegments);
    py_Parameters.def_readwrite("proximalSegmentThreshold",     &Parameters::proximalSegmentThreshold);
    py_Parameters.def_readwrite("proximalIncrement",            &Parameters::proximalIncrement);
    py_Parameters.def_readwrite("proximalDecrement",            &Parameters::proximalDecrement);
    py_Parameters.def_readwrite("proximalSynapseThreshold",     &Parameters::proximalSynapseThreshold);
    py_Parameters.def_readwrite("proximalInitialPermanence",    &Parameters::proximalInitialPermanence);
    py_Parameters.def_readwrite("proximalMinConnections",       &Parameters::proximalMinConnections);
    py_Parameters.def_readwrite("proximalMaxConnections",       &Parameters::proximalMaxConnections);
    py_Parameters.def_readwrite("distalMaxSegments",            &Parameters::distalMaxSegments);
    py_Parameters.def_readwrite("distalMaxSynapsesPerSegment",  &Parameters::distalMaxSynapsesPerSegment);
    py_Parameters.def_readwrite("distalSegmentThreshold",       &Parameters::distalSegmentThreshold);
    py_Parameters.def_readwrite("distalSegmentMatch",           &Parameters::distalSegmentMatch);
    py_Parameters.def_readwrite("distalAddSynapses",            &Parameters::distalAddSynapses);
    py_Parameters.def_readwrite("distalInitialPermanence",      &Parameters::distalInitialPermanence);
    py_Parameters.def_readwrite("distalIncrement",              &Parameters::distalIncrement);
    py_Parameters.def_readwrite("distalDecrement",              &Parameters::distalDecrement);
    py_Parameters.def_readwrite("distalMispredictDecrement",    &Parameters::distalMispredictDecrement);
    py_Parameters.def_readwrite("distalSynapseThreshold",       &Parameters::distalSynapseThreshold);
    py_Parameters.def_readwrite("stabilityRate",                &Parameters::stabilityRate);
    py_Parameters.def_readwrite("fatigueRate",                  &Parameters::fatigueRate);
    py_Parameters.def_readwrite("period",                       &Parameters::period);
    py_Parameters.def_readwrite("seed",                         &Parameters::seed);
    py_Parameters.def_readwrite("verbose",                      &Parameters::verbose);

    m.def("DefaultTopology", &DefaultTopology,
        py::arg("potentialPct") = 0.0f,
        py::arg("radius") = 0.0f,
        py::arg("wrapAround") = false);

    m.def("NoTopology", &NoTopology,
        py::arg("potentialPct"));

    m.def("defaultProximalInitialPermanence", &defaultProximalInitialPermanence);

    py::class_<ColumnPooler> py_ColumnPooler(m, "ColumnPooler");
    py_ColumnPooler.def(py::init<const Parameters&>());
    py_ColumnPooler.def("setParameters", &ColumnPooler::setParameters);
    py_ColumnPooler.def_property_readonly("cellDimensions", [](const ColumnPooler &self)
        { return self.cellDimensions; });
    py_ColumnPooler.def_property_readonly("dimensions", [](const ColumnPooler &self)
        { return self.dimensions; });
    py_ColumnPooler.def_property_readonly("size", [](const ColumnPooler &self)
        { return self.size; });
    py_ColumnPooler.def_property_readonly("parameters", [](const ColumnPooler &self)
        { return self.parameters; });
    py_ColumnPooler.def_property_readonly("activeCells", [](const ColumnPooler &self)
        { return &self.activeCells; });
    py_ColumnPooler.def_property_readonly("winnerCells", [](const ColumnPooler &self)
        { return &self.winnerCells; });
    py_ColumnPooler.def_property_readonly("rawAnomaly", [](const ColumnPooler &self)
        { return self.rawAnomaly; });
    py_ColumnPooler.def_property_readonly_static("defaultParameters",
        [](py::object self) { return new Parameters(); });

    py_ColumnPooler.def("reset", &ColumnPooler::reset);

    py_ColumnPooler.def("compute",
        (void (ColumnPooler::*)(const SDR&, const bool))
                &ColumnPooler::compute,
            py::arg("proximalInputActive"),
            py::arg("learn"));

    py_ColumnPooler.def("compute",
        (void (ColumnPooler::*)(const SDR&, const SDR&, const SDR&, const bool))
                &ColumnPooler::compute,
            py::arg("proximalInputActive"),
            py::arg("distalInputActive"),
            py::arg("distalInputLearning"),
            py::arg("learn"));

    py_ColumnPooler.def_readwrite("proximalConnections", &ColumnPooler::proximalConnections);
    py_ColumnPooler.def_readwrite("distalConnections", &ColumnPooler::distalConnections);

    py_ColumnPooler.def("__str__", [](const ColumnPooler &self)
    {
        stringstream buf;
        buf << "Column Pooler\nProximal " << self.proximalConnections << endl << endl;
        buf << "Distal " << self.distalConnections << endl;
        return buf.str();
    });
  }
}
