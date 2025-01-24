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
    std::shared_ptr<DimensionDataBase> dimObj;  // Dimension data
    DimensionDescription description;  // Dimension description
    std::vector<std::string> paths; // BUFR paths associated with the dimension
  };

  class EncoderDimensions
  {
   public:
      EncoderDimensions() = delete;
      EncoderDimensions(const std::vector<EncoderDimension> dims,
                        const std::shared_ptr<DataContainer>& container,
                        const std::vector<std::string>& category) :
          dims_(dims),
          varDimNameMap_(makeVarDimNameMap(container, category)),
          varChunkMap_(makeVarChunkMap(container, category))
      {;
      }

      std::map<std::string, std::vector<std::string>> getVarDimNameMap() { return varDimNameMap_; }
      std::map<std::string, std::vector<size_t>> getVarChunkMap() { return varChunkMap_; }

    private:
      const std::vector<EncoderDimension> dims_;
      const std::map<std::string, std::vector<std::string>> varDimNameMap_;
      const std::map<std::string, std::vector<size_t>> varChunkMap_;

      std::map<std::string, std::vector<std::string>> makeVarDimNameMap(
                                                    const std::shared_ptr<DataContainer>& container,
                                                    const std::vector<std::string>& category);

      std::map<std::string, std::vector<size_t>> makeVarChunkMap(
                                                    const std::shared_ptr<DataContainer>& container,
                                                    const std::vector<std::string>& category);
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

    std::vector<EncoderDimension> getEncoderDimensions(
                                                    const std::shared_ptr<DataContainer>& container,
                                                    const std::vector<std::string>& category) const;

    std::map<std::string, std::vector<std::string>> getVariableDimNameMap(
                                                  const std::shared_ptr<DataContainer>& container,
                                                  const std::vector<std::string>& category,
                                                  const std::vector<EncoderDimension>& dims) const;

    std::map<std::string, std::vector<size_t>> getVariableChunkSizeMap(
                                                  const std::shared_ptr<DataContainer>& container,
                                                  const std::vector<std::string>& category,
                                                  const std::vector<EncoderDimension>& dims) const;

    std::optional<EncoderDimension> findNamedDimForPath(const std::vector<EncoderDimension>& dims,
                                                        const std::string& dim_path) const;

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
