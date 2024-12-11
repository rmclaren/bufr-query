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
        if (auto _ =
          std::dynamic_pointer_cast<GlobalDescription<int>>(global))
        {
          writer = std::make_shared<PyGlobalWriter<int>>(pyGlobals);
        }
        else if (auto _ =
          std::dynamic_pointer_cast<GlobalDescription<std::vector<int>>>(global))
        {
          writer = std::make_shared<PyGlobalWriter<std::vector<int>>>(pyGlobals);
        }
        else if (auto _ =
          std::dynamic_pointer_cast<GlobalDescription<float>>(global))
        {
          writer = std::make_shared<PyGlobalWriter<float>>(pyGlobals);
        }
        else if (auto _ =
          std::dynamic_pointer_cast<GlobalDescription<std::vector<float>>>(global))
        {
          writer = std::make_shared<PyGlobalWriter<std::vector<float>>>(pyGlobals);
        }
        else if (auto _ =
          std::dynamic_pointer_cast<GlobalDescription<std::string>>(global))
        {
          writer = std::make_shared<PyGlobalWriter<std::string>>(pyGlobals);
        }

        global->writeTo(writer);
      }

      return pyGlobals;
    })
   .def("get_dims", [](Description& self) -> py::list
     {
        py::list dims;
        for (const auto& dim : self.getDims())
        {
          py::dict dimMap;
          dimMap["name"] = dim.name;

          py::list paths;
          for (const auto& path : dim.paths)
          {
            paths.append(path.str());
          }

          dimMap["paths"] = paths;
          dimMap["source"] = dim.source;
          dims.append(dimMap);
        }

        return dims;
      })
   .def("get_variables", [](Description& self) -> py::list
     {
        py::list variables;
        for (const auto& var : self.getVariables())
        {
          py::dict varMap;
          varMap["name"] = var.name;
          varMap["source"] = var.source;
          varMap["units"] = var.units;
          varMap["longName"] = var.longName;

          if (var.range)
          {
            py::list range;
            range.append(var.range->start);
            range.append(var.range->end);

            varMap["range"] = range;
          }

          if (var.coordinates)
          {
            varMap["coordinates"] = *var.coordinates;
          }

          if (!var.chunks.empty())
          {
            py::list chunks;
            for (const auto& chunk : var.chunks)
            {
              chunks.append(chunk);
            }

            varMap["chunks"] = chunks;
          }

          if (var.compressionLevel)
          {
            varMap["compressionLevel"] = var.compressionLevel;
          }

          variables.append(varMap);
        }

        return variables;
      });
}
