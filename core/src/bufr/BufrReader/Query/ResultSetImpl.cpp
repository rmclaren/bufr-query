// (C) Copyright 2022 NOAA/NWS/NCEP/EMC

#include "ResultSetImpl.h"

#include <algorithm>
#include <iostream>
#include <string>

#include "eckit/exception/Exceptions.h"

#include "VectorMath.h"
#include "bufr/DataObject.h"
#include "../../DataObjectBuilder.h"


namespace bufr {
  std::shared_ptr<DataObjectBase> ResultSetImpl::get(const std::string& fieldName,
                                                     const std::string& groupByFieldName,
                                                     const std::string& overrideType)
{
    // Make sure we have accumulated frames.
    if (frames_.size() == 0)
    {
      auto data = details::ResultData();
      data.buffer = {};
      data.dims = {0};
      data.dimPaths = {Query()};

      auto object = DataObjectBuilder::make(fieldName,
                                            groupByFieldName,
                                            TypeInfo(),
                                            overrideType,
                                            data.buffer,
                                            data.dims,
                                            data.dimPaths);

      std::cout << "WARNING: ResultSet has no data." << std::endl;

      return object;
    }

    // Get the metadata for the target
    const auto targetMetaData = analyzeTarget(fieldName);

    // Assemble Result Data
    auto data = assembleData(targetMetaData);

    if (!groupByFieldName.empty()) {
      applyGroupBy(data, targetMetaData, groupByFieldName);
    }

    auto object = DataObjectBuilder::make(fieldName,
                                          groupByFieldName,
                                          targetMetaData->typeInfo,
                                          overrideType,
                                          data.buffer,
                                          data.dims,
                                          data.dimPaths);

    return object;
  }

  std::string ResultSetImpl::resolveType(const eckit::mpi::Comm& comm,
                                      const std::string& fieldName)
  {
    TypeInfo typeInfo;
    if (!frames_.empty())
    {
      typeInfo = analyzeTarget(fieldName)->typeInfo;
    }

    std::vector<int> bits(comm.size());
    std::vector<int> scale(comm.size());
    std::vector<int> reference(comm.size());

    comm.allGather(typeInfo.bits, bits.begin(), bits.end());
    comm.allGather(typeInfo.scale, scale.begin(), scale.end());
    comm.allGather(typeInfo.reference, reference.begin(), reference.end());

    std::vector<std::string> unit(comm.size());
    {
      size_t charsToSend = typeInfo.unit.size();

      size_t charsToReceive = charsToSend;
      comm.allReduce(charsToReceive, charsToReceive, eckit::mpi::Operation::SUM);

      auto sizeArray = std::vector<int>(comm.size());
      comm.allGather(static_cast<int>(charsToSend), sizeArray.begin(), sizeArray.end());

      std::vector<char> rcvBuffer(charsToReceive, 0);
      auto rcvCounts = std::vector<int>(comm.size());

      std::vector<int> displacement(comm.size(), 0);
      for (size_t i = 1; i < comm.size(); i++)
      {
        displacement[i] = displacement[i - 1] + sizeArray[i - 1];
      }

      comm.allGatherv(typeInfo.unit.begin(), typeInfo.unit.end(), rcvBuffer.begin(),
                      sizeArray.data(), displacement.data());

      for (size_t i = 0; i < comm.size(); i++)
      {
        unit[i] = std::string(rcvBuffer.begin() + displacement[i],
                              rcvBuffer.begin() + displacement[i] + sizeArray[i]);
      }
    }

    for (size_t bitIdx=0; bitIdx < bits.size(); ++bitIdx)
    {
      if (bits[bitIdx] != 0)
      {
        typeInfo.bits = bits[bitIdx];
        typeInfo.scale = scale[bitIdx];
        typeInfo.reference = reference[bitIdx];
        typeInfo.unit = unit[bitIdx];
        break;
      }
    }

    return DataObjectBuilder::typeString(typeInfo);
  }

  details::TargetMetaDataPtr ResultSetImpl::analyzeTarget(const std::string& name)
  {
    if (metaData_.find(name) != metaData_.end()) return metaData_[name];

    auto metaData       = std::make_shared<details::TargetMetaData>();
    metaData->targetIdx = frames_.front().getTargetIdx(name);
    metaData->missingFrames.resize(frames_.size(), false);

    // Loop through the frames to determine the overall parameters for the result data. We will
    // want to find the dimension information and determine if the array could be jagged which
    // means we will need to do extra work later (otherwise we can quickly copy the data).
    size_t frameIdx = 0;
    for (const auto& frame : frames_) {
      const auto& target = frame.targetAtIdx(metaData->targetIdx);

      if (target->path.size() == 0) {
        metaData->missingFrames[frameIdx] = true;
        ++frameIdx;
        continue;
      }

      if (target->path.size() - 1 > metaData->rawDims.size()) {
        metaData->rawDims.resize(target->path.size() - 1, 0);
      }

      // Resize the dims if necessary
      // Jagged if the dims need a resize (skip first one)
      if (target->exportDimIdxs.size() > metaData->dims.size()) {
        metaData->dims.resize(target->exportDimIdxs.size(), 1);
        metaData->filteredDims.resize(target->exportDimIdxs.size(), 0);
      }

      // Capture the dimensional information
      auto pathIdx      = 0;
      auto exportIdxIdx = 0;
      for (auto p = target->path.begin(); p != target->path.end() - 1; ++p) {
        const auto& counts = frame[p->nodeId].counts;
        if (counts.empty()) {
          metaData->missingFrames[frameIdx] = true;
          break;
        }

        const auto maxCount = std::max(max(counts), 1);
        if (maxCount > metaData->rawDims[pathIdx]) {
          metaData->rawDims[pathIdx] = maxCount;
        }

        if (target->exportDimIdxs.size() - 1 < static_cast<size_t>(exportIdxIdx))
        {
          ++pathIdx;
          continue;
        }

        if (target->exportDimIdxs[exportIdxIdx] != pathIdx)
        {
          ++pathIdx;
          continue;
        }

        const auto newDimVal = std::max(metaData->dims[exportIdxIdx], std::max(max(counts), 1));

        metaData->dims[exportIdxIdx] = newDimVal;

        // Capture the filtered dimension information
        if (!p->queryComponent->filter.empty()) {
          metaData->filteredDims[exportIdxIdx]
            = std::max(metaData->filteredDims[exportIdxIdx],
                       static_cast<int>(p->queryComponent->filter.size()));
        }

        pathIdx++;
        exportIdxIdx++;
      }

      // Fill in the type information
      metaData->typeInfo.reference
        = std::min(metaData->typeInfo.reference, target->typeInfo.reference);
      metaData->typeInfo.bits = std::max(metaData->typeInfo.bits, target->typeInfo.bits);

      if (std::abs(target->typeInfo.scale) > metaData->typeInfo.scale) {
        metaData->typeInfo.scale = target->typeInfo.scale;
      }

      if (metaData->typeInfo.unit.empty()) metaData->typeInfo.unit = target->typeInfo.unit;

      // Fill in the dimPaths data
      if (!target->dimPaths.empty() && metaData->dimPaths.size() < target->dimPaths.size()) {
        metaData->dimPaths = target->dimPaths;
      }

      ++frameIdx;
    }

    if (metaData->dimPaths.empty()) {
      metaData->dimPaths = {Query()};
    }

    // Fill the filtered dims array with the raw dims for elements that are not filtered
    for (size_t dimIdx = 0; dimIdx < metaData->filteredDims.size(); ++dimIdx) {
      if (metaData->filteredDims[dimIdx] == 0) {
        metaData->filteredDims[dimIdx] = metaData->dims[dimIdx];
      }
    }

    metaData_[name] = metaData;

    return metaData;
  }

  details::ResultData ResultSetImpl::assembleData(const details::TargetMetaDataPtr& metaData) const {
    int rowLength = 1;
    for (size_t dimIdx = 1; dimIdx < metaData->rawDims.size(); ++dimIdx) {
      rowLength *= metaData->rawDims[dimIdx];
    }

    // need to preserve one spot for the MissingValue even if there is no data
    rowLength = std::max(rowLength, 1);

    // Allocate the output data
    auto totalRows = frames_.size();
    auto data      = details::ResultData();
    data.buffer.isLongStr(metaData->typeInfo.isLongString());
    data.buffer.resize(totalRows * rowLength);
    data.dims     = metaData->dims;
    data.rawDims  = metaData->rawDims;
    data.dimPaths = metaData->dimPaths;

    // Update the dims to reflect the actual size of the data
    data.dims[0]    = totalRows;
    data.rawDims[0] = totalRows;

    bool needsFiltering = false;

    // Copy the data fragments into the raw data array.
    for (size_t frameIdx = 0; frameIdx < frames_.size(); ++frameIdx) {
      if (metaData->missingFrames[frameIdx]) {
        continue;
      }

      const auto& frame  = frames_[frameIdx];
      const auto& target = frame.targetAtIdx(metaData->targetIdx);
      copyData(data, frame, target, frameIdx * rowLength);

      if (target->usesFilters) needsFiltering = true;
    }

    if (needsFiltering) {
      int filteredRowLength = 1;
      for (size_t dimIdx = 1; dimIdx < metaData->filteredDims.size(); ++dimIdx) {
        filteredRowLength *= metaData->filteredDims[dimIdx];
      }

      auto filteredData = details::ResultData();
      filteredData.buffer.isLongStr(metaData->typeInfo.isLongString());
      filteredData.buffer.resize(totalRows * filteredRowLength);

      for (size_t frameIdx = 0; frameIdx < frames_.size(); ++frameIdx) {
        const auto& frame  = frames_[frameIdx];
        const auto& target = frame.targetAtIdx(metaData->targetIdx);

        size_t inputOffset  = frameIdx * rowLength;
        size_t outputOffset = frameIdx * filteredRowLength;
        size_t maxDepth     = target->path.size() - 1;

        copyFilteredData(filteredData, data, target, inputOffset, outputOffset, 1, maxDepth,
                         target->filterDataList, false);
      }

      filteredData.dimPaths = metaData->dimPaths;
      filteredData.dims     = metaData->filteredDims;
      filteredData.dims[0]  = data.dims[0];

      data = std::move(filteredData);
    }

    return data;
  }

  void ResultSetImpl::copyData(details::ResultData& data, const Frame& frame, const TargetPtr& target,
                           size_t outputOffset) const {
    size_t inputOffset = 0;
    size_t dimIdx      = 0;
    size_t countNumber = 1;
    size_t countOffset = 0;

    _copyData(data, frame, target, outputOffset, inputOffset, dimIdx, countNumber, countOffset);
  }

  void ResultSetImpl::_copyData(details::ResultData& data, const Frame& frame,
                            const TargetPtr& target, size_t& outputOffset, size_t& inputOffset,
                            const size_t dimIdx, const size_t countNumber,
                            const size_t countOffset) const {
    size_t totalDimSize = 1;
    for (size_t i = dimIdx; i < data.rawDims.size(); ++i) {
      totalDimSize *= data.rawDims[i];
    }

    if (!totalDimSize || dimIdx > data.rawDims.size() - 1 || frame[target->nodeIdx].data.empty())
      return;

    const auto& counts = frame[target->path[dimIdx].nodeId].counts;
    if (counts.empty()) {
      outputOffset += totalDimSize;
      return;
    }

    size_t newOffset = 0;
    for (size_t countIdx = 0; countIdx < countNumber; ++countIdx) {
      const auto& count = counts[countIdx + countOffset];
      if (count == 0) {
        outputOffset += totalDimSize;
        continue;
      }

      // When we reach the last layer of counts then copy the data
      // Ignore the subset path element (reason for -2)
      if (dimIdx == target->path.size() - 2) {
        const auto& fragment = frame[target->nodeIdx].data;

        if (fragment.isLongStr()) {
          std::copy(fragment.value.strings.begin() + inputOffset,
                    fragment.value.strings.begin() + inputOffset + count,
                    data.buffer.value.strings.begin() + outputOffset);
        } else {
          std::copy(fragment.value.octets.begin() + inputOffset,
                    fragment.value.octets.begin() + inputOffset + count,
                    data.buffer.value.octets.begin() + outputOffset);
        }

        inputOffset += count;
        outputOffset += totalDimSize;
      } else {
        _copyData(data, frame, target, outputOffset, inputOffset, dimIdx + 1, count, newOffset);
      }

      newOffset++;
    }
  }

  void ResultSetImpl::validateGroupByField(const details::TargetMetaDataPtr& targetMetaData,
                                       const details::TargetMetaDataPtr& groupByMetaData) const {
    // Validate the groupby field is in the same path as the field
    auto& groupByPath = groupByMetaData->dimPaths.back();
    auto& targetPath  = targetMetaData->dimPaths.back();

    auto groupByPathComps = splitPath(groupByPath.str());
    auto targetPathComps  = splitPath(targetPath.str());

    for (size_t i = 1; i < std::min(groupByPathComps.size(), targetPathComps.size()); i++) {
      if (targetPathComps[i] != groupByPathComps[i]) {
        std::ostringstream errStr;
        errStr << "The GroupBy and Target Fields do not share a common path.\n";
        errStr << "GroupByField path: " << groupByPath.str() << std::endl;
        errStr << "TargetField path: " << targetPath.str() << std::endl;
        throw eckit::BadParameter(errStr.str());
      }
    }
  }

  void ResultSetImpl::copyFilteredData(details::ResultData& resData,
                                   const details::ResultData& srcData, const TargetPtr& target,
                                   size_t& inputOffset, size_t& outputOffset, size_t depth,
                                   size_t maxDepth, const FilterDataList& filterDataList,
                                   bool skipResult) const {
    if (depth == maxDepth) {
      if (!skipResult) {
        if (resData.buffer.isLongStr()) {
          resData.buffer.value.strings[outputOffset] = srcData.buffer.value.strings[inputOffset];
        } else {
          resData.buffer.value.octets[outputOffset] = srcData.buffer.value.octets[inputOffset];
        }

        outputOffset++;
      }

      inputOffset++;

      return;
    }

    const auto& filterData = filterDataList[depth];

    if (filterData.isEmpty) {
      for (size_t count = 1; count <= static_cast<size_t>(srcData.rawDims[depth]); count++) {
        copyFilteredData(resData, srcData, target, inputOffset, outputOffset, depth + 1, maxDepth,
                         filterDataList, skipResult);
      }
    } else {
      size_t filterIdx     = 0;
      auto nextFilterCount = filterData.filter[filterIdx];
      for (size_t count = 1; count <= static_cast<size_t>(srcData.rawDims[depth]); count++) {
        bool skip = skipResult;
        if (!skip) {
          if (nextFilterCount == count) {
            filterIdx++;
            if (filterIdx < filterData.filter.size())
              nextFilterCount = filterData.filter.at(filterIdx);
          } else {
            skip = true;
          }
        }

        copyFilteredData(resData, srcData, target, inputOffset, outputOffset, depth + 1, maxDepth,
                         filterDataList, skip);
      }
    }
  }

  void ResultSetImpl::applyGroupBy(details::ResultData& resData,
                               const details::TargetMetaDataPtr& targetMetaData,
                               const std::string& groupByFieldName) {
    const auto groupByMetaData = analyzeTarget(groupByFieldName);
    validateGroupByField(targetMetaData, groupByMetaData);

    // If the groupby field has more dims than the target then we must duplicate the
    // target values to match the groupby field
    if (groupByMetaData->dims.size() > targetMetaData->dims.size()) {
      auto newData = details::ResultData();
      newData.buffer.isLongStr(resData.buffer.isLongStr());
      newData.dims = {resData.dims[0] * product(groupByMetaData->dims)};
      newData.buffer.resize(resData.dims[0] * product(groupByMetaData->dims));

      const auto numTargetVals = static_cast<size_t>(product(targetMetaData->dims));

      // There is no data
      if (numTargetVals == 0) {
        newData.dimPaths = {targetMetaData->dimPaths.back()};
        resData          = std::move(newData);
        return;
      }

      const auto numReps = static_cast<size_t>(product(groupByMetaData->dims) / numTargetVals);

      for (size_t targIdx = 0; targIdx < numTargetVals * resData.dims[0]; targIdx++) {
        for (size_t rep = 0; rep < numReps; rep++) {
          if (resData.buffer.isLongStr()) {
            newData.buffer.value.strings[targIdx * numReps + rep]
              = resData.buffer.value.strings[targIdx];
          } else {
            newData.buffer.value.octets[targIdx * numReps + rep]
              = resData.buffer.value.octets[targIdx];
          }
        }
      }

      newData.dimPaths = {targetMetaData->dimPaths.back()};
      resData          = std::move(newData);
    }
    // If the group_by field has less dims than the target data we only need to change the
    // dimensions around.
    else {
      const auto sizeDiff = targetMetaData->dims.size() - groupByMetaData->dims.size();
      auto newDims        = std::vector<int>(sizeDiff + 1);
      auto newDimPaths    = std::vector<Query>(sizeDiff + 1);

      newDims[0]     = resData.dims[0] * product(groupByMetaData->dims);
      newDimPaths[0] = targetMetaData->dimPaths.front();
      for (size_t i = 1; i < sizeDiff + 1; i++) {
        newDims[i]     = targetMetaData->dims[groupByMetaData->dims.size() + i - 1];
        newDimPaths[i] = targetMetaData->dimPaths[groupByMetaData->dims.size() + i - 1];
      }

      resData.dims     = std::move(newDims);
      resData.dimPaths = std::move(newDimPaths);
    }
  }

  std::string ResultSetImpl::unit(const std::string& fieldName) const {
    const auto targetIdx = frames_.front().getTargetIdx(fieldName);
    const auto& target   = frames_.front().targetAtIdx(targetIdx);
    return target->typeInfo.unit;
  }

  std::shared_ptr<DataObjectBase> ResultSetImpl::makeDataObject(
    const std::string& fieldName, const std::string& groupByFieldName, const TypeInfo& info,
    const std::string& overrideType, const Data& data, const std::vector<int>& dims,
    const std::vector<Query>& dimPaths) const {
    std::shared_ptr<DataObjectBase> object;
    if (overrideType.empty()) {
      object = objectByTypeInfo(info);
    } else {
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

  std::shared_ptr<DataObjectBase> ResultSetImpl::objectByTypeInfo(const TypeInfo& info) const {
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

  std::shared_ptr<DataObjectBase> ResultSetImpl::objectByType(const std::string& overrideType) const {
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

  std::vector<std::string> ResultSetImpl::splitPath(const std::string& path) {
    std::vector<std::string> components;
    std::string::size_type start = 0;
    std::string::size_type end   = 0;

    while ((end = path.find('/', start)) != std::string::npos) {
      if (end != start) {
        components.push_back(path.substr(start, end - start));
      }

      start = end + 1;
    }

    if (start < path.size()) {
      components.push_back(path.substr(start));
    }

    return components;
  }
}  // namespace bufr
