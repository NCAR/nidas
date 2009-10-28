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
#include <cstdio>

using namespace nidas::dynld::raf;

NIDAS_CREATOR_FUNCTION_NS(raf,IRS_HW_YG1854);

float IRS_HW_YG1854::processLabel(const int data)
{
  int sign = 1;
  float carry = 0.0;
  float value;

//err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned int)0xffffff00) );

  switch (data & 0xff) {

  case 0007:  // BCD - time to nav          (min)
    if (((data & SSM) == NCD) || ((data & SSM) == TST)) break;
    return (
            ((data & (0xf<<22)) >> 22) * 1.0 +
            ((data & (0xf<<18)) >> 18) * 0.1
            ); // no sign

  case 0011:  // BCD - pos longitude        (deg)
    carry = ((data & (0x1<<28)) >> 28) *  100.0;
  case 0010:  // BCD - pos latitude         (deg)
    if (((data & SSM) == NCD) || ((data & SSM) == TST)) break;
    if ((data & SSM) == SSM) sign = -1;
    return (
            carry +
            ((data & (0xf<<24)) >> 24) *  10.0 +
            ((data & (0xf<<20)) >> 20) *  1.0 +
            (
             ((data & (0xf<<16)) >> 16) * 10.0 +
             ((data & (0xf<<12)) >> 12) * 1.0 +
             ((data & (0xf<< 8)) >>  8) * 0.1
             ) / 60.0
            ) * sign;

  case 0012:  // BCD - ground speed         (knot)
    if (((data & SSM) == NCD) || ((data & SSM) == TST)) break;
    return (
            ((data & (0x7<<26)) >> 26) * 1000.0 +
            ((data & (0xf<<22)) >> 22) * 100.0 +
            ((data & (0xf<<18)) >> 18) * 10.0 +
            ((data & (0xf<<14)) >> 14) * 1.0
            ) * KTS_MS; // no sign

  case 0013:  // BCD - trk angle true       (deg)
  case 0014:  // BCD - mag heading          (deg)
  case 0044:  // BCD - true heading         (deg)
    if (((data & SSM) == NCD) || ((data & SSM) == TST)) break;
    if ((data & SSM) == SSM) {sign = -1; carry = 360.0;}
    return (
            ((data & (0x3<<26)) >> 26) * 100.0 +
            ((data & (0xf<<22)) >> 22) * 10.0 +
            ((data & (0xf<<18)) >> 18) * 1.0 +
            ((data & (0xf<<14)) >> 14) * 0.1
            ) * sign + carry;

  case 0015:  // BCD - wind speed           (knot)
    if (((data & SSM) == NCD) || ((data & SSM) == TST)) break;
    return (
            ((data & (0x3<<26)) >> 26) * 100.0 +
            ((data & (0xf<<22)) >> 22) * 10.0 +
            ((data & (0xf<<18)) >> 18) * 1.0
            ) * KTS_MS; // no sign

  case 0016:  // BCD - wind dir true        (deg)
    if (((data & SSM) == NCD) || ((data & SSM) == TST)) break;
    if ((data & SSM) == SSM) {sign = -1; carry = 360.0;}
    return (
            ((data & (0x3<<26)) >> 26) * 100.0 +
            ((data & (0xf<<22)) >> 22) * 10.0 +
            ((data & (0xf<<18)) >> 18) * 1.0
            ) * sign + carry;

  case 0265:  // BNR - integ_vert_acc       (ft/s)
    if ((data & SSM) != SSM) break;
    return (data<<3>>11) * 1.0/(1<<13) * FT_MTR;

  case 0314:  // BNR - true_heading         (rad)
    carry = _irs_thdg_corr;
  case 0313:  // BNR - track_angle_true     (rad)
  case 0316:  // BNR - wind_dir_true        (rad)
    if ((data & SSM) != SSM) break;
    value = (data<<3>>11) * 1.0/(1<<20) * 180.0 + carry;  // 21 bits

    // C130 IRS puts out -180 to 180.  Convert to 0 to 360.
    if (value > 360.0)
      value -= 360.0;
    if (value < 0.0)
      value += 360.0;

    return value;

  case 0324:  // BNR - pitch_angle          (rad)
    carry = _irs_ptch_corr; goto corr;
  case 0325:  // BNR - roll_angle           (rad)
    carry = _irs_roll_corr; goto corr;
  case 0310:  // BNR - pos_latitude         (rad)
  case 0311:  // BNR - pos_longitude        (rad)
  case 0317:  // BNR - wtrack_angle_mag     (rad)
  case 0320:  // BNR - mag_heading          (rad)
  case 0321:  // BNR - drift_angle          (rad)
  case 0322:  // BNR - flight_path_angle    (rad)
  case 0334:  // BNR - platform_heading     (rad)
  corr:
    if ((data & SSM) != SSM) break;
    return (data<<3>>11) * 1.0/(1<<20) * 180.0 + carry;  // 21 bits

  case 0312:  // BNR - ground_speed         (knot)
  case 0366:  // BNR - velocity_ns          (knot)
  case 0367:  // BNR - velocity_ew          (knot)
    if ((data & SSM) != SSM) break;
    return (data<<3>>11) * 1.0/(1<<8) * KTS_MS; // no sign

  case 0315:  // BNR - wind_speed           (knot)
    if ((data & SSM) != SSM) break;
    return (data<<3>>11) * 1.0/(1<<13) * KTS_MS;

  case 0323:  // BNR - flight_path_accel    (G)
  case 0331:  // BNR - long_accel           (G)
  case 0332:  // BNR - lat_accel            (G)
  case 0333:  // BNR - normal_accel         (G)
  case 0362:  // BNR - along trk accel      (G)
  case 0363:  // BNR - cross trk accel      (G)
  case 0370:  // BNR - norm_accel           (G)
    if ((data & SSM) != SSM) break;
    return (data<<3>>16) * 1.0/(1<<13);

  case 0364:  // BNR - vertical_accel       (G)
    if ((data & SSM) != SSM) break;
    return (data<<3>>16) * 1.0/(1<<13) * G_MPS2;

  case 0326:  // BNR - pitch_rate           (deg/s)
  case 0327:  // BNR - roll_rate            (deg/s)
  case 0330:  // BNR - yaw_rate             (deg/s)
  case 0336:  // BNR - pitch_att_rate       (deg/s)
  case 0337:  // BNR - roll_att_rate        (deg/s)
    if ((data & SSM) != SSM) break;
    return (data<<3>>16) * 1.0/(1<<8);

  case 0335:  // BNR - track_ang_rate       (deg/s)
    if ((data & SSM) != SSM) break;
    return (data<<3>>11) * 1.0/(1<<15);

  case 0351:  // BCD - time_to_nav_ready    (min)
    if (((data & SSM) == NCD) || ((data & SSM) == TST)) break;
    return (
            ((data & (0xf<<18)) >> 18) * 1.0 +
            ((data & (0xf<<14)) >> 14) * 0.1
            ); // no sign

  case 0354:  // BNR - total time           (count)
    if ((data & SSM) != SSM) break;
    return (data<<3>>10) * 1.0; // no sign

  case 0360:  // BNR - pot_vert_speed       (ft/min)
    if ((data & SSM) != SSM) break;
    return (data<<3>>16) * 1.0 * FPM_MPS;

  case 0361:  // BNR - inertial_alt         (f)
     return (data<<3>>11) * 1.0/(1<<3) * FT_MTR;

  case 0365:  // BNR - inrt_vert_speed      (ft/min)
    if ((data & SSM) != SSM) break;
    return (data<<3>>11) * 1.0/(1<<5) * FPM_MPS;

  case 0270:  // DIS - irs_discretes        ()
  case 0350:  // DIS - irs_maint_discretes  ()
  case 0371:  // DIS - equipment_id         ()
  default:
    break;
  }
  return _nanf;
}
