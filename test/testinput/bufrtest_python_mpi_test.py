# (C) Copyright 2024 NOAA/NWS/NCEP/EMC
import sys
import os
import subprocess

import bufr
from bufr.encoders import netcdf
import numpy as np

def run_compare(input_path, comp_path):
    result = subprocess.Popen(f'nccmp -d -m -g -f -S {input_path} {comp_path}', shell=True).wait()
    assert result == 0, f"Comparison failed for {input_path} and {comp_path}."


def test_mpi_basic():
    DATA_PATH = 'testdata/gdas.t18z.1bmhs.tm00.bufr_d'
    YAML_PATH = 'testinput/bufrtest_mhs_basic_mapping.yaml'
    OUTPUT_PATH = 'testrun/bufrtest_mhs_basic_parallel.nc'
    COMP_PATH = 'testoutput/bufrtest_mhs_basic.nc'

    bufr.mpi.App(sys.argv) # Don't do this if passing in MPI communicator
    comm = bufr.mpi.Comm("world")

    container = bufr.Parser(DATA_PATH, YAML_PATH).parse(comm)
    container.gather(comm)

    if comm.rank() == 0:
        netcdf.Encoder(YAML_PATH).encode(container, OUTPUT_PATH)
        run_compare(OUTPUT_PATH, COMP_PATH)


def test_mpi_categories():
    DATA_PATH = 'testdata/gdas.t12z.esmhs.tm00.bufr_d'
    YAML_PATH = 'testinput/bufrtest_esmhs_mapping.yaml'
    OUTPUT_PATH = 'testrun/bufrtest_esmhs_{splits/satId}_cats.nc'
    COMP_PATH = 'testoutput/bufrtest_esmhs_noaa-19.nc'

    bufr.mpi.App(sys.argv) # Don't do this if passing in MPI communicator
    comm = bufr.mpi.Comm("world")

    container = bufr.Parser(DATA_PATH, YAML_PATH).parse(comm)
    container.gather(comm)

    if comm.rank() == 0:
        netcdf.Encoder(YAML_PATH).encode(container, OUTPUT_PATH)
        run_compare('testrun/bufrtest_esmhs_noaa-19_cats.nc', COMP_PATH)


def test_mpi_sub_container():
    DATA_PATH = 'testdata/gdas.t12z.esmhs.tm00.bufr_d'
    YAML_PATH = 'testinput/bufrtest_esmhs_mapping.yaml'
    OUTPUT_PATH = 'testrun/bufrtest_esmhs_{splits/satId}_sub_container.nc'
    COMP_PATH = 'testoutput/bufrtest_esmhs_noaa-19.nc'

    bufr.mpi.App(sys.argv) # Don't do this if passing in MPI communicator
    comm = bufr.mpi.Comm("world")

    container = bufr.Parser(DATA_PATH, YAML_PATH).parse(comm)
    container = container.get_sub_container(['noaa-19'])
    container.gather(comm)

    if comm.rank() == 0:
        netcdf.Encoder(YAML_PATH).encode(container, OUTPUT_PATH)
        run_compare('testrun/bufrtest_esmhs_noaa-19_sub_container.nc', COMP_PATH)


def test_mpi_all_gather():
    DATA_PATH = 'testdata/gdas.t06z.snocvr.tm00.bufr_d'
    YAML_PATH = 'testinput/bufrtest_long_strs_mapping.yaml'
    OUTPUT_PATH = 'testrun/bufrtest_mpi_all_gather.nc'
    COMP_PATH = 'testoutput/bufrtest_long_strs.nc'

    bufr.mpi.App(sys.argv) # Don't do this if passing in MPI communicator
    comm = bufr.mpi.Comm("world")

    container = bufr.Parser(DATA_PATH, YAML_PATH).parse(comm)
    container.all_gather(comm)

    if comm.rank() == 1:
        netcdf.Encoder(YAML_PATH).encode(container, OUTPUT_PATH)
        run_compare(OUTPUT_PATH, COMP_PATH)


if __name__ == '__main__':
    test_mpi_basic()
    test_mpi_categories()
    test_mpi_sub_container()
    test_mpi_all_gather()
