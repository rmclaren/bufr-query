// (C) Copyright 2022 NOAA/NWS/NCEP/EMC

/** @file
    @brief Define signature to enable the SSMIS spatial average module
    written in Fortran 90 to be called via wrapper functions from C and C++
    application programs.

 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

  void SSMIS_Spatial_Average_f(int BufrSat, int Method, int Num_Obs, int NChanl, void* Fov, 
                              void* Node_InOut,void* Time, void* Lat, void* Lon, void* BT_InOut,
                              int* Error_Status);

#ifdef __cplusplus
}
#endif


