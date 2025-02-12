module ssmis_spatial_average_c_interface_mod

  use iso_c_binding

  implicit none

  private
  public:: SSMIS_Spatial_Average_c

contains

  subroutine SSMIS_Spatial_Average_c(BufrSat, Method, Num_Obs, NChanl,  &
                                   FOV, Node_InOut, Time, Lat, Lon, BT_InOut, Error_status) &
                                    bind(C, name='SSMIS_Spatial_Average_f')

    use ssmis_spatial_average_mod, only: SSMIS_Spatial_Average

    integer(c_int), value, intent(in)    :: BufrSat
    integer(c_int), value, intent(in)    :: Method
    integer(c_int), value, intent(in)    :: Num_Obs
    integer(c_int), value, intent(in)    :: Nchanl
    type(c_ptr),           intent(in)    :: Fov
    type(c_ptr),           intent(in)    :: Time 
    type(c_ptr),           intent(in)    :: Lat
    type(c_ptr),           intent(in)    :: Lon
    type(c_ptr),           intent(inout) :: Node_InOut
    type(c_ptr),           intent(inout) :: BT_InOut
    integer(c_int),        intent(inout) :: Error_status

    integer(c_int),     pointer :: Fov_f(:)
    integer(c_int),     pointer :: Node_InOut_f(:)
    real(c_float),      pointer :: Time_f(:)
    real(c_float),      pointer :: Lat_f(:)
    real(c_float),      pointer :: Lon_f(:)
    real(c_float),      pointer :: BT_Inout_f(:,:)

    call c_f_pointer(Fov, Fov_f, [Num_Obs])
    call c_f_pointer(Node_InOut, Node_InOut_f, [Num_Obs])
    call c_f_pointer(Time, Time_f, [Num_Obs])
    call c_f_pointer(Lat, Lat_f, [Num_Obs])
    call c_f_pointer(Lon, Lon_f, [Num_Obs])
    call c_f_pointer(BT_InOut, BT_InOut_f, [Nchanl, Num_Obs])

    call SSMIS_Spatial_Average(BufrSat, Method, Num_Obs, NChanl,  &
                             FOV_f, Node_InOut_f, Time_f, Lat_f, Lon_f, BT_InOut_f, Error_status)

  end subroutine SSMIS_Spatial_Average_c

end module ssmis_spatial_average_c_interface_mod
