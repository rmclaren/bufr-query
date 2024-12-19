/*
* (C) Copyright 2023 NOAA/NWS/NCEP/EMC
*
* This software is licensed under the terms of the Apache Licence Version 2.0
* which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
*/

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>

#include "bufr/encoders/Description.h"


namespace py = pybind11;

using bufr::encoders::Description;

void setupEncoderDescription(py::module& m)
{
  py::class_<Description>(m, "Description")
   .def(py::init<const std::string&>())
   .def("add_variable", &Description::py_addVariable,
        py::arg("name"),
        py::arg("source"),
        py::arg("units"),
        py::arg("longName")="",
        py::arg("coordinates")="",
        py::arg("chunks")=std::vector<size_t>{},
        py::arg("compressionLevel")=3,
        "Add a variable to the description.")
   .def("remove_variable", &Description::removeVariable,
        py::arg("name"),
        "Remove a variable from the description.")
    .def("add_dimension",
         static_cast<void (Description::*)(const std::string&,
                                           const std::vector<std::string>&,
                                           const std::string&)>(&Description::addDimension),
         py::arg("name"),
         py::arg("paths"),
         py::arg("source") = "",
         "Add a dimension to the description.")
   .def("remove_dimension", &Description::removeDimension,
        py::arg("name"),
        "Remove a dimension from the description.")
   .def("add_global", [](Description& self, const std::string& name, const py::object& value)
   {
      if (py::isinstance<py::str>(value))
      {
          self.addGlobal(name, value.cast<std::string>());
      }
      else if (py::isinstance<py::float_>(value))
      {
        self.addGlobal(name, value.cast<float>());
      }
      else if (py::isinstance<py::int_>(value))
      {
        self.addGlobal(name, value.cast<int>());
      }
      else if (py::isinstance<py::list>(value))
      {
        py::list values = value.cast<py::list>();
        if (py::isinstance<py::str>(values[0]))
        {
          std::vector<std::string> vec;
          for (auto v : values)
          {
            vec.push_back(v.cast<std::string>());
          }
          self.addGlobal(name, vec);
        }
        else if (py::isinstance<py::float_>(values[0]))
        {
          std::vector<float> vec;
          for (auto v : values)
          {
            vec.push_back(v.cast<float>());
          }
          self.addGlobal(name, vec);
        }
        else if (py::isinstance<py::int_>(values[0]))
        {
          std::vector<int> vec;
          for (auto v : values)
          {
            vec.push_back(v.cast<int>());
          }
          self.addGlobal(name, vec);
        }
        else
        {
          throw eckit::BadValue("Unsupported data type encountered.");
        }
      }
      else
      {
        throw eckit::BadValue("Unsupported data type encountered.");
      }
     },
      py::arg("name"),
      py::arg("value"),
     "Add a global attribute to the description.")
   .def("remove_global", &Description::removeGlobal,
        py::arg("name"),
        "Remove a global attribute from the description.");
}
