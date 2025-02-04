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
        "Remove a global attribute from the description.")
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
