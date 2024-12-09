/*
* (C) Copyright 2023 NOAA/NWS/NCEP/EMC
*
* This software is licensed under the terms of the Apache Licence Version 2.0
* which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
*/

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <any>
#include <vector>
#include <map>
#include <string>

#include "bufr/encoders/Description.h"


namespace py = pybind11;

using bufr::encoders::Description;


template<typename T>
class PyGlobalWriter : public bufr::encoders::GlobalWriter<T>
{
public:
  explicit PyGlobalWriter(py::dict& globals) : globals_(globals) {}

  void write(const std::string& name, const T& data) final
  {
    py::gil_scoped_acquire gil;
    globals_[py::str(name)] = py::cast(data);
  }

private:
  py::dict& globals_;
};

void setupEncoderDescription(py::module& m)
{
  using bufr::encoders::GlobalDescription;
  using bufr::encoders::GlobalWriterBase;
  using bufr::encoders::GlobalWriter;

  py::class_<Description>(m, "Description")
   .def(py::init<const std::string&>())

   .def("add_variable", &Description::py_addVariable,
        py::arg("name"),
        py::arg("source"),
        py::arg("units"),
        py::arg("longName") = "", "")
   .def ("get_globals", [](Description& self) -> py::dict
    {
      py::dict pyGlobals;
      // Create the Globals
      for (auto &global: self.getGlobals())
      {
        std::shared_ptr<GlobalWriterBase> writer = nullptr;
        if (auto intGlobal =
          std::dynamic_pointer_cast<GlobalDescription<int>>(global))
        {
          writer = std::make_shared<PyGlobalWriter<int>>(pyGlobals);
        }
        if (auto intGlobal =
          std::dynamic_pointer_cast<GlobalDescription<std::vector<int>>>(global))
        {
          writer = std::make_shared<PyGlobalWriter<std::vector<int>>>(pyGlobals);
        }
        else if (auto floatGlobal =
          std::dynamic_pointer_cast<GlobalDescription<float>>(global))
        {
          writer = std::make_shared<PyGlobalWriter<float>>(pyGlobals);
        }
        else if (auto floatVectorGlobal =
          std::dynamic_pointer_cast<GlobalDescription<std::vector<float>>>(global))
        {
          writer = std::make_shared<PyGlobalWriter<std::vector<float>>>(pyGlobals);
        }
        else if (auto doubleGlobal =
          std::dynamic_pointer_cast<GlobalDescription<std::string>>(global))
        {
          writer = std::make_shared<PyGlobalWriter<std::string>>(pyGlobals);
        }

        global->writeTo(writer);
      }

      return pyGlobals;
    })
   .def("get_dims", [](Description& self) -> std::vector<std::map<std::string, std::string>>
     {
        auto dims = std::vector<std::map<std::string, std::string>>();
        for (const auto& dim : self.getDims())
        {
          std::map<std::string, std::string> dimMap;
          dimMap["name"] = dim.name;
          dimMap["source"] = dim.source;
          dims.push_back(dimMap);
        }

        return dims;
      })
   .def("get_variables", [](Description& self) -> std::vector<std::map<std::string, std::string>>
     {
        auto variables = std::vector<std::map<std::string, std::string>>();
        for (const auto& var : self.getVariables())
        {
          std::map<std::string, std::string> varMap;
          varMap["name"] = var.name;
          varMap["source"] = var.source;
          varMap["units"] = var.units;
          varMap["longName"] = var.longName;

          if (var.range)
          {
            varMap["range"] = "(" + std::to_string(var.range->start) + ", " + \
                              std::to_string(var.range->end) + ")";
          }

          if (var.coordinates)
          {
            varMap["coordinates"] = *var.coordinates;
          }

          if (!var.chunks.empty())
          {
            varMap["chunks"] = "(" + std::to_string(var.chunks[0]) + ", " + \
                               std::to_string(var.chunks[1]) + ")";
          }

          variables.push_back(varMap);
        }

        return variables;
      });
}
