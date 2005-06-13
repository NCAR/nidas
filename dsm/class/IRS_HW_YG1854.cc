/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#include <IRS_HW_YG1854.h>

using namespace dsm;

CREATOR_ENTRY_POINT(IRS_HW_YG1854);

float IRS_HW_YG1854::processLabel(const unsigned long data)
{
//err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );

  switch (data & 0xff) {
  case 0310:  // present_lat
  case 0311:  // present_lon
  case 0312:  // ground_speed
  case 0313:  // track_angle_true
  case 0314:  // true_heading
  case 0315:  // wind_speed
  case 0316:  // wind_dir_true
  case 0321:  // drift_angle
  case 0324:  // pitch_angle
  case 0325:  // roll_angle
  case 0326:  // pitch_rate
  case 0327:  // roll_rate
  case 0330:  // yaw_rate
  case 0331:  // long_accel
  case 0332:  // lat_accel
  case 0333:  // normal_accel
  case 0335:  // track_ang_rate
  case 0361:  // inertial_alt
  case 0364:  // vertical_accel
  case 0365:  // inrt_vert_speed
  case 0366:  // velocity_ns
  case 0367:  // velocity_ew
  default:
    break;
  }
  return 42.0;
}
