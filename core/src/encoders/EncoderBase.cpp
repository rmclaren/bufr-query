// (C) Copyright 2024 NOAA/NWS/NCEP/EMC

#include "bufr/encoders/EncoderBase.h"

#include <numeric>
#include <sstream>
#include <string>


namespace bufr {
namespace encoders {

  static const std::string LocationName = "Location";
  static const char* DefualtDimName = "dim";

  // EncoderDimension Methods

  std::map<std::string, std::vector<std::string>> EncoderDimensions::initVarDimNameMap(
                                                    const Description &description,
                                                    const std::shared_ptr<DataContainer>& container,
                                                    const std::vector<std::string>& category) const
  {
    auto varDimNameMap = std::map<std::string, std::vector<std::string>>();
    for (const auto& var : description.getVariables())
    {
      const auto varObj = container->get(var.source, category);
      varDimNameMap[var.name] = std::vector<std::string>();

      for (const auto& path : varObj->getDimPaths())
      {
        auto dim = findNamedDimForPath(path.str());
        if (dim)
        {
          varDimNameMap[var.name].push_back((*dim)->description.name);
        }
        else
        {
          std::ostringstream errStr;
          errStr << "Could not find dimension for path " << path.str();
          throw eckit::BadParameter(errStr.str());
        }
      }

      // Check that datetime variable has the rite number of dimensions
      if (var.name == "MetaData/dateTime" || var.name == "MetaData/datetime")
      {
        if (varDimNameMap[var.name].size() != 1)
        {
          throw eckit::BadParameter(
            "Datetime variable must be one dimensional.");
        }
      }
    }

    return varDimNameMap;
  }

  std::map<std::string, std::vector<size_t>> EncoderDimensions::initVarChunkMap(
                                                    const Description &description,
                                                    const std::shared_ptr<DataContainer>& container,
                                                    const std::vector<std::string>& category) const
  {
    auto varChunkMap = std::map<std::string, std::vector<size_t>>();
    for (const auto& var : description.getVariables())
    {
      const auto varObj = container->get(var.source, category);
      varChunkMap[var.name] = std::vector<size_t>();

      if (var.chunks.empty())
      {
        for (const auto& dimPath : varObj->getDimPaths())
        {
          auto dim = findNamedDimForPath(dimPath.str());
          if (dim)
          {
            varChunkMap[var.name].push_back((*dim)->dimObj->size());
          }
          else
          {
            std::ostringstream errStr;
            errStr << "Could not find dimension for path " << dimPath.str();
            throw eckit::BadParameter(errStr.str());
          }
        }
      }
      else
      {
        if (var.chunks.size() != varObj->getDimPaths().size())
        {
          std::ostringstream errStr;
          errStr << "Number of chunk sizes does not match the number of dimensions for variable ";
          errStr << var.name;
          throw eckit::BadParameter(errStr.str());
        }

        size_t dimIdx = 0;
        for (const auto& chunkSize : var.chunks)
        {
          varChunkMap[var.name].push_back(std::min(chunkSize, dims_[dimIdx]->dimObj->size()));
          dimIdx++;
        }
      }
    }

    return varChunkMap;
  }

  std::optional<EncoderDimensionPtr> EncoderDimensions::findNamedDimForPath(
    const std::string& dim_path) const
  {
    for (const auto& dim : dims_)
    {
      for (const auto& path : dim->paths)
      {
        if (path == dim_path)
        {
          return dim;
        }
      }
    }

    return std::nullopt;
  }

  // EncoderBase Methods
  EncoderBase::EncoderBase(const std::string &yamlPath) :
    description_(Description(yamlPath))
  {
  }

  EncoderBase::EncoderBase(const Description &description) :
    description_(description)
  {
  }

  EncoderBase::EncoderBase(const eckit::Configuration &conf) :
    description_(Description(conf))
  {
  }

  EncoderDimensions EncoderBase::getEncoderDimensions(
                                                    const std::shared_ptr<DataContainer>& container,
                                                    const std::vector<std::string>& category) const
  {
    std::vector<EncoderDimensionPtr> dims;

    // Add the root "Location" dimension as a named dimension
    auto rootLocation = DimensionDescription();
    rootLocation.name = LocationName;
    rootLocation.source = "";

    auto numLocs =
      container->get(container->getFieldNames().front(), category)->getDims().front();
    dims.push_back(std::make_shared<EncoderDimension>(
                                        std::make_shared<DimensionData<int>>(LocationName, numLocs),
                                        rootLocation,
                                        std::vector<std::string>{"*"}));

    // Add dimensions that are explicitly defined in the description (YAML file)
    for (const auto& descDim : description_.getDims())
    {
      std::vector<int> labels;

      auto paths = std::vector<std::string>{};
      for (const auto& path : descDim.paths)
      {
        paths.emplace_back(path.str());
      }

      // Skip the dimension if it is not in any data dimensioning path.
      // The dimension might have been removed via group_by, or that path might
      // not exist in the read data.
      if (!isDimPath(container, paths, category))
      {
        std::stringstream warningStr;
        warningStr << "WARNING: Dimension " << descDim.name << " has no query path for category (";

        for (const auto& cat : category)
        {
          warningStr << cat;

          if (cat != category.back())
          {
            warningStr << ", ";
          }
        }
        warningStr << ")";

        std::cout << warningStr.str() << std::endl;

        continue;
      }

      // Make the labels for explicitly defined dimensions.
      // Make the labels for dimensions from the "source" variable
      if (!descDim.source.empty())
      {
        auto dataObject = container->get(descDim.source, category);

        if (dataObject->size() == 0)
        {
          std::stringstream warningStr;
          warningStr << "WARNING: Dimension source ";
          warningStr << descDim.source << " has no data for category (";
          for (const auto& cat : category)
          {
            warningStr << cat;

            if (cat != category.back())
            {
              warningStr << ", ";
            }
          }
          warningStr << ")";
          std::cout << warningStr.str() << std::endl;
          continue;
        }

        // Validate the path for the source field makes sense for the dimension
        if (std::find(descDim.paths.begin(),
                      descDim.paths.end(),
                      dataObject->getDimPaths().back()) == descDim.paths.end())
        {
          std::stringstream errStr;
          errStr << "netcdf::dimensions: Source field " << descDim.source << " in ";
          errStr << descDim.name << " is not in the correct path.";
          throw eckit::BadParameter(errStr.str());
        }

        labels.resize(dataObject->getDims().back());
        if (const auto obj = std::dynamic_pointer_cast<DataObject<int>>(dataObject))
        {
          for (size_t idx = 0; idx < labels.size(); idx++)
          {
            labels[idx] = obj->getRawData()[idx];
          }
        }
        else
        {
          throw eckit::BadParameter("Dimension data type not supported.");
        }
      }
      // Create the labels for specified by the "labels" field
      else if (!descDim.labels.empty())
      {
        labels = patternToDimLabels(descDim.labels);

        const auto pathSize = getPathSize(container, paths, category);
        if (labels.size() != pathSize)
        {
          std::ostringstream errStr;
          errStr << "The number of labels (" << pathSize << ") ";
          errStr << "does not match the length of dimension \"" << descDim.name << "\".";
          throw eckit::BadParameter(errStr.str());
        }
      }
      // Set labels to the default (all 0's)
      else
      {
        auto pathLength = getPathSize(container, paths, category);
        labels.resize(pathLength, 0);
      }

      // Create the encoder dimension
      auto newDim = std::make_shared<EncoderDimension>(
        std::make_shared<DimensionData<int>>(descDim.name, labels),
        descDim,
        paths);

      dims.push_back(newDim);
    }

    // Find additional dimensions that are not explicitly defined in the description (YAML file)
    // but that are needed to encode the data.
    size_t dimIdx = 2;
    for (const auto &varDesc: description_.getVariables())
    {
      auto dataObject = container->get(varDesc.source, category);

      for (const auto &path: dataObject->getDimPaths())
      {
        if (!findNamedDimForPath(dims, path.str()))
        {
          auto labels = std::vector<int>(dataObject->getDims().back());

          auto newDimStr = std::ostringstream();
          newDimStr << DefualtDimName << "_" << dimIdx++;
          auto dimName = newDimStr.str();

          auto dimDesc = DimensionDescription();
          dimDesc.name = dimName;
          dimDesc.source = "";

          // Create the encoder dimension
          auto newDim = std::make_shared<EncoderDimension>(
            std::make_shared<DimensionData<int>>(dimName, labels),
            dimDesc,
            std::vector<std::string>{path.str()});

          dims.push_back(newDim);
        }
      }
    }

    return EncoderDimensions(dims, description_, container, category);
  }

  std::optional<EncoderDimensionPtr> EncoderBase::findNamedDimForPath(
    const std::vector<EncoderDimensionPtr>& dims,
    const std::string& dim_path)
  {
    auto it = std::find_if(dims.begin(), dims.end(), [&](const EncoderDimensionPtr& dim)
    {
      return std::find(dim->paths.begin(), dim->paths.end(), dim_path) != dim->paths.end();
    });
    return it != dims.end() ? std::optional<EncoderDimensionPtr>(*it) : std::nullopt;
  }

  std::vector<int> EncoderBase::patternToDimLabels(const std::string& str) const
  {
    auto cleanedStr = str;
    cleanedStr.erase(std::remove_if(cleanedStr.begin(), cleanedStr.end(), isspace),
                     cleanedStr.end());
    static const std::regex validationRegex(R"((\[)?\d+(-\d+)?(,\d+(-\d+)?)*(\])?)");

    std::vector<int> indices;

    // match index
    std::smatch matches;
    if (std::regex_match(cleanedStr, matches, validationRegex))
    {
      // regular expression pattern to match ranges and standalone numbers
      static const std::regex pattern(R"((\d+)-(\d+)|(\d+))");

      std::smatch match;

      // iterate over all matches in the input string
      for (auto it = std::sregex_iterator(cleanedStr.begin(), cleanedStr.end(), pattern);
           it != std::sregex_iterator(); ++it)
      {
        // check which capture group matched
        if ((*it)[1].matched && (*it)[2].matched)  // range match
        {
          int start = std::stoi((*it)[1].str());
          int end = std::stoi((*it)[2].str());
          for (int i = start; i <= end; i++)
          {
            indices.push_back(i);
          }
        }
        else  // standalone number match
        {
          int number = std::stoi((*it)[3].str());
          indices.push_back(number);
        }
      }
    }
    else
    {
      throw eckit::BadParameter("Pattern " + str + " in dimension label is invalid.");
    }

    return indices;
  }

  size_t EncoderBase::getPathSize(const std::shared_ptr<DataContainer>& container,
                                  const std::vector<std::string>& paths,
                                  const std::vector<std::string>& category) const
  {
    for (const auto& varName : container->getFieldNames())
    {
      auto dataObject = container->get(varName, category);
      for (const auto& path : paths)
      {
        size_t dimIdx = 0;
        for (const auto& dimPath : dataObject->getDimPaths())
        {
          if (dimPath.str() == path)
          {
            return dataObject->getDims()[dimIdx];
          }

          dimIdx++;
        }
      }
    }

    auto errStr = std::ostringstream {};
    errStr << "Couldn't find any given paths ";
    for (const auto& path : paths)
    {
      errStr << path << " ";
    }
    errStr << "in the container.";

    throw eckit::BadParameter(errStr.str());
  }

  bool EncoderBase::isDimPath(const std::shared_ptr<DataContainer>& container,
                              const std::vector<std::string>& paths,
                              const std::vector<std::string>& category) const
  {
    for (const auto &varName: container->getFieldNames())
    {
      auto dataObject = container->get(varName, category);
      for (const auto &path: paths)
      {
        for (const auto &dimPath: dataObject->getDimPaths())
        {
          if (dimPath.str() == path)
          {
            return true;
          }
        }
      }
    }

    return false;
  }
}  // namespace encoders
}  // namespace bufr
