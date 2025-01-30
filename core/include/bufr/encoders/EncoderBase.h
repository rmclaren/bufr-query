// (C) Copyright 2024 NOAA/NWS/NCEP/EMC

#pragma once

#include <set>
#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "eckit/config/LocalConfiguration.h"

#include "bufr/DataContainer.h"
#include "bufr/QueryParser.h"
#include "bufr/encoders/Description.h"

namespace bufr {
namespace encoders {

  struct EncoderDimension
  {
    EncoderDimension() = delete;
    EncoderDimension(const std::shared_ptr<DimensionDataBase>& dimObj,
                     const DimensionDescription& description,
                     const std::vector<std::string>& paths) :
        dimObj(dimObj),
        description(description),
        paths(paths)
    {
    }

    virtual ~EncoderDimension() = default;

    std::shared_ptr<DimensionDataBase> dimObj;  // Dimension data
    DimensionDescription description;  // Dimension description
    std::vector<std::string> paths; // BUFR paths associated with the dimension
  };

  typedef std::shared_ptr<EncoderDimension> EncoderDimensionPtr;

  class EncoderDimensions
  {
   public:
      EncoderDimensions() = delete;
      EncoderDimensions(const std::vector<EncoderDimensionPtr>& dims,
                        const Description& description,
                        const std::shared_ptr<DataContainer>& container,
                        const std::vector<std::string>& category) :
          dims_(dims),
          varDimNameMap_(initVarDimNameMap(description, container, category)),
          varChunkMap_(initVarChunkMap(description, container, category))
      {
      }

      virtual ~EncoderDimensions() = default;

      std::vector<EncoderDimensionPtr> dims() { return dims_; };

      std::map<std::string, std::vector<std::string>> getVarDimNameMap() { return varDimNameMap_; }
      std::map<std::string, std::vector<size_t>> getVarChunkMap() { return varChunkMap_; }
      std::optional<EncoderDimensionPtr> findNamedDimForPath(const std::string& dim_path) const;

    private:
      const std::vector<EncoderDimensionPtr> dims_;
      const std::map<std::string, std::vector<std::string>> varDimNameMap_;
      const std::map<std::string, std::vector<size_t>> varChunkMap_;

      std::map<std::string, std::vector<std::string>> initVarDimNameMap(
                                                    const Description &description,
                                                    const std::shared_ptr<DataContainer>& container,
                                                    const std::vector<std::string>& category) const;

      std::map<std::string, std::vector<size_t>> initVarChunkMap(
                                                    const Description &description,
                                                    const std::shared_ptr<DataContainer>& container,
                                                    const std::vector<std::string>& category) const;
  };

  class EncoderBase
  {
   public:
    EncoderBase() = delete;

    explicit EncoderBase(const std::string &yamlPath);
    explicit EncoderBase(const Description &description);
    explicit EncoderBase(const eckit::Configuration &conf);

    virtual ~EncoderBase() = default;



   protected:
    /// \brief The description
    const Description description_;

    static std::optional<EncoderDimensionPtr> findNamedDimForPath(
      const std::vector<EncoderDimensionPtr>& dims,
      const std::string& dim_path);

    EncoderDimensions getEncoderDimensions(const std::shared_ptr<DataContainer>& container,
                                           const std::vector<std::string>& category) const;

   private:
    std::vector<int> patternToDimLabels(const std::string& str) const;

    size_t getPathSize(const std::shared_ptr<DataContainer>& container,
                       const std::vector<std::string>& paths,
                       const std::vector<std::string>& category) const;

    bool isDimPath(const std::shared_ptr<DataContainer>& container,
                   const std::vector<std::string>& paths,
                   const std::vector<std::string>& category) const;

  };
}  // namespace encoders
}  // namespace bufr
