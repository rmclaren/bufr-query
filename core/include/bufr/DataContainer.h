// (C) Copyright 2020 NOAA/NWS/NCEP/EMC

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "BufrTypes.h"
#include "DataObject.h"

#include "eckit/mpi/Comm.h"

namespace bufr {
  /// List of possible category strings (for splitting data)
  typedef std::vector<std::string> SubCategory;

  /// Map of data set id's to vector of possible value strings
  typedef std::map<std::string, SubCategory> CategoryMap;

  /// Map string paths (ex: variable/radiance) to DataObject
  typedef std::map<std::string, std::shared_ptr<DataObjectBase>> DataSetMap;

  /// Map category combo (ex: SatId/sat_1, GeoBox/lat_25_30__lon_23_26) to the relevant DataSetMap
  typedef std::map<std::vector<std::string>, DataSetMap> DataSets;

  /// \brief Collection of DataObjects that a Parser collected identified by their exported name
  class DataContainer {
  public:
    /// \brief Simple constructor
    DataContainer();

    /// \brief Construct to create container with subcategories.
    /// \details constructor that creates a underlying data structure to store data in separate
    ///          sub categories defined by combining all possible combinations of categories
    ///          defined in the category map.
    /// \param categoryMap map of major category types ex: "SatId" to the possible sub types
    ///        for the category type ex: {"GOES-15", "GOES-16", "GOES-17"}.
    explicit DataContainer(const CategoryMap& categoryMap);

    /// \brief Add a DataObject to the collection
    /// \param fieldName The unique (export) string that identifies this data
    /// \param data The DataObject to store
    /// \param categoryId The vector<string> for the subcategory
    void add(const std::string& fieldName, std::shared_ptr<DataObjectBase> data,
             const SubCategory& categoryId = {});

    /// \brief Replace a DataObject in the collection
    /// \param data The DataObject to store
    /// \param fieldName The name of the data object to replace
    /// \param categoryId The vector<string> for the subcategory
    void set(std::shared_ptr<DataObjectBase> data, const std::string& fieldName,
             const SubCategory& categoryId = {});

    /// \brief Get a DataObject from the collection
    /// \param fieldName The name of the data object to get
    /// \param categoryId The vector<string> for the subcategory
    std::shared_ptr<DataObjectBase> get(const std::string& fieldName,
                                        const SubCategory& categoryId = {}) const;

    /// \brief Get list of dimensioning paths for the field
    /// \param fieldName The name of the data object to get
    /// \param categoryId The vector<string> for the subcategory
    std::vector<std::string> getPaths(const std::string& fieldName,
                                      const SubCategory& categoryId = {}) const;

    /// \brief Get a DataObject for the group by field
    /// \param fieldName The name of the data object to get
    /// \param categoryId The vector<string> for the subcategory
    std::shared_ptr<DataObjectBase> getGroupByObject(const std::string& fieldName,
                                                     const SubCategory& categoryId = {}) const;

    /// \brief Check if DataObject with name is available
    /// \param fieldName The name of the object
    /// \param categoryId The vector<string> for the subcategory
    bool hasKey(const std::string& fieldName, const SubCategory& categoryId = {}) const;

    /// \brief Check if a category is available
    /// \param categoryId The vector<string> for the subcategory
    bool hasCategory(const SubCategory& categoryId) const;

    /// \brief Get a new data-container for the specified sub-category
    /// \param categoryId The vector<string> for the subcategory
    std::shared_ptr<DataContainer> getSubContainer(const SubCategory& categoryId) const;

    /// \brief Get the number of rows of the specified sub category
    /// \param categoryId The vector<string> for the subcategory
    size_t size(const SubCategory& categoryId = {}) const;

    /// \brief Get the number of rows of the specified sub category
    std::vector<SubCategory> allSubCategories() const;

    /// \brief Get the map of categories
    inline CategoryMap getCategoryMap() const { return categoryMap_; }

    /// \brief Convenience function used to make a string out of a subcategory listing.
    /// \param categoryId Subcategory (ie: vector<string>) listing.
    static std::string makeSubCategoryStr(const SubCategory& categoryId);

    /// \brief Get all field names
    std::vector<std::string> getFieldNames() const;

    /// \brief Append contents of other data container to this one.
    /// \param other DataContainer to append.
    void append(const DataContainer& other);

    /// \brief Gather data from all ranks into rank 0.
    /// \param comm MPI communicator to use.
    void gather(const eckit::mpi::Comm& comm);

    /// \brief Gather data to all the ranks soo they all have the same data.
    /// \param comm MPI communicator to use.
    void allGather(const eckit::mpi::Comm& comm);

  private:
    /// Category map given (see constructor).
    CategoryMap categoryMap_;

    /// Map of data for each possible subcategory
    DataSets dataSets_;

    /// \brief Uses category map to generate listings of all possible subcategories.
    void makeDataSets();

    /// \brief Remove the path info from the a field name string
    /// \param fieldName The field name to remove the path from
    /// \return The field name without the path
    static std::string dropPath(const std::string& fieldName);
  };
}  // namespace bufr

