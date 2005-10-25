/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    Taken from the Honeywell Engineering Specification A.4.1 spec. no.
    EB7022597 cage code 55939 "Air Data Computer"    (pages A-53..79).

 ******************************************************************
*/
#include <ADC_HW_EB7022597.h>

using namespace dsm;

CREATOR_FUNCTION(ADC_HW_EB7022597);

float ADC_HW_EB7022597::processLabel(const long data)
{
  int sign = 1;

//err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );

  switch (data & 0xff) {

  case 0203:  // BNR - pressure alt         (ft)
  case 0204:  // BNR - baro corr alt #1     (ft)
  case 0220:  // BNR - baro corr alt #2     (ft)
  case 0252:  // BNR - baro corr alt #4     (ft)
    if ((data & SSM) != SSM) break;
    return (data<<3>>13) * 0.5 * FT_MTR;

  case 0205:  // BNR - mach                 (mach)
    if ((data & SSM) != SSM) break;
    return (data<<3>>13) * 1.5625e-5;

  case 0206:  // BNR - computed air speed   (knot)
  case 0207:  // BNR - max oper. speed      (knot)
    if ((data & SSM) != SSM) break;
    return (data<<3>>13) * 3.90625e-3 * KTS_MS;

  case 0210:  // BNR - true air speed       (knot)
    if ((data & SSM) != SSM) break;
    return (data<<3>>13) * 7.8125e-3 * KTS_MS;

  case 0211:  // BNR - total air pressure   (deg_C)
  case 0213:  // BNR - static air pressure  (deg_C)
  case 0215:  // BNR - impact pressure      (mbar)
    if ((data & SSM) != SSM) break;
    return (data<<3>>13) * 1.953125e-3;

  case 0212:  // BNR - altitude rate        (ft/min)
    if ((data & SSM) != SSM) break;
    return (data<<3>>13) * 0.125 * FPM_MPS;

  case 0217:  // BNR - static pressure      (inHg)
    if ((data & SSM) != SSM) break;
    return (data<<3>>13) * 2.44140625e-4 * INHG_MBAR;

  case 0234:  // BCD - baro correction #1   (mbar)
  case 0236:  // BCD - baro correction #2   (mbar)
    if (((data & SSM) == NCD) || ((data & SSM) == TST)) break;
    if ((data & SSM) == SSM) sign = -1;
    return (
            ((data & (0x7<<26)) >> 26) * 1000.0 +
            ((data & (0xf<<22)) >> 22) * 100.0 +
            ((data & (0xf<<18)) >> 18) * 10.0 +
            ((data & (0xf<<14)) >> 14) * 1.0 +
            ((data & (0xf<<10)) >> 10) * 0.1
            ) * sign;

  case 0235:  // BCD - baro correction #1   (inHg)
  case 0237:  // BCD - baro correction #2   (inHg)
    if (((data & SSM) == NCD) || ((data & SSM) == TST)) break;
    if ((data & SSM) == SSM) sign = -1;
    return (
            ((data & (0x7<<26)) >> 26) * 10.0 +
            ((data & (0xf<<22)) >> 22) * 1.0 +
            ((data & (0xf<<18)) >> 18) * 0.1 +
            ((data & (0xf<<14)) >> 14) * 0.01 +
            ((data & (0xf<<10)) >> 10) * 0.001
            ) * sign * INHG_MBAR;

  case 0242:  // BNR - total pressure       (mbar)
    if ((data & SSM) != SSM) break;
    return (data<<3>>13) *  7.8125e-3;

  case 0244:  // BNR - norm angle of attack (ratio)
    if ((data & SSM) != SSM) break;
    return (data<<3>>13) * 7.62939453125e-6;

  case 0270:  // DIS - discrete #1          ()
  case 0271:  // DIS - discrete #2          ()
  case 0350:  // DIS - maintence data #1    ()
  case 0351:  // DIS - maintence data #2    ()
  case 0352:  // DIS - maintence data #3    ()
  case 0353:  // DIS - maintence data #4    ()
  case 0371:  // DIS - equipment_id         ()
  default:
    break;
  }
  return _nanf;
}
