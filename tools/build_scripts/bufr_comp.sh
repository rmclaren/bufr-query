#!/bin/bash

# use nccmp or odc to compare the output of a ioda-converter
#
# argument 1: what type of file to compare; netcdf or odb
# argument 2: the command to run the ioda converter
# argument 3: the filename to test

set -eu

file_type=$1
cmd=$2  
file_name=($3)     # convert space-separated strings into an array
tol=${4:-"0.0"}
verbose=${5:-${VERBOSE:-"N"}}

[[ $verbose == [YyTt] || \
   $verbose == [Yy][Ee][Ss] || \
   $verbose == [Tt][Rr][Uu][Ee] ]] && set -x

rc="-1"
echo "emily checking: U are using new bufr_comp.sh ....."

case $file_type in
  netcdf)
    $cmd && \
    for i in "${!file_name[@]}"; do
        file_name=${file_name[$i]}
        nccmp testrun/$file_name testoutput/$file_name -d -m -g -f -s -S -B -T ${tol}
        rc=${?}
        if [[ $rc -ne 0 ]]; then
           echo "Files testrun/$file_name and testoutput/$file_name are not identical."
        fi
    done
    ;;
   odb)
    $cmd && \
    odc compare testrun/$file_name testoutput/$file_name
    rc=${?}
    ;;
  ascii)
    $cmd && \
    diff testrun/$file_name testoutput/$file_name
    rc=${?}
    ;;
   *)
    echo "ERROR: iodaconv_comp.sh: Unrecognized file type: ${file_type}"
    rc="-2"
    ;;
esac
# If any comparison fails, exit with an error
if [[ $rc -ne 0 ]]; then
    exit $rc
fi

exit $rc
