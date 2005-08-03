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

  case 0203:  // pressure alt
  case 0204:  // baro corrected alt #1
  case 0220:  // baro corrected alt #2
  case 0252:  // baro corrected alt #4
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<2) * sign;

  case 0205:  // mach
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 0.001/(1<<6) * sign;

  case 0206:  // computed air speed
  case 0207:  // maximum operating speed
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<8) * sign;

  case 0210:  // true air speed
  case 0242:  // total pressure
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<7) * sign;

  case 0211:  // total air pressure
  case 0213:  // static air pressure
  case 0215:  // impact pressure
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<9) * sign;

  case 0212:  // altitude rate
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<3) * sign;

  case 0217:  // static pressure
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<12) * sign;

  case 0234:  // baro correction #1 mB
  case 0236:  // baro correction #2 mB
    if ((data & SSM == NCD) || (data & SSM == TST)) break;
    if (data & SSM == SSM) sign = -1;
    ulPackedBcdData = (data & m19) >> 10;
    for (int i = 0; i < 19; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1 * sign;

  case 0235:  // baro correction #1 inHg
  case 0237:  // baro correction #2 inHg
    if ((data & SSM == NCD) || (data & SSM == TST)) break;
    if (data & SSM == SSM) sign = -1;
    ulPackedBcdData = (data & m19) >> 10;
    for (int i = 0; i < 19; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.001 * sign;

  case 0244:  // norm angle of attack
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & m18) >> 10) * 1.0/(1<<17) * sign;

  default:
    break;
  }
  return _nanf;
}
