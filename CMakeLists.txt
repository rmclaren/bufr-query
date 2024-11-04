#
# (C) Copyright 2024 NOAA/NCEP/EMC
#
# This software is licensed under the terms of the Apache Licence Version 2.0
# which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.

################################################################################
# BUFR Query
################################################################################

cmake_minimum_required( VERSION 3.14 )
project( bufr_query VERSION 0.0.4 LANGUAGES CXX Fortran)

## Configuration
find_package( ecbuild QUIET )
include( ecbuild_system NO_POLICY_SCOPE )
ecbuild_declare_project()
list( APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake )
set( CMAKE_DIRECTORY_LABELS ${PROJECT_NAME} )
include( ${PROJECT_NAME}_compiler_flags )

## Dependencies
find_package( OpenMP REQUIRED)
find_package( MPI REQUIRED)
find_package( eckit 1.24.4 REQUIRED COMPONENTS MPI )
find_package( Eigen3 REQUIRED NO_MODULE HINTS
              $ENV{Eigen3_ROOT} $ENV{EIGEN3_ROOT} $ENV{Eigen_ROOT} $ENV{EIGEN_ROOT}
              $ENV{Eigen3_PATH} $ENV{EIGEN3_PATH} $ENV{Eigen_PATH} $ENV{EIGEN_PATH} )
find_package( gsl-lite REQUIRED HINTS $ENV{gsl_lite_DIR} )
find_package( NetCDF REQUIRED COMPONENTS CXX)
find_package( bufr 12.0.1 REQUIRED)


# Python-finding settings
set(Python3_FIND_REGISTRY "LAST")
set(Python3_FIND_FRAMEWORK "LAST")
find_package( Python3 COMPONENTS Interpreter Development)
find_package( pybind11 QUIET)

if (Python3_FOUND AND pybind11_FOUND)
  option(BUILD_PYTHON_BINDINGS "Build Python bindings using pybind11?" ON)
else()
  option(BUILD_PYTHON_BINDINGS "Build Python bindings using pybind11?" OFF)
endif()
add_feature_info(Python BUILD_PYTHON_BINDINGS "Build Python bindings using pybind11")


## Sources
add_subdirectory( core )

if (BUILD_PYTHON_BINDINGS)
  add_subdirectory( python )
endif()

add_subdirectory( tools )
add_subdirectory( test )

## Tests
ecbuild_add_test( TARGET ${PROJECT_NAME}_coding_norms
                  TYPE SCRIPT
                  COMMAND ${PROJECT_NAME}_cpp_lint.py
                  ARGS --quiet --recursive ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/test
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin )
set_tests_properties( ${PROJECT_NAME}_coding_norms PROPERTIES TIMEOUT 90 )

## Package Config
ecbuild_install_project( NAME ${PROJECT_NAME} )

ecbuild_print_summary()

