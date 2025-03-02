Module SSMIS_Spatial_Average_Mod
!
!
! abstract:  This routine reads BUFR format SSMIS radiance 
!            (brightness temperature) files, spatially 
!            averages the data using the AAPP averaging routines
!            and writes the data back into BUFR format
!
!
! Program history log:
!    2011-12-20   eliu      - SSMIS (originally from Banghua Yan) with modifications 
!    2016-03-03   ejones    - Add option for spatial averaging of GMI
!    2016-03-24   ejones    - Add option for spatial averaging of AMSR2
!

  use ssmis_kinds, only: r_kind,r_double,i_kind,i_llong
  use m_distance

  implicit none     

! Declare module level parameters
  real(r_double), parameter    :: Missing_Value=1.e11_r_double

  private
  public :: SSMIS_Spatial_Average

CONTAINS 

  SUBROUTINE SSMIS_Spatial_Average(bufrsat, method, num_obs, nchanl,  &
                                   fov, rflg, node_inout, time, lat, lon, bt_inout, error_status)

    IMPLICIT NONE
    
    ! Declare passed variables
    integer(i_kind) ,intent(in   ) :: bufrsat  
    integer(i_kind) ,intent(in   ) :: method ! 1=SSMIS, 2=GMI, 3=AMSR2  
    integer(i_kind) ,intent(in   ) :: num_obs, nchanl
    integer(i_kind) ,intent(in   ) :: fov(num_obs)
    integer(i_kind) ,intent(in   ) :: rflg(num_obs)
    integer(i_llong),intent(in   ) :: time(num_obs)
    real(r_kind)    ,intent(in   ) :: lat(num_obs)
    real(r_kind)    ,intent(in   ) :: lon(num_obs)
    real(r_kind)    ,intent(inout) :: bt_inout(nchanl*num_obs)
    integer(i_kind) ,intent(inout) :: node_inout(num_obs)
    integer(i_kind) ,intent(inout) :: error_status

    ! Declare local parameters
    integer(i_kind), parameter :: lninfile=15
    integer(i_kind), parameter :: max_fov=60  ! SSMIS
    integer(i_kind), parameter :: max_fov_gmi=221
    integer(i_kind), parameter :: max_fov_amsr2=243
    integer(i_kind), parameter :: as_node= 1_i_kind
    integer(i_kind), parameter :: ds_node=-1_i_kind
    real(r_kind),    parameter :: btmin=70.0_r_kind
    real(r_kind),    parameter :: btmax=320.0_r_kind
    real(r_kind),    parameter :: sigma = 1.0_r_kind/25.0_r_kind    
    real(r_kind),    parameter :: scan_interval = 60.0_r_kind/31.6_r_kind ! SSMIS: 31.6 RPM ~ 1.9 s

    ! Declare local variables
    character(100) :: infile 

    integer(i_kind) :: i,iscan,ifov,ichan,nchannels,wmosatid,version
    integer(i_kind) :: is,ip,ic 
    integer(i_kind) :: iobs   
    integer(i_kind) :: ios,max_scan,min_scan
    integer(i_kind) :: ns1,ns2,np1,np2 
    integer(i_kind) :: nscan       
    integer(i_llong):: mintime 
    integer(i_llong) :: t1,t2  
    integer(i_kind), allocatable ::  nodeinfo(:,:)
    integer(i_kind), allocatable ::  rainflag(:,:)
    integer(i_kind), allocatable ::  scanline_back(:,:)
    integer(i_kind), allocatable ::  scanline(:)
    real(r_kind), allocatable ::  scanlinex(:)
    real(r_kind), allocatable ::  latitude(:,:), longitude(:,:)
    real(r_kind), allocatable :: bt_image(:,:,:)
    real(r_kind), dimension(nchanl, num_obs) :: bt_obs
    real(r_kind) :: tdiff 
    real(r_kind) :: lat1,lat2,lon1,lon2,dist,wgt 
    real(r_kind) :: xnum,mta   
    real(r_kind) :: dlat
    logical      :: gaussian_wgt
    logical      :: below_50, above_500 

    error_status=0
    bt_obs = reshape(bt_inout, (/nchanl, num_obs/))

    if (Method == 1) then  ! SSMIS

       gaussian_wgt = .false.
       below_50  = any(bt_obs < 50)
       above_500 = any(bt_obs > 500)
       write(*,*) 'SSMIS_Spatial_Average: input infomration ...'
       write(*,*) 'SSMIS_Spatial_Average: bufrsat = ', bufrsat
       write(*,*) 'SSMIS_Spatial_Average: method  = ', method 
       write(*,*) 'SSMIS_Spatial_Average: nchanl  = ', nchanl 
       write(*,*) 'SSMIS_Spatial_Average: num_obs = ', num_obs 
       write(*,*) 'SSMIS_Spatial_Average: Gaussian Weighted Averaging = ', gaussian_wgt 
       write(*,*) 'SSMIS_Spatial_Average: time min/max     = ', minval(time), maxval(time) 
       write(*,*) 'SSMIS_Spatial_Average: lat min/max      = ', minval(lat), maxval(lat) 
       write(*,*) 'SSMIS_Spatial_Average: lon min/max      = ', minval(lon), maxval(lon) 
       write(*,*) 'SSMIS_Spatial_Average: fov min/max      = ', minval(fov), maxval(fov) 
       write(*,*) 'SSMIS_Spatial_Average: rflg min/max     = ', minval(rflg), maxval(rflg) 
       write(*,*) 'SSMIS_Spatial_Average: bt_inout min/max = ', minval(bt_inout), maxval(bt_inout) 
       write(*,*) 'SSMIS_Spatial_Average: bt_obs min/max   = ', minval(bt_obs), maxval(bt_obs) 
       write(*,*) 'SSMIS_Spatial_Average: there are values below 50K in bt_obs  ', below_50
       write(*,*) 'SSMIS_Spatial_Average: there are values above 500K in bt_obs ', above_500

       ! Determine scanline from time
       !==============================
       write(*,*) 'SSMIS_Spatial_Average: determine scanline from time ...'
       allocate(scanline(num_obs))
       t1          = time(1)  ! time for first scanline
       nscan       = 1        ! first scanline
       scanline(1) = nscan
       write(1000+bufrsat,100) 'iobs', 'fov', 'time', 'time-1', 't2', 't1', 'tdiff', 'slnm'
       do iobs = 2, num_obs
          t2    = time(iobs) 
          tdiff = t2-t1
!         if (tdiff >= 0.00001_r_kind) then  ! tdiff unit: hour 
          if (tdiff >= 1_r_kind) then        ! tdiff unit: second 
             nscan = nscan+1 
             t1    = t2
          endif
          scanline(iobs) = nscan

          write(1000+bufrsat,101) iobs, fov(iobs), time(iobs), time(iobs-1), t2, t1, tdiff, scanline(iobs)
       enddo
100    format(2(a6,2x),2(a12,2x), 2(a12,2x), a12,  2x,a6)
101    format(2(i6,2x),2(i12,2x), 2(i12,2x), f12.5,2x,i6)
       max_scan = maxval(scanline)
       write(*,*) 'SSMIS_Spatial_Average: max_scan,max_fov,nchanl = ', &
                 max_scan,max_fov,nchanl
       write(*,*) 'SSMIS_Spatial_Average: scanline min/max = ', minval(scanline), maxval(scanline) 

!       ! Determine scanline from time
!       ! This approach should also 
!       ! work if the precision in 
!       ! time includes miliseonds
!       !==============================
!       allocate(scanline(num_obs))
!       allocate(scanlinex(num_obs))
!       mintime = MINVAL(time)
!       scanline(1:num_obs) = NINT(DBLE((time(1:num_obs)-mintime)/scan_interval))+1
!!      scanline(1:num_obs) = FLOOR((time(1:num_obs)-mintime)/scan_interval)+1
!       scanlinex(1:num_obs) = DBLE((time(1:num_obs)-mintime)/scan_interval)
!       max_scan=MAXVAL(scanline)
!       write(1000+bufrsat,100) 'iobs','fov','snlm','rflg','snlmx','lat','lon','time','tmin','dscan','bt'
!       do i = 1, num_obs
!          write(1000+bufrsat,101) i, fov(i), scanline(i), rflg(i), scanlinex(i), lat(i), lon(i), time(i), mintime, scan_interval, bt_obs(:,i)
!       enddo
!100    format(4(a6,2x),a8,  2x,2(a12  ,2x),a12,2x,a12,a8,a8)
!101    format(4(i6,2x),f8.3,2x,2(f12.5,2x),i12,2x,i12,f8.3,24(f8.3,2x))
!       write(*,*) 'SSMIS_Spatial_Average: mintime          = ', mintime
!       write(*,*) 'SSMIS_Spatial_Average: time min/max     = ', minval(time), maxval(time)
!       write(*,*) 'SSMIS_Spatial_Average: scanline min/max = ', minval(scanline), maxval(scanline)
!       write(*,*) 'SSMIS_Spatial_Average: rflg min/max     = ', minval(rflg), maxval(rflg)
!       write(*,*) 'SSMIS_Spatial_Average: max_scan         = ', max_scan
!       write(*,*) 'SSMIS_Spatial_Average: max_fov          = ', max_fov

       ! Index between 1D and 2D data arrays 
       write(2000+bufrsat,200) 'iobs','fov','snlm','snlmbk','rflg','lat','lon''time','tmin','dscan','bt'
       allocate(scanline_back(max_fov,max_scan))
       scanline_back(:,:) = -1_i_kind
       do iobs=1,num_obs
          scanline_back(fov(iobs),scanline(iobs))=iobs
          write(2000+bufrsat,201) iobs, fov(iobs), scanline(iobs), scanline_back(fov(iobs),scanline(iobs)), rflg(iobs), &
                                  lat(iobs),lon(iobs),time(iobs), mintime, scan_interval, bt_obs(:,iobs)
       end do
       write(6,*) 'SSMIS_Spatial_Average: scanline_back min/max   = ', minval(scanline_back), maxval(scanline_back)
       write(6,*) 'SSMIS_Spatial_Average: scanline      min/max   = ', minval(scanline), maxval(scanline)
       write(*,*) 'SSMIS_Spatial_Average: max_scan,max_fov,nchanl = ', max_scan,max_fov,nchanl
200    format(5(a6,2x),2(a12,  2x),2(a12,2x),a8,  2x,24(f8.3,2x))
201    format(5(i6,2x),2(f12.5,2x),2(i12,2x),f8.3,2x,24(f8.3,2x))

!      Allocate and initialize variables
       allocate(bt_image(max_fov,max_scan,nchanl))
       allocate(latitude(max_fov,max_scan))
       allocate(longitude(max_fov,max_scan))
       allocate(rainflag(max_fov,max_scan))
       allocate(nodeinfo(max_fov,max_scan))
       bt_image(:,:,:) = 1000.0_r_kind
       latitude(:,:)   = 1000.0_r_kind 
       longitude(:,:)  = 1000.0_r_kind 
       nodeinfo(:,:)   = 1000_i_kind 
       rainflag(:,:)   = 1000_i_kind 

!      Put data into 2D (fov vs. scanline) array
       write(*,*) 'SSMIS_Spatial_Average: put data into 2D ...'
       write(3000+bufrsat,300) 'iobs','snlmbk','lat','lon','fov','snlm','rflg','bt'
       do iobs = 1, num_obs
          latitude(fov(iobs),scanline(iobs))      = lat(iobs) 
          longitude(fov(iobs),scanline(iobs))     = lon(iobs) 
          rainflag(fov(iobs),scanline(iobs))      = rflg(iobs) 
!         bt_image(fov(iobs),scanline(iobs),:) = bt_obs(:,iobs)
          if (rainflag(fov(iobs),scanline(iobs)) < 1_i_kind) bt_image(fov(iobs),scanline(iobs),:) = bt_obs(:,iobs)
          scanline_back(fov(iobs),scanline(iobs)) = iobs
          write(3000+bufrsat,301) iobs, scanline_back(fov(iobs),scanline(iobs)), &
                          latitude(fov(iobs),scanline(iobs)), longitude(fov(iobs),scanline(iobs)), &
                          fov(iobs), scanline(iobs), rainflag(fov(iobs),scanline(iobs)), bt_image(fov(iobs),scanline(iobs),1:nchanl)
300       format(2(a6,2x),2(a12,2x),3(a6,2x),24(a8))
301       format(2(i6,2x),2(f12.5,2x),3(i6,2x),24(f8.3))
       enddo
       write(*,*) 'SSMIS_Spatial_Average: latitude min/max      = ', minval(latitude), maxval(latitude) 
       write(*,*) 'SSMIS_Spatial_Average: longitude min/max     = ', minval(longitude), maxval(longitude) 
       write(*,*) 'SSMIS_Spatial_Average: scanline_back min/max = ', minval(scanline_back), maxval(scanline_back)
       write(*,*) 'SSMIS_Spatial_Average: bt_image min/max      = ', minval(bt_image), maxval(bt_image)
       write(*,*) 'SSMIS_Spatial_Average: rainflag min/max      = ', minval(rainflag), maxval(rainflag)

!>>debug
       write(33000+bufrsat,302) 'iscan', 'ifov', 'slnmbk', 'lat', 'lon'
       do iscan = 1, max_scan
          do ifov = 1, max_fov
              write(33000+bufrsat,303) iscan, ifov, scanline_back(ifov,iscan),latitude(ifov,iscan), longitude(ifov,iscan)
           enddo
       enddo
302    format(3(a6,2x),2(a12  ,2x))
303    format(3(i6,2x),2(f12.5,2x))

!<<debug

!       write(*,*) 'SSMIS_Spatial_Average: determine node ...' 
!!      Determine AS/DS node information for each scanline
!       loop1: do iscan = 1, max_scan-1
!          loop2: do ifov = 1, max_fov
!             if (scanline_back(ifov,iscan) > 0 .and. scanline_back(ifov,iscan+1) > 0) then
!                dlat = latitude(ifov,iscan+1)-latitude(ifov,iscan)
!                if (dlat < 0.0_r_kind) then
!                   nodeinfo(:,iscan) = ds_node 
!                else
!                   nodeinfo(:,iscan) = as_node 
!                endif
!                cycle loop1
!             endif
!          enddo loop2
!       enddo loop1
!       nodeinfo(:,max_scan) = nodeinfo(:,max_scan-1)
!       write(*,*) 'SSMIS_Spatial_Average: nodeinfo min/max = ', minval(nodeinfo), maxval(nodeinfo)
!
!!>>debug
!       write(4000+bufrsat,400)'iscan','ifov','dlat','lat1','lat','lon','node','bt'
!       loopa: do iscan = 1, max_scan-1
!          loopb: do ifov = 1, max_fov
!             if (scanline_back(ifov,iscan) > 0 .and. scanline_back(ifov,iscan+1) > 0) then
!                dlat = latitude(ifov,iscan+1)-latitude(ifov,iscan)
!             endif
!             write(4000+bufrsat,401) iscan, ifov, dlat, latitude(ifov,iscan+1), latitude(ifov,iscan), longitude(ifov,iscan), &
!                            nodeinfo(ifov,iscan), minval(bt_image(ifov,iscan,:)), maxval(bt_image(ifov,iscan,:))
!          enddo loopb
!       enddo loopa
!400    format(2(a6,2x),4(a25,2x),    (a6,2x),2(a8,2x))
!401    format(2(i6,2x),4(es25.18,2x),(i6,2x),2(f8.3,2x))
!<<debug

       write(5000+bufrsat,500) 'iobs', 'iscan', 'ifov', 'slnm', 'fov', 'rflg','lat', 'lon'
       write(6000+bufrsat,600) 'iobs', 'iscan', 'ifov', 'slnm', 'fov', 'rflg','lat', 'lon'
       write(*,*) 'SSMIS_Spatial_Average: do spatial averaging ... '
!      Do spatial averaging in the box centered on each fov for each channel
!$omp parallel do  schedule(dynamic,1)private(ic,iobs,iscan,ifov,ns1,ns2,np1,np2,xnum,mta,is,ip,lat1,lon1,lat2,lon2,dist,wgt)
       scan_loop: do iscan = 1, max_scan 
          fov_loop: do ifov = 1, max_fov 

             iobs = scanline_back(ifov,iscan) 
             write(5000+bufrsat,501) iobs, iscan, ifov, scanline(iobs), fov(iobs), rainflag(ifov,iscan), latitude(ifov,iscan), longitude(ifov,iscan)
             write(5000+bufrsat,502) bt_image(ifov,iscan,:) 
             write(5000+bufrsat,502) bt_obs(:,iobs) 
500          format(6(a6,2x),2(a12  ,2x))
501          format(6(i6,2x),2(f12.5,2x))
502          format(24(f8.3,2x))
             if (iobs >0) then 
                node_inout(iobs) = nodeinfo(ifov,iscan)
!               Define grid box (3 (scan direction) x 7 (satellite track dir))
                ns1 = iscan-3          
                ns2 = iscan+3          
                if (ns1 < 1) ns1=1
                if (ns2 > max_scan) ns2=max_scan
                np1 = ifov-1          
                np2 = ifov+1          
                if (np1 < 1) np1=1
                if (np2 > max_fov) np2=max_fov

                channel_loop: do ic = 1, nchanl  
                   xnum   = 0.0_r_kind
                   mta    = 0.0_r_kind
                   if (any(bt_image(np1:np2,ns1:ns2,ic) < btmin .or. &
                           bt_image(np1:np2,ns1:ns2,ic) > btmax)) then 
                      bt_obs(ic,iobs) = 1000.0_r_kind 
                   else
!                     ! Calculate distance of each fov to the center fov 
                      box_y1: do is = ns1, ns2 
                      box_x1: do ip = np1, np2 
                         lat1 = latitude(ifov,iscan)    ! lat of the center fov
                         lon1 = longitude(ifov,iscan)   ! lon of the center fov
                         lat2 = latitude(ip,is)          
                         lon2 = longitude(ip,is)
                         dist = distance(lat1,lon1,lat2,lon2) 
                         if (dist > 100.0_r_kind) cycle box_x1  ! outside the box 
                         if (gaussian_wgt) then
                            wgt = exp(-0.5_r_kind*(dist*sigma)*(dist*sigma))
                         else
                            wgt = 1.0
                         endif
                         xnum   = xnum+wgt
                         mta    = mta +wgt*bt_image(ip,is,ic)
                      enddo box_x1
                      enddo box_y1
                      bt_obs(ic,iobs) = mta/xnum
                   endif
                enddo channel_loop 
             endif
             write(6000+bufrsat,601) iobs, iscan, ifov, scanline(iobs), fov(iobs), rainflag(ifov,iscan), latitude(ifov,iscan), longitude(ifov,iscan)
             write(6000+bufrsat,602) bt_image(ifov,iscan,:) 
             write(6000+bufrsat,602) bt_obs(:,iobs) 
600          format(6(a6,2x),2(a12  ,2x))
601          format(6(i6,2x),2(f12.5,2x))
602          format(24(f8.3,2x))
          enddo fov_loop
       enddo scan_loop
       write(*,*) 'SSMIS_Spatial_Average: spatial averaging Done ... '
       bt_inout = reshape(bt_obs, (/nchanl*num_obs/))

       write(*,*) 'SSMIS_Spatial_Average: deallocating arrays... '
!      Deallocate arrays
       deallocate(nodeinfo,scanline_back)
       deallocate(scanline)
!      deallocate(scanlinex)
       deallocate(latitude,longitude)
       deallocate(bt_image)
    endif ! Method=1

!============================================================================================================

!   Simple method for GMI (like method 1)
    if (Method == 2) then  ! simple averaging
       gaussian_wgt = .false.
!       write(*,*) 'SSMIS_Spatial_Average for GMI: using method from Banghua'
       write(*,*) 'SSMIS_Spatial_Average for GMI: bufrsat = ', BufrSat
       write(*,*) 'SSMIS_Spatial_Average for GMI: Gaussian Weighted Averaging =',gaussian_wgt

       ! Determine scanline from time
       !==============================
       allocate(scanline(num_obs))
       t1          = time(1)  ! time for first scanline
       nscan       = 1        ! first scanline
       scanline(1) = nscan
       do iobs = 2, num_obs
          t2    = time(iobs)
          tdiff = t2-t1
          if (tdiff >= 0.00001_r_kind) then
             nscan = nscan+1
             t1    = t2
          endif
          scanline(iobs) = nscan
       enddo
       max_scan = maxval(scanline)
       write(*,*) 'SSMIS_Spatial_Average for GMI:max_scan,max_fov,nchanl = ', &
                 max_scan,max_fov,nchanl

!      Allocate and initialize variables
       allocate(bt_image(max_fov_gmi,max_scan,nchanl))
       allocate(latitude(max_fov_gmi,max_scan))
       allocate(longitude(max_fov_gmi,max_scan))
       allocate(nodeinfo(max_fov_gmi,max_scan))
       allocate(scanline_back(max_fov_gmi,max_scan))
       bt_image(:,:,:)= 1000.0_r_kind
       latitude(:,:)       = 1000.0_r_kind
       longitude(:,:)      = 1000.0_r_kind
       scanline_back(:,:)  = -1
       nodeinfo(:,:)       = 1000_i_kind

!      Put data into 2D (fov vs. scanline) array
       do iobs = 1, num_obs
          latitude(fov(iobs),scanline(iobs))       = lat(iobs)
          longitude(fov(iobs),scanline(iobs))      = lon(iobs)
          bt_image(fov(iobs),scanline(iobs),:)= bt_obs(:,iobs)
          scanline_back(fov(iobs),scanline(iobs))  = iobs
       enddo

!      Determine AS/DS node information for each scanline
       gmi_loop1: do iscan = 1, max_scan-1
          gmi_loop2: do ifov = 1, max_fov_gmi
             if (scanline_back(ifov,iscan) > 0 .and. scanline_back(ifov,iscan+1)> 0) then
                dlat = latitude(ifov,iscan+1)-latitude(ifov,iscan)
                if (dlat < 0.0_r_kind) then
                   nodeinfo(:,iscan) = ds_node
                else
                   nodeinfo(:,iscan) = as_node
                endif
                cycle gmi_loop1
             endif
          enddo gmi_loop2
       enddo gmi_loop1
       nodeinfo(:,max_scan) = nodeinfo(:,max_scan-1)

!      Do spatial averaging in the box centered on each fov for each channel
       gmi_scan_loop: do iscan = 1, max_scan
          gmi_fov_loop: do ifov = 1, max_fov_gmi    
             iobs = scanline_back(ifov,iscan)
             if (iobs >0) then
                node_inout(iobs) = nodeinfo(ifov,iscan)
                gmi_channel_loop: do ic = 1, nchanl
!            Define grid box by channel -
!            Ch 1-2: 1 scan direction, 1 track direction
!            Ch 3-13: 3 scan direction, 3 track direction

                   ns1 = iscan-4
                   ns2 = iscan+4
                   if (ns1 < 1) ns1=1
                   if (ns2 > max_scan) ns2=max_scan
                   np1 = ifov-8
                   np2 = ifov+8
                   if (np1 < 1) np1=1
                   if (np2 > max_fov_gmi) np2=max_fov_gmi
                   xnum   = 0.0_r_kind
                   mta    = 0.0_r_kind
                   if (any(bt_image(np1:np2,ns1:ns2,ic) < btmin .or. &
                           bt_image(np1:np1,ns1:ns2,ic) > btmax)) then
                       bt_obs(ic,iobs) = 1000.0_r_kind
                   else
                ! Calculate distance of each fov to the center fov
                      gmi_box_y1: do is = ns1, ns2
                      gmi_box_x1: do ip = np1, np2
                         lat1 = latitude(ifov,iscan)    ! lat of the center fov
                         lon1 = longitude(ifov,iscan)   ! lon of the center fov
                         lat2 = latitude(ip,is)
                         lon2 = longitude(ip,is)
                         dist = distance(lat1,lon1,lat2,lon2)
                         if (dist > 20.0_r_kind) cycle gmi_box_x1  ! outside the box
                         if (gaussian_wgt) then
                            wgt = exp(-0.5_r_kind*(dist/sigma)*(dist/sigma))
                         else
                            wgt = 1.0
                         endif
                         xnum   = xnum+wgt
                         mta    = mta +wgt*bt_image(ip,is,ic)
                      enddo gmi_box_x1
                      enddo gmi_box_y1
                      bt_obs(ic,iobs) = mta/xnum
                   endif
                enddo gmi_channel_loop
             endif
          enddo gmi_fov_loop
       enddo gmi_scan_loop

       bt_inout = reshape(bt_obs, (/nchanl*num_obs/))

!      Deallocate arrays
       deallocate(nodeinfo,scanline,scanline_back)
       deallocate(latitude,longitude)
       deallocate(bt_image)
    endif ! Method=2

!============================================================================================================

!   Simple method for AMSR2 (like method 1)
    if (Method == 3) then  ! simple averaging 1
       gaussian_wgt = .false.
!       write(*,*) 'SSMIS_Spatial_Average for AMSR2: using method from Banghua'
       write(*,*) 'SSMIS_Spatial_Average for AMSR2: bufrsat = ', BufrSat
       write(*,*) 'SSMIS_Spatial_Average for AMSR2: Gaussian Weighted Averaging=',gaussian_wgt

       ! Determine scanline from time
       !==============================
       allocate(scanline(num_obs))
       t1          = time(1)  ! time for first scanline
       nscan       = 1        ! first scanline
       scanline(1) = nscan
       do iobs = 2, num_obs
          t2    = time(iobs)
          tdiff = t2-t1
          if (tdiff >= 0.00001_r_kind) then
             nscan = nscan+1
             t1    = t2
          endif
          scanline(iobs) = nscan
       enddo
       max_scan = maxval(scanline)
       write(*,*) 'SSMIS_Spatial_Average for AMSR2:max_scan,max_fov,nchanl = ', &
                 max_scan,max_fov,nchanl

!      Allocate and initialize variables
       allocate(bt_image(max_fov_amsr2,max_scan,nchanl))
       allocate(latitude(max_fov_amsr2,max_scan))
       allocate(longitude(max_fov_amsr2,max_scan))
       allocate(nodeinfo(max_fov_amsr2,max_scan))
       allocate(scanline_back(max_fov_amsr2,max_scan))
       bt_image(:,:,:)= 1000.0_r_kind
       latitude(:,:)       = 1000.0_r_kind
       longitude(:,:)      = 1000.0_r_kind
       scanline_back(:,:)  = -1
       nodeinfo(:,:)       = 1000_i_kind

!      Put data into 2D (fov vs. scanline) array
       do iobs = 1, num_obs
          latitude(fov(iobs),scanline(iobs))       = lat(iobs)
          longitude(fov(iobs),scanline(iobs))      = lon(iobs)
          bt_image(fov(iobs),scanline(iobs),:)= bt_obs(:,iobs)
          scanline_back(fov(iobs),scanline(iobs))  = iobs
       enddo

!      Determine AS/DS node information for each scanline
       amsr2_loop1: do iscan = 1, max_scan-1
          amsr2_loop2: do ifov = 1, max_fov_amsr2
             if (scanline_back(ifov,iscan) > 0 .and. scanline_back(ifov,iscan+1)> 0) then
                dlat = latitude(ifov,iscan+1)-latitude(ifov,iscan)
                if (dlat < 0.0_r_kind) then
                   nodeinfo(:,iscan) = ds_node
                else
                   nodeinfo(:,iscan) = as_node
                endif
                cycle amsr2_loop1
             endif
          enddo amsr2_loop2
       enddo amsr2_loop1
       nodeinfo(:,max_scan) = nodeinfo(:,max_scan-1)

!      Do spatial averaging in the box centered on each fov for each channel
       amsr2_scan_loop: do iscan = 1, max_scan
          amsr2_fov_loop: do ifov = 1, max_fov_amsr2
             iobs = scanline_back(ifov,iscan)
             if (iobs >0) then
                node_inout(iobs) = nodeinfo(ifov,iscan)
                amsr2_channel_loop: do ic = 1, nchanl
!            Define grid box by channel -
!            Ch 1-6: 1 scan direction, 1 track direction
!            Ch 7-14: 3 scan direction, 3 track direction
                   if ((ic >= 1) .and. (ic <= 6)) then
                      ns1 = iscan
                      ns2 = iscan
                      if (ns1 < 1) ns1=1
                      if (ns2 > max_scan) ns2=max_scan
                      np1 = ifov
                      np2 = ifov
                      if (np1 < 1) np1=1
                      if (np2 > max_fov_gmi) np2=max_fov_amsr2
                   else if ((ic >= 7) .and. (ic <= 14)) then
                      ns1 = iscan-1
                      ns2 = iscan+1
                      if (ns1 < 1) ns1=1
                      if (ns2 > max_scan) ns2=max_scan
                      np1 = ifov-1
                      np2 = ifov+1
                      if (np1 < 1) np1=1
                      if (np2 > max_fov_amsr2) np2=max_fov_amsr2
                   endif

                   xnum   = 0.0_r_kind
                   mta    = 0.0_r_kind
                   if (any(bt_image(np1:np2,ns1:ns2,ic) < btmin .or. &
                           bt_image(np1:np1,ns1:ns2,ic) > btmax)) then
                       bt_obs(ic,iobs) = 1000.0_r_kind
                   else
                ! Calculate distance of each fov to the center fov
                      amsr2_box_y1: do is = ns1, ns2
                      amsr2_box_x1: do ip = np1, np2
                         lat1 = latitude(ifov,iscan)    ! lat of the center fov
                         lon1 = longitude(ifov,iscan)   ! lon of the center fov
                         lat2 = latitude(ip,is)
                         lon2 = longitude(ip,is)
                         dist = distance(lat1,lon1,lat2,lon2)
                         if (dist > 50.0_r_kind) cycle amsr2_box_x1  ! outside the box
                         if (gaussian_wgt) then
                            wgt = exp(-0.5_r_kind*(dist/sigma)*(dist/sigma))
                         else
                            wgt = 1.0
                         endif
                         xnum   = xnum+wgt
                         mta    = mta +wgt*bt_image(ip,is,ic)
                      enddo amsr2_box_x1
                      enddo amsr2_box_y1
                      bt_obs(ic,iobs) = mta/xnum
                   endif
                enddo amsr2_channel_loop
             endif
          enddo amsr2_fov_loop
       enddo amsr2_scan_loop

       bt_inout = reshape(bt_obs, (/nchanl*num_obs/))

!      Deallocate arrays
       deallocate(nodeinfo,scanline,scanline_back)
       deallocate(latitude,longitude)
       deallocate(bt_image)
    endif ! Method=3


  END SUBROUTINE SSMIS_Spatial_Average

END MODULE SSMIS_Spatial_Average_Mod
