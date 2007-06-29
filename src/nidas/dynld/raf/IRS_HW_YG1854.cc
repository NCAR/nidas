/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#include <nidas/dynld/raf/IRS_HW_YG1854.h>

using namespace nidas::dynld::raf;

NIDAS_CREATOR_FUNCTION_NS(raf,IRS_HW_YG1854);

float IRS_HW_YG1854::processLabel(const long data)
{
  /* All clients of arinc20bit are in pirad's, so multiply by 180 right
   * now to get to degree's.
   */
  static const float arinc20bits = (1.0 / (1 << 20));
  static const float arinc15bits = (1.0 / (1 << 15));
  static const float arinc13bits = (1.0 / (1 << 13));
  static const float arinc8bits  = (1.0 / (1 << 8));

//err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );

  switch (data & 0xff) {
  case 0310:  // BNR - pos_latitude         (rad)
  case 0311:  // BNR - pos_longitude        (rad)
  case 0313:  // BNR - track_angle_true     (rad)
  case 0314:  // BNR - true_heading         (rad)
  case 0316:  // BNR - wind_dir_true        (rad)
  case 0321:  // BNR - drift_angle          (rad)
  case 0324:  // BNR - pitch_angle          (rad)
  case 0325:  // BNR - roll_angle           (rad)
     return (float)(data>>11) * arinc20bits * 180.0;

  case 0312:  // BNR - ground_speed         (knot)
  case 0366:  // BNR - velocity_ns          (knot)
  case 0367:  // BNR - velocity_ew          (knot)
     return (float)(data>>11) * arinc8bits * KTS_MS;

  case 0315:  // BNR - wind_speed           (knot)
     return (float)(data>>10) * arinc13bits * KTS_MS;

  case 0326:  // BNR - pitch_rate           (deg/s)
  case 0327:  // BNR - roll_rate            (deg/s)
  case 0330:  // BNR - yaw_rate             (deg/s)
     return (float)(data>>16) * arinc8bits;

  case 0331:  // BNR - long_accel           (G)
  case 0332:  // BNR - lat_accel            (G)
  case 0333:  // BNR - normal_accel         (G)
     return (float)(data>>16) * arinc13bits * G_MPS2;

  case 0335:  // BNR - track_ang_rate       (deg/s)
     return (float)(data>>11) * arinc15bits;

  case 0361:  // BNR - inertial_alt         (f)
     return (float)(data>>11) * 0.125 * FT_MTR;

  case 0364:  // BNR - vertical_accel       (G)
     return (float)(data>>16) * arinc13bits * G_MPS2;

  case 0365:  // BNR - inrt_vert_speed      (ft/min)
     return (float)(data>>11) * 0.03125 * FPM_MPS;

  default:
    break;
  }
}
