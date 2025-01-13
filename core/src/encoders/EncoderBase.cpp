// (C) Copyright 2024 NOAA/NWS/NCEP/EMC

#include "bufr/encoders/EncoderBase.h"

#include <numeric>
#include <sstream>
#include <string>


namespace bufr {
namespace encoders {

  static const std::string LocationName = "Location";
  static const char* DefualtDimName = "dim";

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

  std::vector<EncoderDimension> EncoderBase::getEncodedDimensions(
                                                    const std::shared_ptr<DataContainer>& container,
                                                    const std::vector<std::string>& category)
  {
    std::vector<EncoderDimension> dims;

    // Add the root Location dimension as a named dimension
    auto rootLocation = DimensionDescription();
    rootLocation.name = LocationName;
    rootLocation.source = "";

    // Add the Location dimension
    auto numLocs = container->get(container->getFieldNames().front(), category)->getDims().front();
    dims.push_back(EncoderDimension{
      std::make_shared<DimensionData<int>>(LocationName, numLocs),
      rootLocation,
      std::vector<std::string>{"*"}});

    // Find the named dimensions
    auto findNamedDimForPath = [&dims = dims](const std::string& dim_path) -> std::optional<EncoderDimension>
    {
      for (const auto& dim : dims)
      {
        for (const auto& path : dim.paths)
        {
          if (path == dim_path)
          {
            return dim;
          }
        }
      }

      return std::nullopt;
    };

    auto getPathSize = [&container = container](const std::vector<std::string>& paths,
      const std::vector<std::string>& category) -> size_t
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
    };

    auto isDimPath = [&container = container](const std::vector<std::string>& paths,
                                              const std::vector<std::string>& category) -> bool
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
    };

    for (const auto& descDim : description_.getDims())
    {
      std::vector<int> labels;

      auto paths = std::vector<std::string>{};
      for (const auto& path : descDim.paths)
      {
        paths.emplace_back(path.str());
      }

      // Skip the dimension if it is not in any data path
      if (!isDimPath(paths, category))
      {
        continue;
      }

      if (!descDim.source.empty())
      {
        auto dataObject = container->get(descDim.source, category);

        if (dataObject->size() == 0)
        {
          std::stringstream warningStr;
          warningStr << "WARNING: Dimension source " << descDim.source << " has no data for category (";
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
      else if (!descDim.labels.empty())
      {
        labels = patternToDimLabels(descDim.labels);

        if (labels.size() != getPathSize(paths, category))
        {
          throw eckit::BadParameter("Number of labels does not match the length of the path.");
        }
      }
      else
      {
        auto pathLength = getPathSize(paths, category);
        labels.resize(pathLength, 0);
      }

      auto newDim = EncoderDimension{};
      newDim.dimObj = std::make_shared<DimensionData<int>>(descDim.name, labels);
      newDim.description = descDim;
      newDim.paths = paths;

      dims.push_back(newDim);
    }

    size_t dimIdx = 1;
    for (const auto& varName : container->getFieldNames())
    {
      const auto dataObject = container->get(varName, category);
      for (const auto &path: dataObject->getDimPaths())
      {
        if (!findNamedDimForPath(path.str()))
        {
          auto labels = std::vector<int>(dataObject->getDims().back());

          auto newDimStr = std::ostringstream();
          newDimStr << DefualtDimName << "_" << dimIdx++;
          auto dimName = newDimStr.str();

          auto dimDesc = DimensionDescription();
          dimDesc.name = dimName;
          dimDesc.source = "";

          auto newDim = EncoderDimension{};
          newDim.dimObj = std::make_shared<DimensionData<int>>(dimName, labels);
          newDim.description = dimDesc;
          newDim.paths = std::vector<std::string>{path.str()};
          dims.push_back(newDim);
        }
      }
    }

    return dims;
  }

  std::vector<int> EncoderBase::patternToDimLabels(const std::string& str) const
  {
    static const std::regex validationRegex("\\[\\d+(\\-\\d+)?)(,\\d+(\\-\\d+)?)*\\]");

    std::vector<int> indices;

    // match index
    std::smatch matches;
    if (std::regex_match(str, matches, validationRegex))
    {
      // regular expression pattern to match ranges and standalone numbers
      static const std::regex pattern("(\\d+)-(\\d+)|(\\d+)");

      std::smatch match;

      // iterate over all matches in the input string
      for (auto it = std::sregex_iterator(str.begin(), str.end(), pattern);
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
}  // namespace encoders
}  // namespace bufr
