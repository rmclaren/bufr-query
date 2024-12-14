/*
* (C) Copyright 2024 NOAA/NWS/NCEP/EMC
*
* This software is licensed under the terms of the Apache Licence Version 2.0
* which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
*/


#include <typeinfo>
#include <iostream>
#include <regex>  // NOLINT

#include "DataObjectFunctions.h"

#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>

namespace py = pybind11;

namespace bufr {

  static const std::regex strRegex("[|\\<\\>]?[US]\\d*");

  py::array pyArrayFromObj(const std::shared_ptr<DataObjectBase>& obj)
  {
    if (const auto& strObj = std::dynamic_pointer_cast<DataObject<std::string>>(obj))
    {
      return pyArrayFromObj(strObj);
    }
    else if (const auto& intObj = std::dynamic_pointer_cast<DataObject<int>>(obj))
    {
      return pyArrayFromObj(intObj);
    }
    else if (const auto& int64Obj = std::dynamic_pointer_cast<DataObject<int64_t>>(obj))
    {
      return pyArrayFromObj(int64Obj);
    }
    else if (const auto& floatObj = std::dynamic_pointer_cast<DataObject<float>>(obj))
    {
      return pyArrayFromObj(floatObj);
    }
    else if (const auto& doubleObj = std::dynamic_pointer_cast<DataObject<double>>(obj))
    {
      return pyArrayFromObj(doubleObj);
    }
    else
    {
      throw std::runtime_error("ResultSet Python Binding: Unsupported type encountered");
    }
  }

  template <>
  py::array pyArrayFromObj<std::string>(const std::shared_ptr<DataObject<std::string>>& obj)
  {
    auto data = obj->getRawData();
    py::list pyStrList(data.size());

    // Convert the std::vector<std::string> into a list of Python Unicode strings
    for (size_t i = 0; i < data.size(); ++i) {
      pyStrList[i] = py::str(data[i]);
    }

    // Create a NumPy array of Python Unicode strings with the correct dimensions
    py::object numpyModule = py::module::import("numpy");
    py::array pyData = numpyModule.attr("array")(pyStrList, py::dtype("O"));
    pyData = pyData.attr("reshape")(obj->getDims());

    // Create the mask array
    py::array_t<bool> mask(obj->getDims());
    bool* maskPtr = static_cast<bool*>(mask.mutable_data());
    for (size_t idx = 0; idx < data.size(); idx++)
    {
      maskPtr[idx] = obj->isMissing(idx);
    }

    // Create a masked array from the data and mask arrays
    py::array maskedArray = numpyModule.attr("ma").attr("masked_array")(pyData, mask);
    numpyModule.attr("ma").attr("set_fill_value")(maskedArray, "");

    return maskedArray;
  }

  std::shared_ptr<DataObjectBase> makeObject(const std::string& fieldName,
                                             const py::array& pyData) {
    std::shared_ptr<DataObjectBase> dataObj;

    py::dtype dt          = pyData.dtype();
    std::string dtype_str = py::cast<std::string>(py::str(dt));

    std::cmatch m;
    if (pyData.dtype().is(py::dtype::of<float>()))
    {
      dataObj = _makeObject<float>(fieldName, pyData);
    }
    else if (pyData.dtype().is(py::dtype::of<double>()))
    {
      dataObj = _makeObject<double>(fieldName, pyData);
    }
    else if (pyData.dtype().is(py::dtype::of<int>()))
    {
      dataObj = _makeObject<int>(fieldName, pyData);
    }
    else if (pyData.dtype().is(py::dtype::of<int64_t>()))
    {
      dataObj = _makeObject<int64_t>(fieldName, pyData);
    }
    else if (dtype_str == "object" || std::regex_match(dtype_str.c_str(), m, strRegex))
    {
      dataObj = _makeObject<std::string>(fieldName, pyData);
    }
    else
    {
      throw eckit::BadParameter("ERROR: Unsupported data type.");
    }

    return dataObj;
  }

  template <>
  std::shared_ptr<DataObjectBase> _makeObject<std::string>(const std::string& fieldName,
                                                           const py::array& pyData)
  {
    std::cmatch m;
    const auto dtype_str = py::cast<std::string>(py::str(pyData.dtype()));
    if (dtype_str != "object" && !std::regex_match(dtype_str.c_str(), m, strRegex))
    {
      throw std::runtime_error("DataContainer::makeObject: Type mismatch");
    }

    auto dataObj = std::make_shared<DataObject<std::string>>();

    std::vector<std::string> strVec(pyData.size());
    for (size_t i = 0; i < static_cast<size_t>(pyData.size()); i++)
    {
      py::object element = pyData.attr("__getitem__")(i);  // Get the element as a Python object
      strVec[i] = element.cast<py::str>();    // Cast it to py::str
    }

    dataObj->setFieldName(fieldName);
    dataObj->setData(std::move(strVec));
    dataObj->setDims(std::vector<int>(pyData.shape(), pyData.shape() + pyData.ndim()));
    dataObj->setDimPaths(std::vector<Query>(pyData.ndim()));

    return dataObj;
  }
}  // namespace bufr
