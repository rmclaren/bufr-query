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
    std::vector<std::string> paths;  // BUFR paths associated with the dimension
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

    std::vector<EncoderDimension> getEncodedDimensions(
                                                    const std::shared_ptr<DataContainer>& container,
                                                    const std::vector<std::string>& category);

  private:
    std::vector<int> patternToDimLabels(const std::string& str) const;

    std::optional<EncoderDimension> findNamedDimForPath(const std::vector<EncoderDimension>& dims,
                                                        const std::string& dim_path) const;

    size_t getPathSize(const std::shared_ptr<DataContainer>& container,
                       const std::vector<std::string>& paths,
                       const std::vector<std::string>& category) const;

    bool isDimPath(const std::shared_ptr<DataContainer>& container,
                   const std::vector<std::string>& paths,
                   const std::vector<std::string>& category) const;
  };
}  // namespace encoders
}  // namespace bufr
