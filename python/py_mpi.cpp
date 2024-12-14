// (C) Copyright 2024 NOAA/NWS/NCEP/EMC

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <string>

#include "py_mpi.h"

#include "eckit/runtime/Main.h"

namespace py = pybind11;


namespace bufr {
namespace mpi {
  class App : public eckit::Main
  {
   public:
    App() = delete;
    App(int argc, char **argv) : eckit::Main(argc, argv)
    {
      name_ = "parallel_bufr_python";
    }
  };
}  // namespace mpi
}  // namespace bufr

void setupMpi(py::module& m)
{
  py::class_<bufr::mpi::App>(m, "App")
    .def(py::init([](std::vector<std::string>& argStrs)
    {

      std::vector<char*> argv;  // NOLINT(cppcoreguidelines-pro-type-cstyle-cast)
      for (const auto& argStr : argStrs)
      {
        argv.push_back(const_cast<char*>(argStr.c_str()));
      }

      return new bufr::mpi::App(static_cast<int>(argStrs.size()), argv.data());
    }),
    "constructor");

  py::class_<bufr::mpi::Comm>(m, "Comm")
    .def(py::init<const std::string&>())
    .def("name", &bufr::mpi::Comm::name)
    .def("rank", &bufr::mpi::Comm::rank)
    .def("size", &bufr::mpi::Comm::size);
}
