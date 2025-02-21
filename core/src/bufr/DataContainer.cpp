// (C) Copyright 2020 NOAA/NWS/NCEP/EMC

#include "bufr/DataContainer.h"

#include <string>
#include <ostream>

#include "eckit/exception/Exceptions.h"


namespace bufr {
  DataContainer::DataContainer() : categoryMap_({}) { makeDataSets(); }

  DataContainer::DataContainer(const CategoryMap& categoryMap) : categoryMap_(categoryMap) {
    makeDataSets();
  }

  void DataContainer::add(const std::string& fieldName, const std::shared_ptr<DataObjectBase> data,
                          const SubCategory& categoryId) {
    if (hasKey(dropPath(fieldName), categoryId)) {
      std::ostringstream errorStr;
      errorStr << "ERROR: Field called " << fieldName << " already exists ";
      errorStr << "for subcategory " << makeSubCategoryStr(categoryId) << std::endl;
      throw eckit::BadParameter(errorStr.str());
    }

    dataSets_.at(categoryId).insert({dropPath(fieldName), data});
  }

  void DataContainer::set(std::shared_ptr<DataObjectBase> data, const std::string& fieldName,
           const SubCategory& categoryId)
  {
    if (!hasKey(dropPath(fieldName), categoryId)) {
        std::ostringstream errStr;
        errStr << "ERROR: Either field called " << fieldName;
        errStr << " or category " << makeSubCategoryStr(categoryId);
        errStr << " does not exist. Cannot set non-existent field.";

        throw eckit::BadParameter(errStr.str());
    }

    dataSets_.at(categoryId).at(dropPath(fieldName)) = data;
  }

  std::shared_ptr<DataObjectBase> DataContainer::get(const std::string& fieldName,
                                                     const SubCategory& categoryId) const {
    if (!hasKey(dropPath(fieldName), categoryId)) {
      std::ostringstream errStr;
      errStr << "ERROR: Either field called " << fieldName;
      errStr << " or category " << makeSubCategoryStr(categoryId);
      errStr << " does not exist.";

      throw eckit::BadParameter(errStr.str());
    }

    return dataSets_.at(categoryId).at(dropPath(fieldName));
  }

  std::vector<std::string> DataContainer::getPaths(const std::string& fieldName,
                                                   const SubCategory& categoryId) const
  {
    auto dimPaths = get(dropPath(fieldName), categoryId)->getDimPaths();
    std::vector<std::string> paths(dimPaths.size());
    for (size_t pathIdx = 0; pathIdx < dimPaths.size(); pathIdx++)
    {
      paths[pathIdx] = dimPaths[pathIdx].str();
    }

    return paths;
  }

  std::shared_ptr<DataObjectBase> DataContainer::getGroupByObject(
    const std::string& fieldName, const SubCategory& categoryId) const {
    if (!hasKey(dropPath(fieldName), categoryId)) {
      std::ostringstream errStr;
      errStr << "ERROR: Either field called " << fieldName;
      errStr << " or category " << makeSubCategoryStr(categoryId);
      errStr << " does not exist.";

      throw eckit::BadParameter(errStr.str());
    }

    auto& dataObject             = dataSets_.at(categoryId).at(dropPath(fieldName));
    const auto& groupByFieldName = dataObject->getGroupByFieldName();

    std::shared_ptr<DataObjectBase> groupByObject = dataObject;
    if (!groupByFieldName.empty()) {
      for (const auto& obj : dataSets_.at(categoryId)) {
        if (obj.second->getFieldName() == groupByFieldName) {
          groupByObject = obj.second;
          break;
        }
      }
    }

    return groupByObject;
  }

  bool DataContainer::hasKey(const std::string& fieldName, const SubCategory& categoryId) const {
    bool hasKey = false;
    if (dataSets_.find(categoryId) != dataSets_.end()
        && dataSets_.at(categoryId).find(dropPath(fieldName)) != dataSets_.at(categoryId).end()) {
      hasKey = true;
    }

    return hasKey;
  }

  bool DataContainer::hasCategory(const SubCategory& categoryId) const
  {
    bool hasCat = false;
    if (dataSets_.find(categoryId) != dataSets_.end())
    {
      hasCat = true;
    }

    return hasCat;
  }

  std::shared_ptr<DataContainer> DataContainer::getSubContainer(const SubCategory& categoryId) const
  {
    std::shared_ptr<DataContainer> subCategory = nullptr;
    if (dataSets_.find(categoryId) != dataSets_.end())
    {
      auto newCategoryMap = CategoryMap();
      size_t catIdx = 0;
      for (const auto& category : categoryMap_)
      {
        newCategoryMap[category.first] = {categoryId[catIdx]};
        catIdx++;
      }

      subCategory = std::make_shared<DataContainer>(newCategoryMap);
      for (const auto& field : dataSets_.at(categoryId))
      {
        subCategory->add(field.first, field.second, categoryId);
      }
    }
    else
    {
      std::ostringstream errStr;
      errStr << "ERROR: Category called " << makeSubCategoryStr(categoryId);
      errStr << " does not exist.";

      throw eckit::BadParameter(errStr.str());
    }

    return subCategory;
  }

  size_t DataContainer::size(const SubCategory& categoryId) const {
    if (dataSets_.find(categoryId) == dataSets_.end()) {
      std::ostringstream errStr;
      errStr << "ERROR: Category called " << makeSubCategoryStr(categoryId);
      errStr << " does not exist.";

      throw eckit::BadParameter(errStr.str());
    }

    return dataSets_.at(categoryId).begin()->second->getDims().at(0);
  }

  std::vector<SubCategory> DataContainer::allSubCategories() const {
    std::vector<SubCategory> allCategories;

    for (const auto& dataSetPair : dataSets_) {
      allCategories.push_back(dataSetPair.first);
    }

    return allCategories;
  }

  void DataContainer::makeDataSets() {
    std::function<void(std::vector<size_t>&, const std::vector<size_t>&, size_t)> incIdx;
    incIdx
      = [&incIdx](std::vector<size_t>& indicies, const std::vector<size_t>& lengths, size_t idx) {
          if (indicies[idx] + 1 >= lengths[idx]) {
            if (idx + 1 < indicies.size()) {
              indicies[idx] = 0;
              incIdx(indicies, lengths, idx + 1);
            }
          } else {
            indicies[idx]++;
          }
        };

    size_t numCombos = 1;
    std::vector<size_t> indicies;
    std::vector<size_t> lengths;
    for (const auto& category : categoryMap_) {
      indicies.push_back(0);
      lengths.push_back(category.second.size());
      numCombos = numCombos * category.second.size();
    }

    if (!indicies.empty()) {
      for (size_t idx = 0; idx < numCombos; idx++) {
        size_t catIdx = 0;
        std::vector<std::string> subsets;
        for (const auto& category : categoryMap_) {
          subsets.push_back(category.second[indicies[catIdx]]);
          catIdx++;
        }

        dataSets_.insert({subsets, DataSetMap()});
        incIdx(indicies, lengths, 0);
      }
    } else {
      dataSets_.insert({{}, DataSetMap()});
    }
  }

  std::string DataContainer::makeSubCategoryStr(const SubCategory& categoryId) {
    std::ostringstream catStr;

    if (!categoryId.empty()) {
      for (const auto& subCategory : categoryId) {
        catStr << subCategory << "_";
      }
    } else {
      catStr << "__MAIN__";
    }

    return catStr.str();
  }

  std::vector<std::string> DataContainer::getFieldNames() const
  {
    std::vector<std::string> fieldNames;
    for (const auto& field : dataSets_.begin()->second)
    {
      fieldNames.push_back(field.first);
    }

    return fieldNames;
  }

  void DataContainer::append(const DataContainer& other)
  {
    bool isEmpty = getFieldNames().empty();

    for (const auto &subCat: other.allSubCategories())
    {
      if (isEmpty)
      {
        categoryMap_ = other.categoryMap_;
        dataSets_.insert({subCat, DataSetMap()});
      }

      for (const auto &field: other.getFieldNames())
      {
        if (isEmpty)
        {
          add(field, other.get(field, subCat)->copy(), subCat);
        }
        else
        {
          if (!hasKey(field, subCat))
          {
            std::ostringstream errStr;
            errStr << "Error: encountered mismatch when combining DataContainers.";
            errStr << " Field \"" << field << "\" category \"" << makeSubCategoryStr(subCat)
                   << "\"";
            throw eckit::BadParameter(errStr.str());
          }

          get(field, subCat)->append(other.get(field, subCat));
        }
      }
    }
  }


  void DataContainer::gather(const eckit::mpi::Comm& comm)
  {
    for (const auto &subCat: allSubCategories())
    {
      for (const auto &field: getFieldNames())
      {
        auto data = get(field, subCat);
        data->gather(comm);
      }
    }
  }

  void DataContainer::allGather(const eckit::mpi::Comm& comm)
  {
    for (const auto &subCat: allSubCategories())
    {
      for (const auto &field: getFieldNames())
      {
        auto data = get(field, subCat);
        data->allGather(comm);
      }
    }
  }

  std::string DataContainer::dropPath(const std::string& fieldName)
  {
    size_t pos = fieldName.find_last_of("/");
    if (pos == std::string::npos)
    {
      return fieldName;
    }

    return fieldName.substr(pos + 1);
  }
}  // namespace bufr
