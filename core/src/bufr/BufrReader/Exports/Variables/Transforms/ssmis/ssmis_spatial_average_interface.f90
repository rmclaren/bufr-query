module ssmis_spatial_average_c_interface_mod

  use iso_c_binding

  implicit none

  private
  public:: SSMIS_Spatial_Average_c

contains

  subroutine SSMIS_Spatial_Average_c(bufrsat, method, num_obs, nchanl, missingval, &
                                     fov, rainflag, time, lat, lon, bt_inout, error_status) &
                                     bind(C, name='SSMIS_Spatial_Average_f')

    use ssmis_spatial_average_mod, only: SSMIS_Spatial_Average

    integer(c_int),   value, intent(in)    :: bufrsat
    integer(c_int),   value, intent(in)    :: method
    integer(c_int),   value, intent(in)    :: num_obs
    integer(c_int),   value, intent(in)    :: nchanl
    real(c_float),    value, intent(in)    :: missingval 
    type(c_ptr),             intent(in)    :: fov
    type(c_ptr),             intent(in)    :: rainflag 
    type(c_ptr),             intent(in)    :: time 
    type(c_ptr),             intent(in)    :: lat
    type(c_ptr),             intent(in)    :: lon
    type(c_ptr),             intent(inout) :: bt_inout
    integer(c_int),          intent(inout) :: error_status

    integer(c_int),     pointer :: fov_f(:)
    integer(c_int),     pointer :: rainflag_f(:)
    integer(c_int64_t), pointer :: time_f(:)
    real(c_float),      pointer :: lat_f(:)
    real(c_float),      pointer :: lon_f(:)
    real(c_float),      pointer :: bt_inout_f(:,:)

    call c_f_pointer(fov, fov_f, [num_obs])
    call c_f_pointer(rainflag, rainflag_f, [num_obs])
    call c_f_pointer(time, time_f, [num_obs])
    call c_f_pointer(lat, lat_f, [num_obs])
    call c_f_pointer(lon, lon_f, [num_obs])
    call c_f_pointer(bt_inout, bt_inout_f, [nchanl, num_obs])

    call SSMIS_Spatial_Average(bufrsat, method, num_obs, nchanl, missingval,  &
                             fov_f, rainflag_f, time_f, lat_f, lon_f, bt_inout_f, error_status)

  end subroutine SSMIS_Spatial_Average_c

end module ssmis_spatial_average_c_interface_mod
