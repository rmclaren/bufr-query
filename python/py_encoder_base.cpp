// (C) Copyright 2025 NOAA/NWS/NCEP/EMC

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <string>

#include "bufr/encoders/EncoderBase.h"

namespace py = pybind11;

using bufr::encoders::EncoderBase;
using bufr::encoders::EncoderDimension;

namespace bufr {
namespace mpi
{
  struct pyEncoderDimension : public EncoderDimension
  {
    std::string name() { return description.name; };
    std::vector<int> labels;
  };

  namespace details
  {
    class LabelWriter : public ObjectWriter<int>
    {
     public:
      LabelWriter() = delete;
      LabelWriter(pyEncoderDimension& dim) : dim_(dim) {}

      void write(const std::vector<int>& data) final
      {
        dim_.labels = data;
      }

     private:
      pyEncoderDimension& dim_;
    };
  }

  struct PyEncoderBase : public EncoderBase
  {
    using EncoderBase::EncoderBase;

    std::vector<pyEncoderDimension> pyGetEncoderDimensions(
                                                    const std::shared_ptr<DataContainer>& container,
                                                    const std::vector<std::string>& category)
    {
      auto dims = EncoderBase::getEncoderDimensions(container, category);
      std::vector<pyEncoderDimension> pyDims;
      for (const auto& dim : dims)
      {
        pyEncoderDimension pyDim;
        pyDim.description = dim.description;
        pyDim.paths = dim.paths;
        auto labelWriter = std::make_shared<details::LabelWriter>(pyDim);
        dim.dimObj->write(labelWriter);
        pyDims.push_back(pyDim);
      }

      return pyDims;
    }

    std::optional<pyEncoderDimension> pyFindNamedDimForPath(
                                                        const std::vector<pyEncoderDimension>& dims,
                                                        const std::string& dim_path)
    {
      std::vector<EncoderDimension> eDims;
      for (const auto& dim : dims)
      {
        eDims.push_back(dim);
      }

      auto dim = EncoderBase::findNamedDimForPath(eDims, dim_path);
      if (dim)
      {
        for (auto& pyDim : dims)
        {
          if (pyDim.description.name == dim->description.name)
          {
            return pyDim;
          }
        }
      }

      return std::nullopt;
    }
  };
} // namespace mpi
}  // namespace bufr

void setupEncoderBase(py::module& m)
{
  py::class_<bufr::mpi::pyEncoderDimension>(m, "EncoderDimension")
    .def(py::init<>())
    .def("name", &bufr::mpi::pyEncoderDimension::name)
    .def_readwrite("labels", &bufr::mpi::pyEncoderDimension::labels)
    .def_readwrite("paths", &bufr::mpi::pyEncoderDimension::paths);

  py::class_<bufr::mpi::PyEncoderBase>(m, "EncoderBase")
    .def(py::init<const std::string&>())
    .def(py::init<const bufr::encoders::Description&>())
    .def(py::init<const eckit::Configuration&>())
    .def("get_encoder_dimensions", &bufr::mpi::PyEncoderBase::pyGetEncoderDimensions)
    .def("find_named_dim_for_path", &bufr::mpi::PyEncoderBase::pyFindNamedDimForPath);
}
