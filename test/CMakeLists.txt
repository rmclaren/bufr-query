# (C) Copyright 2024 NOAA/NCEP/EMC.

# Download the test data if we don't have it.
# Code borrowed from NCEPLIB-bufr project...

set(DOWNLOAD_URL "https://ftp.emc.ncep.noaa.gov/static_files/public/bufr-query")
set(FILE_COLLECTION "bufr_query-0.0.1.tgz")

# Get the test data if we don't have it.
if(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/${FILE_COLLECTION}")
  message(STATUS "Downloading: " ${CMAKE_CURRENT_BINARY_DIR}/${FILE_COLLECTION})
  file(DOWNLOAD
    ${DOWNLOAD_URL}/${FILE_COLLECTION}
    ${CMAKE_CURRENT_BINARY_DIR}/${FILE_COLLECTION}
    SHOW_PROGRESS
    STATUS download_status
    INACTIVITY_TIMEOUT 30
  )

  list(GET download_status 0 download_status_num)

  if(NOT download_status_num EQUAL 0 OR NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/${FILE_COLLECTION})
    # Remove empty file if download doesn't complete
    file(REMOVE ${CMAKE_CURRENT_BINARY_DIR}/${BUFR_TAR})
    message(STATUS "Could not download test files, not building tests")
    return()
  endif()

  add_custom_target(get_bufr_test_data ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${FILE_COLLECTION})
  add_custom_command(
    TARGET get_bufr_test_data
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR} tar xzf ${FILE_COLLECTION}
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR} rm -rf testdata
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR} rm -rf testoutput
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR} mv remote_data/testdata testdata
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR} mv remote_data/testoutput testoutput
    COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_CURRENT_BINARY_DIR} rm -rf remote_data)
endif()


# Function definitions

# The list of all test input config files, which need to be symlink from the build folder.
# (These symlink are created at the end of this file, to give CMake functions setting up certain
# tests a chance to append items to this list).

list( APPEND test_input
  testinput/bufrtest_filtering_mapping.yaml
  testinput/bufrtest_split_mapping.yaml
  testinput/bufrtest_filter_split_mapping.yaml
  testinput/bufrtest_empty_fields_mapping.yaml
  testinput/bufrtest_simple_groupby_mapping.yaml
  testinput/bufrtest_read_2_dim_blocks_mapping.yaml
  testinput/bufrtest_mhs_basic_mapping.yaml
  testinput/bufrtest_hrs_basic_mapping.yaml
  testinput/bufrtest_adpupa_mapping.yaml
  testinput/bufrtest_amua_ta_mapping.yaml
  testinput/bufrtest_amua_n15_mapping.yaml
  testinput/bufrtest_esamua_mapping.yaml
  testinput/bufrtest_mhs_mapping.yaml
  testinput/bufrtest_esmhs_mapping.yaml
  testinput/bufrtest_adpsfc_mapping.yaml
  testinput/bufrtest_adpsfc_prepbufr_mapping.yaml
  testinput/bufrtest_adpsfc_snow_mapping.yaml
  testinput/bufrtest_aircar_mapping.yaml
  testinput/bufrtest_aircft_noamdar103_mapping.yaml
  testinput/bufrtest_aircft_amdar103_mapping.yaml
  testinput/bufrtest_aircft_prepbufr_mapping.yaml
  testinput/bufrtest_wmo_radiosonde_mapping.yaml
  testinput/bufrtest_satwnd_new_format_mapping.yaml
  testinput/bufrtest_satwnd_old_format_mapping.yaml
  testinput/bufrtest_satwnd_avhrr_mapping.yaml
  testinput/bufrtest_mtiasi_mapping.yaml
  testinput/bufrtest_atms_mapping.yaml
  testinput/bufrtest_sfcshp_mapping.yaml
  testinput/bufrtest_adpupa_prepbufr_mapping.yaml
  testinput/bufrtest_rtma_mesonet_mapping.yaml
  testinput/bufrtest_rtma_adpsfc_mapping.yaml
  testinput/bufrtest_sevcsr_mapping.yaml
  testinput/bufrtest_specific_subsets_by_query_mapping.yaml
  testinput/bufrtest_specifying_subsets_mapping.yaml
  testinput/bufrtest_lgycld_rrfs_mapping.yaml
  testinput/bufrtest_adpsfc_cloud_rrfs_prepbufr_mapping.yaml
  testinput/bufrtest_long_strs_mapping.yaml
  testinput/bufrtest_rtma_aircft_mapping.yaml
  testinput/bufrtest_rtma_aircft_NC004103_mapping.yaml
  testinput/bufrtest_wmo_amdar_multi_mapping.yaml
  testinput/bufrtest_dim_labels_mapping.yaml
  testinput/bufrtest_fieldname_validation.py
  testinput/bufrtest_python_test.py
  testinput/bufrtest_python_mpi_test.py
)

# create test directories and make links to the input files
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/testinput)
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/testrun)
foreach(FILENAME ${test_input})
  execute_process( COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_CURRENT_SOURCE_DIR}/${FILENAME}
    ${CMAKE_CURRENT_BINARY_DIR}/${FILENAME} )
endforeach(FILENAME)


ecbuild_add_test( TARGET  test_bufr_filtering
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t18z.1bmhs.tm00.bufr_d
                                                                 testinput/bufrtest_filtering_mapping.yaml
                                                                 testrun/bufrtest_filtering.nc"
                          bufrtest_filtering.nc)

ecbuild_add_test( TARGET  test_bufr_split
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t18z.1bmhs.tm00.bufr_d
                                                                 testinput/bufrtest_split_mapping.yaml
                                                                 testrun/bufrtest_split_{splits/hour}_{splits/minute}.nc"
                          bufrtest_split_15_seven.nc)

ecbuild_add_test( TARGET  test_bufr_filter_split
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t18z.1bmhs.tm00.bufr_d
                                                                 testinput/bufrtest_filter_split_mapping.yaml
                                                                 testrun/bufrtest_filter_split_{splits/hour}_{splits/minute}.nc"
                          bufrtest_filter_split_15_7.nc)

ecbuild_add_test( TARGET  test_bufr_empty_fields
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/bufr_empty_fields.bufr_d
                                                                 testinput/bufrtest_empty_fields_mapping.yaml
                                                                 testrun/bufrtest_empty_fields.nc"
                          bufrtest_empty_fields.nc)


ecbuild_add_test( TARGET  test_bufr_simple_groupby
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/bufr_simple_groupby.bufr
                                                                 testinput/bufrtest_simple_groupby_mapping.yaml
                                                                 testrun/bufrtest_simple_groupby.nc"
                          bufrtest_simple_groupby.nc)

ecbuild_add_test( TARGET  test_bufr_read_2_dim_blocks
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/bufr_read_2_dim_blocks.bufr
                                                                 testinput/bufrtest_read_2_dim_blocks_mapping.yaml
                                                                 testrun/bufrtest_read_2_dim_blocks.nc"
                          bufrtest_read_2_dim_blocks.nc)

ecbuild_add_test( TARGET  test_bufr_mhs_basic
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t18z.1bmhs.tm00.bufr_d
                                                                 testinput/bufrtest_mhs_basic_mapping.yaml
                                                                 testrun/bufrtest_mhs_basic.nc"
                          bufrtest_mhs_basic.nc)

ecbuild_add_test( TARGET  test_bufr_hrs_basic
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t00z.1bhrs4.tm00.bufr_d
                                                                 testinput/bufrtest_hrs_basic_mapping.yaml
                                                                 testrun/bufrtest_hrs_basic.nc"
                          bufrtest_hrs_basic.nc)

ecbuild_add_test( TARGET  test_bufr_adpupa
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.adpupa.tm00.bufr_d
                                                                 testinput/bufrtest_adpupa_mapping.yaml
                                                                 testrun/bufrtest_adpupa.nc"
                          bufrtest_adpupa.nc)

ecbuild_add_test( TARGET  test_bufr_amua_ta
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.1bamua.tm00.bufr_d
                                                                 testinput/bufrtest_amua_ta_mapping.yaml
                                                                 testrun/bufrtest_amua_{splits/satId}_ta.nc"
                          bufrtest_amua_metop-a_ta.nc)

ecbuild_add_test( TARGET  test_bufr_amua_n15
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.1bamua.tm00.bufr_d
                                                                 testinput/bufrtest_amua_n15_mapping.yaml
                                                                 testrun/bufrtest_amua_{splits/satId}.nc"
                          bufrtest_amua_n15.nc)

ecbuild_add_test( TARGET  test_bufr_esamua
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.esamua.tm00.bufr_d
                                                                 testinput/bufrtest_esamua_mapping.yaml
                                                                 testrun/bufrtest_esamua_{splits/satId}.nc"
                          bufrtest_esamua_n18.nc)

ecbuild_add_test( TARGET  test_bufr_mhs
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t18z.1bmhs.tm00.bufr_d
                                                                 testinput/bufrtest_mhs_mapping.yaml
                                                                 testrun/bufrtest_mhs_{splits/satId}.nc"
                          bufrtest_mhs_metop-b.nc)

ecbuild_add_test( TARGET  test_bufr_esmhs
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.esmhs.tm00.bufr_d
                                                                 testinput/bufrtest_esmhs_mapping.yaml
                                                                 testrun/bufrtest_esmhs_{splits/satId}.nc"
                          bufrtest_esmhs_noaa-19.nc)

ecbuild_add_test( TARGET  test_bufr_adpsfc
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.adpsfc.tm00.bufr_d
                                                                 testinput/bufrtest_adpsfc_mapping.yaml
                                                                 testrun/bufrtest_adpsfc.nc"
                          bufrtest_adpsfc.nc)

ecbuild_add_test( TARGET  test_bufr_adpsfc_prepbufr
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.adpsfc.prepbufr
                                                                 testinput/bufrtest_adpsfc_prepbufr_mapping.yaml
                                                                 testrun/bufrtest_adpsfc_prepbufr.nc"
                          bufrtest_adpsfc_prepbufr.nc)

ecbuild_add_test( TARGET  test_bufr_adpsfc_snow
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t06z.adpsfc.tm00.bufr_d
                                                                 testinput/bufrtest_adpsfc_snow_mapping.yaml
                                                                 testrun/bufrtest_adpsfc_snow.nc"
                          bufrtest_adpsfc_snow.nc)

ecbuild_add_test( TARGET  test_bufr_aircar
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.aircar.tm00.bufr_d
                                                                 testinput/bufrtest_aircar_mapping.yaml
                                                                 testrun/bufrtest_aircar.nc"
                          bufrtest_aircar.nc)

ecbuild_add_test( TARGET  test_bufr_aircft_noamdar103
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.aircft.tm00.bufr_d
                                                                 testinput/bufrtest_aircft_noamdar103_mapping.yaml
                                                                 testrun/bufrtest_aircft_noamdar103.nc"
                          bufrtest_aircft_noamdar103.nc)

ecbuild_add_test( TARGET  test_bufr_aircft_amdar103
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.aircft.tm00.bufr_d
                                                                 testinput/bufrtest_aircft_amdar103_mapping.yaml
                                                                 testrun/bufrtest_aircft_amdar103.nc"
                          bufrtest_aircft_amdar103.nc)

ecbuild_add_test( TARGET  test_bufr_aircft_prepbufr
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.acft_profiles.prepbufr
                                                                 testinput/bufrtest_aircft_prepbufr_mapping.yaml
                                                                 testrun/bufrtest_aircft_prepbufr.nc"
                          bufrtest_aircft_prepbufr.nc)

ecbuild_add_test( TARGET  test_bufr_wmo_radiosonde
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/bufr_read_wmo_radiosonde.bufr
                                                                 testinput/bufrtest_wmo_radiosonde_mapping.yaml
                                                                 testrun/bufrtest_wmo_radiosonde.nc
                                                                 -t testdata/bufr_tables"
                          bufrtest_wmo_radiosonde.nc)

ecbuild_add_test( TARGET  test_bufr_satwnd_old_format
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/bufr_satwnd_old_format.bufr
                                                                 testinput/bufrtest_satwnd_old_format_mapping.yaml
                                                                 testrun/bufrtest_satwnd_old_format.nc"
                          bufrtest_satwnd_old_format.nc)

ecbuild_add_test( TARGET  test_bufr_satwnd_new_format
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/bufr_satwnd_new_format.bufr
                                                                 testinput/bufrtest_satwnd_new_format_mapping.yaml
                                                                 testrun/bufrtest_satwnd_new_format.nc"
                          bufrtest_satwnd_new_format.nc)

ecbuild_add_test( TARGET  test_bufr_satwnd_avhrr
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t18z.satwnd_avhrr.tm00.bufr_d
                                                                 testinput/bufrtest_satwnd_avhrr_mapping.yaml
                                                                 testrun/bufrtest_satwnd_avhrr.nc"
                          bufrtest_satwnd_avhrr.nc)

ecbuild_add_test( TARGET  test_bufr_mtiasi
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.mtiasi.tm00.bufr_d
                                                                 testinput/bufrtest_mtiasi_mapping.yaml
                                                                 testrun/bufrtest_mtiasi_{splits/satId}.nc"
                          bufrtest_mtiasi_metop-c.nc)

#ecbuild_add_test( TARGET  test_bufr_mtiasi_cached
#                  TYPE    SCRIPT
#                  COMMAND bash
#                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
#                          netcdf
#                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.mtiasi.tm00.bufr_d
#                                                                 testinput/bufrtest_mtiasi_cached_mapping.yaml
#                                                                 testrun/bufrtest_mtiasi_cached_{splits/satId}.nc"
#                          bufrtest_mtiasi_metop-c.nc)

ecbuild_add_test( TARGET  test_bufr_atms
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t00z.atms.tm00.bufr_d
                                                                 testinput/bufrtest_atms_mapping.yaml
                                                                 testrun/bufrtest_atms_{splits/satId}.nc"
                          bufrtest_atms_n20.nc)

ecbuild_add_test( TARGET  test_bufr_sfcshp
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/bufr_sfcshp.bufr
                                                                 testinput/bufrtest_sfcshp_mapping.yaml
                                                                 testrun/bufrtest_sfcshp.nc"
                          bufrtest_sfcshp.nc)

ecbuild_add_test( TARGET  test_bufr_adpupa_prepbufr
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/bufr_adpupa_prepbufr.bufr
                                                                 testinput/bufrtest_adpupa_prepbufr_mapping.yaml
                                                                 testrun/bufrtest_adpupa_prepbufr.nc"
                          bufrtest_adpupa_prepbufr.nc)

ecbuild_add_test( TARGET  test_bufr_rtma_mesonet
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/rtma_ru.t00z.msonet.tm00.bufr_d
                                                                 testinput/bufrtest_rtma_mesonet_mapping.yaml
                                                                 testrun/bufrtest_rtma_mesonet.nc"
                          bufrtest_rtma_mesonet.nc)

ecbuild_add_test( TARGET  test_bufr_rtma_adpsfc
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/rtma_ru.t0000z.adpsfc_nc000101.tm00.bufr_d
                                                                 testinput/bufrtest_rtma_adpsfc_mapping.yaml
                                                                 testrun/bufrtest_rtma_adpsfc.nc"
                          bufrtest_rtma_adpsfc.nc)

ecbuild_add_test( TARGET  test_bufr_sevcsr
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t00z.sevcsr.tm00.bufr_d
                                                                 testinput/bufrtest_sevcsr_mapping.yaml
                                                                 testrun/bufrtest_sevcsr.nc"
                          bufrtest_sevcsr.nc)

ecbuild_add_test( TARGET  test_bufr_specific_subsets_by_query
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.aircft.tm00.bufr_d
                                                                 testinput/bufrtest_specific_subsets_by_query_mapping.yaml
                                                                 testrun/bufrtest_specific_subsets_by_query.nc"
                          bufrtest_specific_subsets_by_query.nc)

ecbuild_add_test( TARGET  test_bufr_specifying_subsets
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t12z.aircft.tm00.bufr_d
                                                                 testinput/bufrtest_specifying_subsets_mapping.yaml
                                                                 testrun/bufrtest_specifying_subsets.nc"
                          bufrtest_specifying_subsets.nc)

ecbuild_add_test( TARGET  test_bufr_lgycld_rrfs
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/rap.t06z.lgycld.tm00.bufr_d
                                                                 testinput/bufrtest_lgycld_rrfs_mapping.yaml
                                                                 testrun/bufrtest_lgycld_rrfs.nc"
                          bufrtest_lgycld_rrfs.nc)

ecbuild_add_test( TARGET  test_bufr_adpsfc_cloud_rrfs_prepbufr
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/rap.t06z.adpsfc.prepbufr.tm00.bufr
                                                                 testinput/bufrtest_adpsfc_cloud_rrfs_prepbufr_mapping.yaml
                                                                 testrun/bufrtest_adpsfc_cloud_rrfs_prepbufr.nc"
                          bufrtest_adpsfc_cloud_rrfs_prepbufr.nc)

ecbuild_add_test( TARGET  test_bufr_long_strs
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t06z.snocvr.tm00.bufr_d
                                                                 testinput/bufrtest_long_strs_mapping.yaml
                                                                 testrun/bufrtest_long_strs.nc"
                          bufrtest_long_strs.nc)

ecbuild_add_test( TARGET  test_bufr_rtma_aircft
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/rtma_ru.t0000z.aircft.tm00_nc004006.bufr_d
                                                                 testinput/bufrtest_rtma_aircft_mapping.yaml
                                                                 testrun/bufrtest_rtma_aircft.nc"
                          bufrtest_rtma_aircft.nc)

ecbuild_add_test( TARGET  test_bufr_rtma_aircft_NC004103
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/rtma_ru.t0000z.aircft.tm00_nc004103.bufr_d
                                                                 testinput/bufrtest_rtma_aircft_NC004103_mapping.yaml
                                                                 testrun/bufrtest_rtma_aircft_NC004103.nc"
                          bufrtest_rtma_aircft_NC004103.nc)

ecbuild_add_test( TARGET  test_bufr_wmo_amdar_multi
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/amdar_wmo_multi.bufr
                                                                 testinput/bufrtest_wmo_amdar_multi_mapping.yaml
                                                                 testrun/bufrtest_wmo_amdar_multi.nc
                                                                 -t testdata/bufr_tables"
                          bufrtest_wmo_amdar_multi.nc)

ecbuild_add_test( TARGET  test_bufr_dim_labels
                  TYPE    SCRIPT
                  COMMAND bash
                  ARGS    ${CMAKE_BINARY_DIR}/bin/bufr_comp.sh
                          netcdf
                          "${CMAKE_BINARY_DIR}/bin/bufr2netcdf.x testdata/gdas.t00z.1bhrs4.tm00.bufr_d
                                                                 testinput/bufrtest_dim_labels_mapping.yaml
                                                                 testrun/bufrtest_hrs_basic.nc"
  bufrtest_hrs_basic.nc)

ecbuild_add_test( TARGET  test_bufr_show_queries_ncep
                  TYPE    SCRIPT
                  COMMAND ${CMAKE_BINARY_DIR}/bin/show_queries.x
                  ARGS "testdata/gdas.t18z.1bmhs.tm00.bufr_d")

ecbuild_add_test( TARGET  test_bufr_show_queries_wmo
                  TYPE    SCRIPT
                  COMMAND ${CMAKE_BINARY_DIR}/bin/show_queries.x
                  ARGS -t  "testdata/bufr_tables"
                           "testdata/amdar_wmo_multi.bufr")

ecbuild_add_test( TARGET  test_bufr_show_queries_help
                  TYPE    SCRIPT
                  COMMAND ${CMAKE_BINARY_DIR}/bin/show_queries.x
                  ARGS    -h)


if (${BUILD_PYTHON_BINDINGS})

  ecbuild_add_test( TARGET  test_bufr_fieldname_validation
                    TYPE    SCRIPT
                    COMMAND python3
                    ARGS    testinput/bufrtest_fieldname_validation.py
                    ENVIRONMENT PYTHONPATH=${CMAKE_BINARY_DIR}/lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages:$ENV{PYTHONPATH})

  ecbuild_add_test( TARGET  test_bufr_python_test
                    TYPE    SCRIPT
                    COMMAND python3
                    ARGS    testinput/bufrtest_python_test.py
                    ENVIRONMENT PYTHONPATH=${CMAKE_BINARY_DIR}/lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages:$ENV{PYTHONPATH})

  ecbuild_add_test( TARGET  test_bufr_python_mpi_test
                    MPI 4
                    TYPE    SCRIPT
                    COMMAND python3
                    ARGS    testinput/bufrtest_python_mpi_test.py
                    ENVIRONMENT PYTHONPATH=${CMAKE_BINARY_DIR}/lib/python${Python3_VERSION_MAJOR}.${Python3_VERSION_MINOR}/site-packages:$ENV{PYTHONPATH})

endif()
