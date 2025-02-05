// (C) Copyright 2024 NOAA/NWS/NCEP/EMC

#pragma once

#include <memory>

#include "eckit/config/LocalConfiguration.h"
#include <netcdf>

#include "bufr/encoders/EncoderBase.h"
#include "bufr/DataContainer.h"
#include "bufr/encoders/Description.h"


namespace nc = netCDF;

namespace bufr {
namespace encoders {
namespace netcdf {
    /// \brief Uses netcdf::Description and parsed data to create NetCDF data.
    class Encoder : public EncoderBase
    {
    public:
        struct Backend
        {
            bool isMemoryFile;
            std::string path;

            Backend() : isMemoryFile(true), path("") {};

            Backend(bool isInMemory, const std::string &backendPath) :
                isMemoryFile(isInMemory),
                path(backendPath) {};
        };

        Encoder() = delete;

        explicit Encoder(const std::string &yamlPath);
        explicit Encoder(const Description &description);
        explicit Encoder(const eckit::Configuration &conf);

        /// \brief Encode the data into an netcdf NcFile object
        /// \param data The data container to use
        /// \param append Add data to existing file?
        std::map<SubCategory, std::shared_ptr<nc::NcFile>>
            encode(const std::shared_ptr<DataContainer> &data,
                   const Backend &backend = Backend(),
                   bool append = false);

    private:
        typedef std::map<std::vector<Query>, DimensionDescription> NamedPathDims;

        /// \brief Create a string from a template string.
        /// \param prototype A template string ex: "my {dogType} barks". Sections labeled {__key__}
        ///        are treated as keys into the dictionary that defines their replacment values.
        std::string makeStrWithSubstitions(const std::string &prototype,
                                           const std::map<std::string, std::string> &subMap);

        /// \brief Used to find indicies of { and } by the makeStrWithSubstitions method.
        /// \param str Template string to search.
        std::vector<std::pair<std::string, std::pair<int, int>>>
        findSubIdxs(const std::string &str);

        /// \brief Check if the subquery string is a named dimension.
        /// \param path The subquery string to check.
        /// \param pathMap The map of named dimensions.
        /// \return True if the subquery string is a named dimension.
        bool existsInNamedPath(const Query &path, const NamedPathDims &pathMap) const;

        /// \brief Get the description associated with the named dimension.
        /// \param path The subquery string for the dimension.
        /// \param pathMap The map of named dimensions.
        /// \return The dimension description associated with the named dimension.
        DimensionDescription dimForDimPath(const Query &path,
                                           const NamedPathDims &pathMap) const;

        /// \brief Split the variable name into group / name components
        /// \param name The variable name to split.
        std::pair<std::string, std::string> splitName(std::string name) const;

        /// \brief Make a prototype filename for the given subcategories.
        /// \param subMap The map of subcategories to use in the prototype.
        std::string makePathPrototype(const std::map<std::string, std::string> &subMap) const;
    };
}  // namespace netcdf
}  // namespace encoders
}  // namespace bufr
