// (C) Copyright 2024 NOAA/NWS/NCEP/EMC

#pragma once

#include "eckit/mpi/Comm.h"

namespace bufr {
namespace mpi {
  class Comm
  {
  public:
    Comm() = delete;
    Comm(const std::string& name) : comm_(eckit::mpi::comm(name.c_str())) {}

    std::string name() { return comm_.name(); }
    eckit::mpi::Comm& getComm() { return comm_; }
    int rank() { return comm_.rank(); }
    int size() { return comm_.size(); }

  private:
    eckit::mpi::Comm& comm_;
  };
}  // namespace mpi
}  // namespace bufr
