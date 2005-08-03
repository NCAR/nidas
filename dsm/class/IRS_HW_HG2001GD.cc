/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/
#include <IRS_HW_HG2001GD.h>

using namespace dsm;

CREATOR_ENTRY_POINT(IRS_HW_HG2001GD);

float IRS_HW_HG2001GD::processLabel(const unsigned long data)
{
  unsigned long ulPackedBcdData;
  unsigned long ulBinaryData = 0L;
  unsigned long ulPlaceValue = 1L;
  int sign = 1;

//err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );

  switch (data & 0xff) {

  case 0007:  // BCD - time to nav          (min)
    if ((data & SSM == NCD) || (data & SSM == TST)) break;
    ulPackedBcdData = (data & (m08<<8)) >> 18;
    for (int i = 0; i < 8; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1; // no sign

  case 0010:  // BCD - pos latitude         (deg)
    if ((data & SSM == NCD) || (data & SSM == TST)) break;
    if (data & SSM == SSM) sign = -1;
    ulPackedBcdData = (data & 0x0fffff00) >> 8;
    for (int i = 0; i < 20; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1 * sign;

  case 0011:  // BCD - pos longitude        (deg)
    if ((data & SSM == NCD) || (data & SSM == TST)) break;
    if (data & SSM == SSM) sign = -1;
    ulPackedBcdData = (data & 0x1fffff00) >> 8;
    for (int i = 0; i < 21; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1 * sign;

  case 0012:  // BCD - ground speed         (knot)
    if ((data & SSM == NCD) || (data & SSM == TST)) break;
    ulPackedBcdData = (data & (m15<<4)) >> 14;
    for (int i = 0; i < 15; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 1.0; // no sign

  case 0013:  // BCD - trk angle true       (deg)
  case 0014:  // BCD - mag heading          (deg)
    if ((data & SSM == NCD) || (data & SSM == TST)) break;
    ulPackedBcdData = (data & (m12<<4)) >> 14;
    for (int i = 0; i < 12; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1; // no sign

  case 0015:  // BCD - wind speed           (knot)
  case 0016:  // BCD - wind dir true        (deg)
    if ((data & SSM == NCD) || (data & SSM == TST)) break;
    ulPackedBcdData = (data & (m08<<8)) >> 18;
    for (int i = 0; i < 8; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 1.0; // no sign

  case 0044:  // BCD - true heading         (deg)
    if ((data & SSM == NCD) || (data & SSM == TST)) break;
    ulPackedBcdData = (data & (m12<<4)) >> 14;
    for (int i = 0; i < 12; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1; // no sign

  case 0126:  // BNR - Time in Nav          (min)
    if (data & SSM != SSM) break;
    return ((data & m15) >> 10) * 1.0; // no sign

  case 0226:  // DIS - Data Loader SAL      ()
  case 0270:  // DIS - irs_discretes        ()
  case 0277:  // DIS - Test Word            ()
    break;

  case 0300:  // BNR - delta theta x        (radian)
  case 0301:  // BNR - delta theta y        (radian)
  case 0302:  // BNR - delta theta z        (radian)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & 0x3fffff00) >> 8) * 1.0/(1<<28) * sign;

  case 0303:  // BNR - delta theta v x      (ft/s)
  case 0304:  // BNR - delta theta v y      (ft/s)
  case 0305:  // BNR - delta theta v z      (ft/s)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & 0x3fffff00) >> 8) * 1.0/(1<<21) * sign;

  case 0310:  // BNR - present_lat          (deg)
  case 0311:  // BNR - present_lon          (deg)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & 0x0fffff00) >> 8) * 180.0/(1<<20) * sign;

  case 0312:  // BNR - ground_speed         (knot)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<6) * sign;

  case 0313:  // BNR - track_angle_true     (deg)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 180.0/(1<<18) * sign;

  case 0314:  // BNR - true_heading         (deg)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 180.0/(1<<18) * sign + _irs_thdg_corr;

  case 0315:  // BNR - wind_speed           (knot)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<10) * sign;

  case 0316:  // BNR - wind_dir_true        (deg)
  case 0317:  // BNR - trk angle mag        (deg)
  case 0320:  // BNR - mag heading          (deg)
  case 0321:  // BNR - drift_angle          (deg)
  case 0322:  // BNR - flt pth angle        (deg)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 180.0/(1<<18) * sign;

  case 0323:  // BNR - flt pth accel        (G)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<16) * sign;

  case 0324:  // BNR - pitch_angle          (deg)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 180.0/(1<<18) * sign + _irs_ptch_corr;

  case 0325:  // BNR - roll_angle           (deg)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 180.0/(1<<18) * sign + _irs_roll_corr;

  case 0326:  // BNR - pitch_rate           (deg/s)
  case 0327:  // BNR - roll_rate            (deg/s)
  case 0330:  // BNR - yaw_rate             (deg/s)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<11) * sign;

  case 0331:  // BNR - long_accel           (G)
  case 0332:  // BNR - lat_accel            (G)
  case 0333:  // BNR - normal_accel         (G)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<16) * sign;

  case 0334:  // BNR - platform_hdg         (deg)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 180.0/(1<<18) * sign;

  case 0335:  // BNR - track_ang_rate       (deg/s)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<13) * sign;

  case 0336:  // BNR - pitch_att_rate       (deg/s)
  case 0337:  // BNR - roll_att_rate        (deg/s)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<11) * sign;

  case 0350:  // DIS - irs_maint_discretes  ()
    break;

  case 0351:  // BCD - time_to_nav_ready    (min)
    if ((data & SSM == NCD) || (data & SSM == TST)) break;
    ulPackedBcdData = (data & (m08<<4)) >> 14;
    for (int i = 0; i < 8; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1; // no sign

  case 0354:  // BNR - total time           (count)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m19) >> 10) * 1.0; // no sign

  case 0360:  // BNR - pot_vert_speed       (ft/min)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<3) * sign;

  case 0361:  // BNR - inertial_alt         (ft)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & 0x0fffff00) >> 8) * 1.0/(1<<3) * sign;

  case 0362:  // BNR - along trk accel      (G)
  case 0363:  // BNR - cross trk accel      (G)
  case 0364:  // BNR - vertical_accel       (G)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<16) * sign;

  case 0365:  // BNR - inrt_vert_speed      (ft/min)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<3) * sign;

  case 0366:  // BNR - velocity_ns          (knot)
  case 0367:  // BNR - velocity_ew          (knot)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<6) * sign;

  case 0370:  // BNR - norm_accel           (G)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<15) * sign;

  case 0371:  // DIS - equipment_id         ()
    break;

  case 0375:  // BNR - along hdg accel      (G)
  case 0376:  // BNR - cross hdg accel      (G)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<16) * sign;

  default:
    break;
  }
  return _nanf;
}
