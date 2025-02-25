/*
* (C) Copyright 2023 NOAA/NWS/NCEP/EMC
*
* This software is licensed under the terms of the Apache Licence Version 2.0
* which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
*/

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/embed.h>

#include <netcdf>

#include "bufr/DataContainer.h"
#include "bufr/encoders/Description.h"
#include "bufr/encoders/netcdf/Encoder.h"


namespace py = pybind11;
namespace nc = netCDF;

using bufr::DataContainer;
using bufr::encoders::Description;
using bufr::encoders::netcdf::Encoder;


void setupNetcdfEncoder(py::module& m)
{
  py::class_<Encoder>(m, "Encoder")
   .def(py::init<const std::string&>())
   .def(py::init<const Description&>())
   .def("encode", [](Encoder& self,
                     const std::shared_ptr<DataContainer>& container,
                     const std::string& path,
                     bool append = false) -> std::map<py::tuple, py::object>
     {
        if (path.empty())
        {
          throw std::invalid_argument("Encoder path string cannot be empty!");
        }

        auto backend = Encoder::Backend();
        backend.isMemoryFile = false;
        backend.path = path;

        auto encodedData = self.encode(container, backend, append);
        std::map<py::tuple, py::object> pyEncodedData;

        // Ensure Python is initialized and import netCDF4
        py::gil_scoped_acquire acquire;
        py::module_ netCDF4 = py::module_::import("netCDF4");

        py::dict kwargs;  // Dictionary to hold keyword arguments
        kwargs["mode"] = "r";   // Read mode, adjust as necessary

        for (auto& [key, value] : encodedData)
        {
          size_t pathLength;
          char path[256];
          nc_inq_path(value->getId(), &pathLength, path);
          value->close();

          auto dataset = netCDF4.attr("Dataset")(path, **kwargs);
          pyEncodedData[py::cast(key)] = dataset;
        }

        return pyEncodedData;
      },
      py::arg("container"),
      py::arg("path"),
      py::arg("append") = false,
      "Get the class to encode the dataset");
}
