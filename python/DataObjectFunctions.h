/*
* (C) Copyright 2024 NOAA/NWS/NCEP/EMC
*
* This software is licensed under the terms of the Apache Licence Version 2.0
* which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
*/


#pragma once

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

#include "bufr/DataObject.h"

namespace py = pybind11;

namespace bufr {

  py::array pyArrayFromObj(const std::shared_ptr<DataObjectBase>& obj);

  template <typename T>
  py::array pyArrayFromObj(const std::shared_ptr<DataObject<T>>& obj) {
    auto data = obj->getRawData();

    // Create the data array
    py::array_t<T> pyData(obj->getDims());
    T* dataPtr = static_cast<T*>(pyData.mutable_data());
    std::copy(data.begin(), data.end(), dataPtr);

    // Create the mask array
    py::array_t<bool> mask(obj->getDims());
    bool* maskPtr = static_cast<bool*>(mask.mutable_data());
    for (size_t idx = 0; idx < data.size(); idx++) {
      maskPtr[idx] = obj->isMissing(idx);
    }

    // Create a masked array from the data and mask arrays
    py::object numpyModule = py::module::import("numpy");
    py::array maskedArray  = numpyModule.attr("ma").attr("masked_array")(pyData, mask);
    numpyModule.attr("ma").attr("set_fill_value")(maskedArray, DataObject<T>::missingValue());

    return maskedArray;
  }

  template <>
  py::array pyArrayFromObj<std::string>(const std::shared_ptr<DataObject<std::string>>& obj);

  template <typename T>
  std::shared_ptr<DataObjectBase> _makeObject(const std::string& fieldName,
                                              const py::array& pyData)
  {
    if (!pyData.dtype().is(py::dtype::of<T>())) {
      throw std::runtime_error("DataContainer::makeObject: Type mismatch");
    }

    auto dataObj = std::make_shared<DataObject<T>>();
    auto strData = std::vector<T>(static_cast<const T*>(pyData.data()),
                                  static_cast<const T*>(pyData.data()) + pyData.size());

    dataObj->setFieldName(fieldName);
    dataObj->setData(std::move(strData));
    dataObj->setDims(std::vector<int>(pyData.shape(), pyData.shape() + pyData.ndim()));
    dataObj->setDimPaths(std::vector<Query>(pyData.ndim()));

    return dataObj;
  }

  template <>
  std::shared_ptr<DataObjectBase> _makeObject<std::string>(const std::string& fieldName,
                                                           const py::array& pyData);

  std::shared_ptr<DataObjectBase> makeObject(const std::string& fieldName,
                                             const py::array& pyData);

}  // namespace bufr
