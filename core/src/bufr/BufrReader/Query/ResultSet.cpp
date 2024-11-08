// (C) Copyright 2022 NOAA/NWS/NCEP/EMC

#include "bufr/ResultSet.h"

#include "ResultSetImpl.h"

namespace bufr {

  ResultSet::ResultSet() : impl_(std::make_unique<ResultSetImpl>()) {}

  ResultSet::ResultSet(const ResultSet& other) :
    impl_(std::make_unique<ResultSetImpl>(*other.impl_)) {}

  ResultSet::~ResultSet() = default;

  std::shared_ptr<DataObjectBase> ResultSet::get(const std::string& fieldName,
                                                 const std::string& groupByFieldName,
                                                 const std::string& overrideType) const
  {
    return impl_->get(fieldName, groupByFieldName, overrideType);
  }

  std::string ResultSet::resolveType(const eckit::mpi::Comm& comm,
                                  const std::string& fieldName) const
  {
    return impl_->resolveType(comm, fieldName);
  }

}  // namespace bufr
