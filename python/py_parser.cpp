/*
* (C) Copyright 2023 NOAA/NWS/NCEP/EMC
*
* This software is licensed under the terms of the Apache Licence Version 2.0
* which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
*/

#include <pybind11/pybind11.h>

#include <memory>
#include <vector>
#include <string>
#include <mpi.h>

#include "bufr/BufrParser.h"

#include "py_mpi.h"

namespace py = pybind11;

using bufr::BufrParser;

void setupParser(py::module& m)
{
  m.doc() = "Provides the ability to process data from BUFR files.";

  py::class_<BufrParser>(m, "Parser")
    .def(py::init<const std::string&, const std::string&, const std::string&>(),
         py::arg("obsfile"),
         py::arg("mapping_path"),
         py::arg("table_path") = "")
    .def("parse", [](BufrParser& self, size_t numMsgs = 0)
         {
           return self.parse(numMsgs);
         },
         py::arg("numMsgs") = 0,
         "Get Parser to parse a config file and get the data container.")
    .def("parse", [](BufrParser& self, bufr::mpi::Comm& comm)
        {
          if (comm.size() == 1)
          {
            // use non-mpi version of the parser
            return self.parse(0);
          }

          return self.parse(comm.getComm());
        },
        py::arg("comm"),
        "Get Parser to parse a config file and get the data container in parallel.");
}
