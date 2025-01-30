// (C) Copyright 2025 NOAA/NWS/NCEP/EMC

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>

#include <memory>
#include <string>

#include "DataObjectFunctions.h"

#include "bufr/encoders/EncoderBase.h"
#include "bufr/encoders/Description.h"
#include "bufr/QueryParser.h"

namespace py = pybind11;

using bufr::encoders::EncoderBase;
using bufr::encoders::EncoderDimension;
using bufr::encoders::EncoderDimensionPtr;
using bufr::encoders::EncoderDimensions;
using bufr::encoders::DimensionDescription;
using bufr::encoders::Description;
using bufr::Query;
using bufr::QueryParser;

namespace bufr {
namespace encoders {
  struct PyEncoderDimension : public EncoderDimension
  {
    PyEncoderDimension(std::shared_ptr<DimensionDataBase> dimObj,
                       const DimensionDescription& description,
                       const std::vector<std::string>& paths) :
        EncoderDimension(dimObj, description, paths) {}

    std::string name() { return description.name; };
    std::vector<int> labels;
  };

  typedef std::shared_ptr<PyEncoderDimension> PyEncoderDimensionPtr;

  namespace details
  {
    class LabelWriter : public ObjectWriter<int>
    {
     public:
      LabelWriter() = delete;
      LabelWriter(std::shared_ptr<PyEncoderDimension>& dim) : dim_(dim) {}

      void write(const std::vector<int>& data) final
      {
        dim_->labels = data;
      }

     private:
      std::shared_ptr<PyEncoderDimension>& dim_;
    };
  }

  struct PyEncoderDimensions : public EncoderDimensions
  {
    using EncoderDimensions::EncoderDimensions;

    PyEncoderDimensions() = delete;
    PyEncoderDimensions(const std::vector<PyEncoderDimensionPtr>& dims,
                        const Description& description,
                        const std::shared_ptr<DataContainer>& container,
                        const std::vector<std::string>& category) :
      EncoderDimensions(convertDims(dims), description, container, category) {}

   private:
    std::vector<EncoderDimensionPtr> convertDims(const std::vector<PyEncoderDimensionPtr>& pyDims)
    {
      std::vector<EncoderDimensionPtr> dims;
      for (const auto& dim : pyDims)
      {
        dims.push_back(std::dynamic_pointer_cast<EncoderDimension>(dim));
      }

      return dims;
    }
  };

  struct PyEncoderBase : public EncoderBase
  {
    using EncoderBase::EncoderBase;

    PyEncoderDimensions pyGetEncoderDimensions(const std::shared_ptr<DataContainer>& container,
                                               const std::vector<std::string>& category)
    {
      auto dims = EncoderBase::getEncoderDimensions(container, category);
      std::vector<std::shared_ptr<PyEncoderDimension>> pyDims;
      for (const auto& dim : dims.dims())
      {
        auto pyDim = std::make_shared<PyEncoderDimension>(dim->dimObj,
                                                                          dim->description,
                                                                          dim->paths);

        auto labelWriter = std::make_shared<details::LabelWriter>(pyDim);
        dim->dimObj->write(labelWriter);

        pyDims.push_back(pyDim);
      }

      return {pyDims, description_, container, category};
    }

//    PyEncoderDimension pyFindNamedDimForPath(const PyEncoderDimensions& dims,
//                                             const std::string& dim_path)
//    {
//      std::vector<EncoderDimensionPtr> eDims;
//      for (const auto& dim : dims.dims())
//      {
//        eDims.push_back(std::make_shared<EncoderDimension>(dim));
//      }
//
//      auto dim = findNamedDimForPath(eDims, dim_path);
//      if (dim)
//      {
//        for (auto& pyDim : dims.dims())
//        {
//          if (pyDim.description.name == (*dim)->description.name)
//          {
//            return pyDim;
//          }
//        }
//      }
//    }
  };
}  // namespace encoders
}  // namespace bufr

void setupEncoderBase(py::module& m)
{
  py::class_<bufr::encoders::PyEncoderDimension>(m, "EncoderDimension")
    .def("name", &bufr::encoders::PyEncoderDimension::name)
    .def_readwrite("labels", &bufr::encoders::PyEncoderDimension::labels)
    .def_readwrite("paths", &bufr::encoders::PyEncoderDimension::paths);

  py::class_<bufr::encoders::PyEncoderDimensions>(m, "EncoderDimensions")
    .def("dims",
          [](bufr::encoders::PyEncoderDimensions& self) -> std::vector<bufr::encoders::PyEncoderDimension>
          {
            std::vector<bufr::encoders::PyEncoderDimension> pyDims;
            for (const auto& dim : self.dims())
            {
              pyDims.push_back(*std::dynamic_pointer_cast<bufr::encoders::PyEncoderDimension>(dim));
            }
            return pyDims;
          });

  py::class_<bufr::encoders::PyEncoderBase>(m, "EncoderBase")
    .def(py::init<const std::string&>())
    .def(py::init<const bufr::encoders::Description&>())
    .def(py::init<const eckit::Configuration&>())
    .def("get_encoder_dimensions", &bufr::encoders::PyEncoderBase::pyGetEncoderDimensions)
    .def("find_named_dim_for_path",
         [](bufr::encoders::PyEncoderBase& self,
            bufr::encoders::PyEncoderDimensions& dims,
            const std::string& dim_path) -> bufr::encoders::PyEncoderDimension
         {
           std::vector<EncoderDimensionPtr> eDims;
           for (auto dim : dims.dims())
           {
             eDims.push_back(std::make_shared<EncoderDimension>(dim->dimObj,
                                                                dim->description,
                                                                dim->paths));
           }

           auto dim = EncoderBase::findNamedDimForPath(eDims, dim_path);
           if (dim)
           {
             for (auto dimPtr : dims.dims())
             {
               if (dimPtr->description.name == (*dim)->description.name)
               {
                 if (const auto pyDim =
                   std::dynamic_pointer_cast<bufr::encoders::PyEncoderDimension>(dimPtr))
                 {
                   return *pyDim;
                 }
                 else
                 {
                   throw eckit::BadParameter("Could not cast to PyEncoderDimension.");
                 }
               }
             }

           }
         });
}
