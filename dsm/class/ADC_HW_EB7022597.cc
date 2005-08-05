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

CREATOR_ENTRY_POINT(ADC_HW_EB7022597);

float ADC_HW_EB7022597::processLabel(const unsigned long data)
{
  unsigned long ulPackedBcdData;
  unsigned long ulBinaryData = 0L;
  unsigned long ulPlaceValue = 1L;
  int sign = 1;

//err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );

  switch (data & 0xff) {

  case 0203:  // pressure alt                      (ft)
  case 0204:  // baro corrected alt #1             (ft)
  case 0220:  // baro corrected alt #2             (ft)
  case 0252:  // baro corrected alt #4             (ft)
    if (data & SSM != SSM) break;
    if (data & (1<<28)) sign = -1;
    return ((data & m18) >> 10) * 65536.0/(1<<17) * sign * FT_MTR;

  case 0205:  // mach                              (mach)
    if (data & SSM != SSM) break;
    if (data & (1<<28)) sign = -1;
    return ((data & m18) >> 10) * 2.048/(1<<17) * sign;

  case 0206:  // computed air speed                (knots)
  case 0207:  // maximum operating speed           (knots)
    if (data & SSM != SSM) break;
    if (data & (1<<28)) sign = -1;
    return ((data & m18) >> 10) * 512.0/(1<<17) * sign * KTS_MS;

  case 0210:  // true air speed                    (knots)
    if (data & SSM != SSM) break;
    if (data & (1<<28)) sign = -1;
    return ((data & m18) >> 10) * 1024.0/(1<<17) * sign * KTS_MS;

  case 0211:  // total air pressure                (deg_C)
  case 0213:  // static air pressure               (deg_C)
    if (data & SSM != SSM) break;
    if (data & (1<<28)) sign = -1;
    return ((data & m18) >> 10) * 256.0/(1<<17) * sign;

  case 0212:  // altitude rate                     (ft/min)
    if (data & SSM != SSM) break;
    if (data & (1<<28)) sign = -1;
    return ((data & m18) >> 10) * 16384.0/(1<<17) * sign * FPM_MPS;

  case 0215:  // impact pressure                   (mbar)
    if (data & SSM != SSM) break;
    if (data & (1<<28)) sign = -1;
    return ((data & m18) >> 10) * 256.0/(1<<17) * sign;

  case 0217:  // static pressure                   (inHg)
    if (data & SSM != SSM) break;
    if (data & (1<<28)) sign = -1;
    return   ((data & m18) >> 10) * 32.0/(1<<17) * sign * INHG_MBAR;

  case 0234:  // baro correction #1                (mbar)
  case 0236:  // baro correction #2                (mbar)
    if ((data & SSM == NCD) || (data & SSM == TST)) break;
    if (data & SSM == SSM) sign = -1;
    ulPackedBcdData = (data & m19) >> 10;
    for (int i = 0; i < 19; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1 * sign;

  case 0235:  // baro correction #1                (inHg)
  case 0237:  // baro correction #2                (inHg)
    if ((data & SSM == NCD) || (data & SSM == TST)) break;
    if (data & SSM == SSM) sign = -1;
    ulPackedBcdData = (data & m19) >> 10;
    for (int i = 0; i < 19; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.001 * sign * INHG_MBAR;

  case 0242:  // total pressure                    (mbar)
    if (data & SSM != SSM) break;
    if (data & (1<<28)) sign = -1;
    return ((data & m18) >> 10) * 1024.0/(1<<17) * sign;

  case 0244:  // norm angle of attack              (ratio)
    if (data & SSM != SSM) break;
    if (data & (1<<28)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<17) * sign;

  default:
    break;
  }
  return _nanf;
}
