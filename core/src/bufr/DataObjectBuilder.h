// (C) Copyright 2024 NOAA/NWS/NCEP/EMC

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "bufr/DataProvider.h"
#include "bufr/DataObject.h"
#include "bufr/SubsetTable.h"


namespace bufr {

  class DataObjectBuilder
  {
  public:
    static std::shared_ptr<DataObjectBase> make(const std::string& fieldName,
                                                const std::string& groupByFieldName,
                                                const TypeInfo& info,
                                                const std::string& overrideType,
                                                const Data& data,
                                                const std::vector<int>& dims,
                                                const std::vector<Query>& dimPaths)
    {
      std::shared_ptr<DataObjectBase> object = nullptr;
      if (overrideType.empty())
      {
        object = objectByTypeInfo(info);
      }
      else
      {
        object = objectByType(overrideType);

        if ((overrideType == "string" && !info.isString())
            || (overrideType != "string" && info.isString())) {
          std::ostringstream errMsg;
          errMsg << "Conversions between numbers and strings are not currently supported. ";
          errMsg << "See the export definition for \"" << fieldName << "\".";
          throw eckit::BadParameter(errMsg.str());
        }
      }

      object->setData(data);
      object->setDims(dims);
      object->setFieldName(fieldName);
      object->setGroupByFieldName(groupByFieldName);
      object->setDimPaths(dimPaths);

      return object;
    }

    template<typename T>
    static std::shared_ptr<DataObjectBase>  make(const std::vector<T>& data,
                                                const std::string& fieldName,
                                                const std::string& groupByFieldName,
                                                const std::vector<int>& dims,
                                                const std::string& query,
                                                const std::vector<Query>& dimPaths)
    {
      std::shared_ptr<DataObject<T>> object = std::make_shared<DataObject<T>>();
      object->setData(data);
      object->setDims(dims);
      object->setFieldName(fieldName);
      object->setGroupByFieldName(groupByFieldName);
      object->setDimPaths(dimPaths);
      object->setQuery(query);

      return object;
    }

    static std::string typeString(const TypeInfo& info)
    {
      std::string typeString;
      if (info.isString()) {
        typeString = "string";
      } else if (info.isInteger()) {
        if (info.isSigned()) {
          typeString = info.is64Bit() ? "int64" : "int32";
        } else {
          typeString = info.is64Bit() ? "uint64" : "uint32";
        }
      } else {
        typeString = info.is64Bit() ? "double" : "float";
      }

      return typeString;
    }

  private:

    static std::shared_ptr<DataObjectBase> objectByTypeInfo(const TypeInfo& info)
    {
      std::shared_ptr<DataObjectBase> object;

      if (info.isString() || info.isLongString()) {
        object = std::make_shared<DataObject<std::string>>();
      } else if (info.isInteger()) {
        if (info.isSigned()) {
          if (info.is64Bit()) {
            object = std::make_shared<DataObject<int64_t>>();
          } else {
            object = std::make_shared<DataObject<int32_t>>();
          }
        } else {
          if (info.is64Bit()) {
            object = std::make_shared<DataObject<uint64_t>>();
          } else {
            object = std::make_shared<DataObject<uint32_t>>();
          }
        }
      } else {
        if (info.is64Bit()) {
          object = std::make_shared<DataObject<double>>();
        } else {
          object = std::make_shared<DataObject<float>>();
        }
      }

      return object;
    }

    static std::shared_ptr<DataObjectBase> objectByType(const std::string& overrideType)
    {
      std::shared_ptr<DataObjectBase> object;

      if (overrideType == "int" || overrideType == "int32") {
        object = std::make_shared<DataObject<int32_t>>();
      } else if (overrideType == "float" || overrideType == "float32") {
        object = std::make_shared<DataObject<float>>();
      } else if (overrideType == "double" || overrideType == "float64") {
        object = std::make_shared<DataObject<double>>();
      } else if (overrideType == "string") {
        object = std::make_shared<DataObject<std::string>>();
      } else if (overrideType == "int64") {
        object = std::make_shared<DataObject<int64_t>>();
      } else if (overrideType == "uint64") {
        object = std::make_shared<DataObject<uint64_t>>();
      } else if (overrideType == "uint32" || overrideType == "uint") {
        object = std::make_shared<DataObject<uint32_t>>();
      } else {
        std::ostringstream errMsg;
        errMsg << "Unknown or unsupported type " << overrideType << ".";
        throw eckit::BadParameter(errMsg.str());
      }

      return object;
    }
  };

}  // namespace bufr
