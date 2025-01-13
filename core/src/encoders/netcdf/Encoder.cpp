// (C) Copyright 2024 NOAA/NWS/NCEP/EMC

#include "bufr/encoders/netcdf/Encoder.h"

#include <chrono>  // NOLINT
#include <numeric>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#include <netcdf>

#include "eckit/exception/Exceptions.h"

#include "../../bufr/Log.h"
#include "bufr/DataObject.h"
#include "bufr/encoders/netcdf/NetcdfHelper.h"

namespace nc = netCDF;

namespace bufr {
namespace encoders {
namespace netcdf {
    static const char* LocationName = "Location";
    static const char* DefualtDimName = "dim";


    template<typename T>
    struct is_vector : public std::false_type {};

    template<typename T, typename A>
    struct is_vector<std::vector<T, A>> : public std::true_type {};

    template <typename T>
    class NcGlobalWriter : public GlobalWriter<T>
    {
    public:
      NcGlobalWriter() = delete;
      NcGlobalWriter(nc::NcFile& file) : file_(file) {}

      void write(const std::string& name, const T& data) final
      {
        _write(name, data);
      }

    private:
      nc::NcFile& file_;

      template<typename U = void>
      void _write(const std::string& name,
                  const T& data,
                  typename std::enable_if<!is_vector<T>::value, U>::type* = nullptr)
      {
        file_.putAtt(name, getNcType<T>(), data);
      }

      // T is a vector
      template<typename U = void>
      void _write(const std::string& name,
                  const T& data,
                  typename std::enable_if<is_vector<T>::value, U>::type* = nullptr)
      {
        file_.putAtt(name, getNcType<typename T::value_type>(), data.size(), data.data());
      }
    };

    template <>
    class NcGlobalWriter<std::string> : public GlobalWriter<std::string>
    {
    public:
      NcGlobalWriter() = delete;
      NcGlobalWriter(nc::NcFile& file) : file_(file) {}

      void write(const std::string& name, const std::string& data) final
      {
        _write(name, data);
      }

    private:
      nc::NcFile& file_;

      void _write(const std::string& name, const std::string& data)
      {
        file_.putAtt(name, data);
      }
    };

    template <typename T>
    class VarWriter : public ObjectWriter<T>
    {
    public:
        VarWriter() = delete;
        VarWriter(nc::NcVar& var) : var_(var) {}

        void write(const std::vector<T>& data) final
        {
            var_.putVar(data.data());
        }

    private:
        nc::NcVar& var_;
    };

    template <>
    class VarWriter<std::string> : public ObjectWriter<std::string>
    {
    public:
      VarWriter() = delete;
      VarWriter(nc::NcVar& var) : var_(var) {}

      void write(const std::vector<std::string>& data) final
      {
        auto startPositions = std::vector<size_t>(data.size(), 0);
        std::vector<size_t> lengths;
        for (const auto& str : data)
        {
          lengths.push_back(str.size());
        }

        auto c_strs = std::vector<const char*>(data.size());
        for (size_t i = 0; i < data.size(); i++)
        {
          c_strs[i] = data[i].c_str();
        }

        var_.putVar(c_strs.data());
      }

    private:
      nc::NcVar& var_;
    };

    template <typename T>
    nc::NcVar createVar(std::shared_ptr<DataObject<T>>& obj,
                        nc::NcGroup& group,
                        const std::string& name,
                        const std::vector<std::string>& dimNames,
                        std::vector<size_t>& chunks,
                        const int compressionLevel)
    {
        auto var = group.addVar(name, encoders::netcdf::getNcType<T>().getName(), dimNames);

        if (!chunks.empty())
        {
          var.setChunking(nc::NcVar::ChunkMode::nc_CHUNKED, chunks);
        }

        if (compressionLevel > 0)
        {
          var.setCompression(true, true, compressionLevel);
        }

        addAttribute(var, _FillValue, obj->missingValue());
        obj->write(std::make_shared<VarWriter<T>>(var));

        return var;
    }

    nc::NcVar createVarFromObj(std::shared_ptr<DataObjectBase> object,
                        nc::NcGroup& group,
                        const std::string& name,
                        const std::vector<std::string>& dimNames,
                        std::vector<size_t>& chunks,
                        const int compressionLevel)
    {
        nc::NcVar var;
        if (auto fltobj = std::dynamic_pointer_cast<DataObject<float>>(object))
        {
            var = createVar(fltobj, group, name, dimNames, chunks, compressionLevel);
        }
        else if (auto dblobj = std::dynamic_pointer_cast<DataObject<double>>(object))
        {
            var = createVar(dblobj, group, name, dimNames, chunks, compressionLevel);
        }
        else if (auto intobj = std::dynamic_pointer_cast<DataObject<int32_t >>(object))
        {
            var = createVar(intobj, group, name, dimNames, chunks, compressionLevel);
        }
        else if (auto uintobj = std::dynamic_pointer_cast<DataObject<uint32_t>>(object))
        {
            var = createVar(uintobj, group, name, dimNames, chunks, compressionLevel);
        }
        else if (auto int64obj = std::dynamic_pointer_cast<DataObject<int64_t>>(object))
        {
            var = createVar(int64obj, group, name, dimNames, chunks, compressionLevel);
        }
        else if (auto uint64obj = std::dynamic_pointer_cast<DataObject<uint64_t>>(object))
        {
            var = createVar(uint64obj, group, name, dimNames, chunks, compressionLevel);
        }
        else if (auto strobj = std::dynamic_pointer_cast<DataObject<std::string>>(object))
        {
            // Can not compress string data
            var = createVar(strobj, group, name, dimNames, chunks, 0);
        }
        else
        {
            throw eckit::BadParameter("Unsupported type for NetCDF.");
        }

        return var;
    }

    Encoder::Encoder(const std::string &yamlPath) : EncoderBase(yamlPath)
    {
    }

    Encoder::Encoder(const Description &description) : EncoderBase(description)
    {
    }

    Encoder::Encoder(const eckit::Configuration &conf) : EncoderBase(conf)
    {
    }

    std::map<SubCategory, std::shared_ptr<nc::NcFile>>
    Encoder::encode(const std::shared_ptr<DataContainer> &dataContainer,
                    const Encoder::Backend &backend,
                    bool append)
    {
        auto startTime = std::chrono::steady_clock::now();

        std::map<SubCategory, std::shared_ptr<nc::NcFile>> obsGroups;

        // Got through each unique category
        for (const auto &categories: dataContainer->allSubCategories())
        {
            auto dataObjectGroupBy = dataContainer->getGroupByObject(
                description_.getVariables()[0].source, categories);

            // When we find that the primary index is zero we need to skip this category
            if (dataObjectGroupBy->getDims()[0] == 0)
            {
                log::warning() << "Category (";
                for (auto category: categories)
                {
                    log::warning() << category;

                    if (category != categories.back())
                    {
                        log::warning() << ", ";
                    }
                }

                log::warning() << ") was not found in file." << std::endl;
            }

            // Make the categories
            size_t catIdx = 0;
            std::map<std::string, std::string> substitutions;
            for (const auto &catPair: dataContainer->getCategoryMap())
            {
                substitutions.insert({catPair.first, categories.at(catIdx)});
                catIdx++;
            }

            auto path = backend.path;
            if (path.empty())
            {
                path = makePathPrototype(substitutions);
            }

            auto fileName = makeStrWithSubstitions(path, substitutions);

            auto file = std::make_shared<nc::NcFile>();
            if (backend.isMemoryFile)
            {
              file->create(fileName, NC_NETCDF4 | NC_CLOBBER | NC_DISKLESS);
            }
            else
            {
              file->create(fileName, NC_NETCDF4 | NC_CLOBBER);
            }

            // Create the Globals
            for (auto &global: description_.getGlobals())
            {
              std::shared_ptr<GlobalWriterBase> writer = nullptr;
              if (auto intGlobal = std::dynamic_pointer_cast<GlobalDescription<int>>(global))
              {
                writer = std::make_shared<NcGlobalWriter<int>>(*file);
              }
              if (auto intGlobal =
                std::dynamic_pointer_cast<GlobalDescription<std::vector<int>>>(global))
              {
                writer = std::make_shared<NcGlobalWriter<std::vector<int>>>(*file);
              }
              else if (auto floatGlobal =
                std::dynamic_pointer_cast<GlobalDescription<float>>(global))
              {
                writer = std::make_shared<NcGlobalWriter<float>>(*file);
              }
              else if (auto floatGlobal =
                std::dynamic_pointer_cast<GlobalDescription<std::vector<float>>>(global))
              {
                writer = std::make_shared<NcGlobalWriter<std::vector<float>>>(*file);
              }
              else if (auto doubleGlobal =
                std::dynamic_pointer_cast<GlobalDescription<std::string>>(global))
              {
                writer = std::make_shared<NcGlobalWriter<std::string>>(*file);
              }

              global->writeTo(writer);
            }

            // Add Dimensions
            auto dims = getEncodedDimensions(dataContainer, categories);
            for (auto dim: dims)
            {
                const auto& ncDim = file->addDim(dim.dimObj->name, dim.dimObj->size());
                auto ncVar = file->addVar(dim.dimObj->name, nc::NcType::nc_INT, ncDim);
                addAttribute(ncVar, _FillValue, DataObject<int>::missingValue());
                dim.dimObj->write(std::make_shared<VarWriter<int>>(ncVar));
            }

            // Write all the other Variables
            std::set<std::string> groupNames;
            for (const auto &varDesc: description_.getVariables())
            {
                auto[groupName, varName] = splitName(varDesc.name);
                if (groupNames.find(groupName) == groupNames.end())
                {
                    file->addGroup(groupName);
                    groupNames.insert(groupName);
                }

                auto group = file->getGroup(groupName);
                std::vector<size_t> chunks = {};
                auto dimNames = std::vector<std::string>();
                auto dataObject = dataContainer->get(varDesc.source, categories);
                for (size_t dimIdx = 0; dimIdx < dataObject->getDims().size(); dimIdx++)
                {
                    auto dimPath = dataObject->getDimPaths()[dimIdx];
                    std::string dimName;
                    for (const auto& dim : dims)
                    {
                      for (const auto& knownDimPath : dim.paths)
                      {
                        if (knownDimPath== dimPath.str())
                        {
                          dimName = dim.dimObj->name;
                          break;
                        }
                      }

                      if (!dimName.empty())
                      {
                        break;
                      }
                    }

                    dimNames.push_back(dimName);

                    auto dimVar = group.getVar(dimName);

                    auto dimChunk = static_cast<size_t>(dataObject->getDims()[dimIdx]);
                    auto chunkMode = nc::NcVar::ChunkMode::nc_CHUNKED;
                    if (!dimVar.isNull())
                    {
                      std::vector<size_t> varChunks;
                      dimVar.getChunkingParameters(chunkMode, varChunks);
                      dimChunk = varChunks[dimIdx];
                    }

                    if (dimIdx < varDesc.chunks.size())
                    {
                      chunks.push_back(std::min(dimChunk, varDesc.chunks[dimIdx]));
                    }
                    else
                    {
                      chunks.push_back(dimChunk);
                    }
                }

                // Check that dateTime variable has the right dimensions
                if (varDesc.name == "MetaData/dateTime" || varDesc.name == "MetaData/datetime")
                {
                    if (dimNames.size() != 1)
                    {
                        throw eckit::BadParameter(
                            "Datetime variable must be one dimensional.");
                    }
                }

                auto var = createVarFromObj(dataObject,
                                            group,
                                            varName,
                                            dimNames,
                                            chunks,
                                            varDesc.compressionLevel);

                var.putAtt("long_name", varDesc.longName);
                if (!varDesc.units.empty())
                {
                    var.putAtt("units", varDesc.units);
                }

                if (varDesc.coordinates)
                {
                    var.putAtt("coordinates", *varDesc.coordinates);
                }

                if (varDesc.range)
                {
                    std::vector<float> range = {varDesc.range->start, varDesc.range->end};
                    var.putAtt("valid_range", getNcType<float>(), 2, range.data());
                }
            }

            obsGroups.insert({categories, file});
        }

        auto timeElapsed = std::chrono::steady_clock::now() - startTime;
        auto timeElapsedDuration = std::chrono::duration_cast<std::chrono::milliseconds>
          (timeElapsed);
        eckit::Log::info() << "Encoder Finished "
                           << "[" << timeElapsedDuration.count() / 1000.0 << "s]" << std::endl;

        return obsGroups;
    }

    std::string Encoder::makeStrWithSubstitions(const std::string &prototype,
                                                const std::map<std::string, std::string> &subMap)
    {
        auto resultStr = prototype;
        auto subIdxs = findSubIdxs(prototype);

        std::reverse(subIdxs.begin(), subIdxs.end());

        // Make sure that the prototype string has all the required substitutions defined
        for (const auto& sub : subMap)
        {
            auto key = sub.first;
            if (std::find_if(subIdxs.begin(),
                             subIdxs.end(),
                             [&key](const std::pair<std::string, std::pair<int, int>> &sub)
                             {
                                 return sub.first == key;
                             }) == subIdxs.end())
            {
                std::ostringstream errStr;
                errStr << "Prototype path string does not contain a substitution for " << key <<".";
                errStr << " example: " << makePathPrototype(subMap);
                throw eckit::BadParameter(errStr.str());
            }
        }

        for (const auto &subs: subIdxs)
        {
            if (subMap.find(subs.first) != subMap.end())
            {
                auto repIdxs = subs.second;
                resultStr = resultStr.replace(repIdxs.first,
                                              repIdxs.second - repIdxs.first + 1,
                                              subMap.at(subs.first));
            } else
            {
                std::ostringstream errStr;
                errStr << "Can not find " << subs.first << ". No category with that name.";
                throw eckit::BadParameter(errStr.str());
            }
        }

        return resultStr;
    }

    std::string Encoder::makePathPrototype(const std::map<std::string, std::string> &subMap) const
    {
      std::ostringstream prototype;
      prototype << "temporary";
      for (const auto &subs: subMap)
      {
        prototype << "_{" << subs.first << "}";
      }

      // add timestamp string
      prototype << "_" << std::to_string(std::time(nullptr));
      prototype << ".nc";

      return prototype.str();
    }

    std::vector<std::pair<std::string, std::pair<int, int>>>
    Encoder::findSubIdxs(const std::string &str)
    {
        std::vector<std::pair<std::string, std::pair<int, int>>> result;

        size_t startPos = 0;
        size_t endPos = 0;

        while (startPos < str.size())
        {
            startPos = str.find("{", startPos+1);

            if (startPos < str.size())
            {
                endPos = str.find("}", startPos+1);

                if (endPos < str.size())
                {
                    result.push_back({str.substr(startPos + 1, endPos - startPos - 1),
                                      {startPos, endPos}});
                    startPos = endPos;
                }
                else
                {
                    throw eckit::BadParameter("Unmatched { found in output filename.");
                }
            }
        }

        return result;
    }

    bool Encoder::existsInNamedPath(const Query &path, const NamedPathDims &pathMap) const
    {
        for (auto &paths: pathMap)
        {
            if (std::find(paths.first.begin(), paths.first.end(), path) != paths.first.end())
            {
                return true;
            }
        }

        return false;
    }

    DimensionDescription Encoder::dimForDimPath(const Query &path,
                                                const NamedPathDims &pathMap) const
    {
        DimensionDescription dimDesc;

        for (auto paths: pathMap)
        {
            if (std::find(paths.first.begin(), paths.first.end(), path) != paths.first.end())
            {
                dimDesc = paths.second;
                break;
            }
        }

        return dimDesc;
    }


    std::pair<std::string, std::string> Encoder::splitName(std::string name) const
    {
        std::string groupName;
        std::string varName;
        size_t pos = name.find_first_of("/@");
        if (pos != std::string::npos)
        {
            if (name[pos] == '/')
            {
                groupName = name.substr(0, pos);
                varName = name.substr(pos + 1);
            }
            else
            {
                groupName = name.substr(pos + 1);
                varName = name.substr(0, pos);
            }
        }
        else
        {
            throw eckit::BadParameter("Variable name must contain a group name");
        }

        return std::make_pair(groupName, varName);
    }
}  // namespace netcdf
}  // namespace encoders
}  // namespace bufr
