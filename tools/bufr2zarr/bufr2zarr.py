#!/usr/bin/env python3

# (C) Copyright 2024 NOAA/NWS/NCEP/EMC

import sys
import argparse
import time

import bufr
from bufr.encoders import zarr

def run(comm, data_path, mapping_path, output_path):
    start = time.time()

    container = bufr.Parser(data_path, mapping_path).parse(comm)
    container.gather(comm)

    if comm.rank() == 0:
        zarr.Encoder(mapping_path).encode(container, output_path)
        print(f"Total Time [{time.time()-start:.1f}s]")


if __name__ == '__main__':
    bufr.mpi.App(sys.argv) # Don't do this if passing in MPI communicator
    comm = bufr.mpi.Comm("world")

    # parse the command line arguments (bufr_data, mapping, output)
    parser = argparse.ArgumentParser(description='Convert BUFR data to Zarr')
    parser.add_argument('data_path', type=str, help='Path to the BUFR data')
    parser.add_argument('mapping_path', type=str, help='Path to the mapping file')
    parser.add_argument('output_path', type=str, help='Path to the output file')

    args = parser.parse_args()
    run(comm, args.data_path, args.mapping_path, args.output_path)
