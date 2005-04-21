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
  throw(atdUtil::IOException)
{
  unsigned long ulPackedBcdData;
  unsigned long ulBinaryData = 0L;
  unsigned long ulPlaceValue = 1L;

//err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );

  // check the SSM...
  switch (data & 0xff) {
  case 0300:  // delta theta x        (radian)
  case 0301:  // delta theta y        (radian)
  case 0302:  // delta theta z        (radian)
  case 0303:  // delta theta v x      (ft/s)
  case 0304:  // delta theta v y      (ft/s)
  case 0305:  // delta theta v z      (ft/s)
  default:
    if (data & 0x60000000 != 0x60000000)
      return _nanf;
  }

  switch (data & 0xff) {

  case 0007:  // time to nav          (min)
    ulPackedBcdData = (data & (m08<<4)) >> 14;
    for (int i = 0; i < 8; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1;

  case 0010:  // pos latitude         (deg)
  case 0011:  // pos longitude        (deg)
    ulPackedBcdData = (data & 0x0fffff00) >> 8;
    for (int i = 0; i < 20; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1;

  case 0012:  // ground speed         (knot)
    ulPackedBcdData = (data & (m15<<4)) >> 14;
    for (int i = 0; i < 15; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 1.0;

  case 0013:  // trk angle true       (deg)
  case 0014:  // mag heading          (deg)
    ulPackedBcdData = (data & (m12<<4)) >> 14;
    for (int i = 0; i < 12; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1;

  case 0015:  // wind speed           (knot)
  case 0016:  // wind dir true        (deg)
    ulPackedBcdData = (data & (m08<<8)) >> 18;
    for (int i = 0; i < 8; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 1.0;

  case 0044:  // true heading         (deg)
    ulPackedBcdData = (data & (m12<<4)) >> 14;
    for (int i = 0; i < 12; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1;

  case 0270:  // irs_discretes        ()
    break; // DIS

  case 0300:  // delta theta x        (radian)
  case 0301:  // delta theta y        (radian)
  case 0302:  // delta theta z        (radian)
    return ((data & 0x3fffff00) >> 8) * 1.0/(1<<28);

  case 0303:  // delta theta v x      (ft/s)
  case 0304:  // delta theta v y      (ft/s)
  case 0305:  // delta theta v z      (ft/s)
    return ((data & 0x3fffff00) >> 8) * 1.0/(1<<21);

  case 0310:  // present_lat          (deg)
  case 0311:  // present_lon          (deg)
    return ((data & 0x0fffff00) >> 8) * 180.0/(1<<20);

  case 0312:  // ground_speed         (knot)
    return ((data & m18) >> 10) * 1.0/(1<<6);

  case 0313:  // track_angle_true     (deg)
    return ((data & m18) >> 10) * 180.0/(1<<18);

  case 0314:  // true_heading         (deg)
    return ((data & m18) >> 10) * 180.0/(1<<18) + _irs_thdg_corr;

  case 0315:  // wind_speed           (knot)
    return ((data & m18) >> 10) * 1.0/(1<<10);

  case 0316:  // wind_dir_true        (deg)
  case 0317:  // trk angle mag        (deg)
  case 0320:  // mag heading          (deg)
  case 0321:  // drift_angle          (deg)
  case 0322:  // flt pth angle        (deg)
    return ((data & m18) >> 10) * 180.0/(1<<18);

  case 0323:  // flt pth accel        (G)
    return ((data & m18) >> 10) * 1.0/(1<<16);

  case 0324:  // pitch_angle          (deg)
    return ((data & m18) >> 10) * 180.0/(1<<18) + _irs_ptch_corr;

  case 0325:  // roll_angle           (deg)
    return ((data & m18) >> 10) * 180.0/(1<<18) + _irs_roll_corr;

  case 0326:  // pitch_rate           (deg/s)
  case 0327:  // roll_rate            (deg/s)
  case 0330:  // yaw_rate             (deg/s)
    return ((data & m18) >> 10) * 1.0/(1<<11);

  case 0331:  // long_accel           (G)
  case 0332:  // lat_accel            (G)
  case 0333:  // normal_accel         (G)
    return ((data & m18) >> 10) * 1.0/(1<<16);

  case 0334:  // platform_hdg         (deg)
    return ((data & m18) >> 10) * 180.0/(1<<18);

  case 0335:  // track_ang_rate       (deg/s)
    return ((data & m18) >> 10) * 1.0/(1<<13);

  case 0336:  // pitch_att_rate       (deg/s)
  case 0337:  // roll_att_rate        (deg/s)
    return ((data & m18) >> 10) * 1.0/(1<<11);

  case 0350:  // irs_maint_discretes  ()
    break; // DIS

  case 0351:  // time_to_nav_ready    (min)
    ulPackedBcdData = (data & (m08<<4)) >> 14;
    for (int i = 0; i < 8; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1;

  case 0354:  // total time           (count)
    return ((data & m19) >> 10) * 1.0;

  case 0360:  // pot_vert_speed       (ft/min)
    return ((data & m18) >> 10) * 1.0/(1<<3);

  case 0361:  // inertial_alt         (ft)
    return ((data & 0x0fffff00) >> 8) * 1.0/(1<<3);

  case 0362:  // along trk accel      (G)
  case 0363:  // cross trk accel      (G)
  case 0364:  // vertical_accel       (G)
    return ((data & m18) >> 10) * 1.0/(1<<16);

  case 0365:  // inrt_vert_speed      (ft/min)
    return ((data & m18) >> 10) * 1.0/(1<<3);

  case 0366:  // velocity_ns          (knot)
  case 0367:  // velocity_ew          (knot)
    return ((data & m18) >> 10) * 1.0/(1<<6);

  case 0370:  // norm_accel           (G)
    return ((data & m18) >> 10) * 1.0/(1<<15);

  case 0371:  // equipment_id         ()
    break; // DIS

  case 0375:  // along hdg accel      (G)
  case 0376:  // cross hdg accel      (G)
    return ((data & m18) >> 10) * 1.0/(1<<16);

  default:
    break;
  }
  return _nanf;
}
