/*
* (C) Copyright 2022 NOAA/NWS/NCEP/EMC
*
* This software is licensed under the terms of the Apache Licence Version 2.0
* which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
*/

#pragma once

#include <iostream>
#include <unordered_map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "eckit/mpi/Comm.h"

#include "DataObject.h"
#include "DataProvider.h"


namespace bufr {
  class ResultSetImpl;

  /// \brief This class acts as the container for all the data that is collected during the
  /// the BUFR querying process in the form of SubsetLookupTable instances.
  ///
  /// \par The getter functions for the data construct the final output based on the data and
  /// metadata in these lookup tables. There are many complications. For one the data may be
  /// jagged (lookup table instances do not necessarily all have the same number of elements
  /// [repeated data could have a different number of repeats per instance]). Another is the
  /// application group_by fields which affect the dimensionality of the data. In order to make
  /// the data into rectangular arrays it may be necessary to strategically fill in missing values
  /// so that the data is organized correctly in each dimension.
  ///
  class ResultSet
  {
   public:
    ResultSet();
    ResultSet(const ResultSet& other);
    ~ResultSet();


    /// \brief Gets the resulting data for a specific field with a given name grouped by the
    /// optional groupByFieldName.
    /// \param fieldName The name of the field to get the data for.
    /// \param groupByFieldName The name of the field to group the data by.
    /// \param overrideType The name of the override type to convert the data to. Possible
    /// values are int, uint, int32, uint32, int64, uint64, float, double
    /// \return A Result object containing the data.
    std::shared_ptr<DataObjectBase> get(const std::string& fieldName,
                                        const std::string& groupByFieldName = "",
                                        const std::string& overrideType     = "") const;

    std::string  resolveType(const eckit::mpi::Comm& comm,
                             const std::string& fieldName) const;

    friend class QueryRunner;

   private:
     std::unique_ptr<ResultSetImpl> impl_;
  };
}  // namespace bufr
