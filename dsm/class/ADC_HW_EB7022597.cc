/*
******************************************************************
Copyright by the National Center for Atmospheric Research

$LastChangedDate: $

$LastChangedRevision$

$LastChangedBy$

$HeadURL: $

******************************************************************
*/
#include <ADC_HW_EB7022597.h>

using namespace dsm;

CREATOR_ENTRY_POINT(ADC_HW_EB7022597);

float ADC_HW_EB7022597::processLabel(const unsigned long data)
  throw(atdUtil::IOException)
{
  unsigned long ulPackedBcdData;
  unsigned long ulBinaryData = 0L;
  unsigned long ulPlaceValue = 1L;

//err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );

  // check the SSM...
  if (data & 0x60000000 != 0x60000000)
    return _nanf;

  switch (data & 0xff) {

  case 0203:  // pressure alt
  case 0204:  // baro corrected alt #1
  case 0220:  // baro corrected alt #2
  case 0252:  // baro corrected alt #4
    return ((data & m18) >> 10) * 1.0/(1<<2);

  case 0205:  // mach
    return ((data & m18) >> 10) * 0.001/(1<<6);

  case 0206:  // computed air speed
  case 0207:  // maximum operating speed
    return ((data & m18) >> 10) * 1.0/(1<<8);

  case 0210:  // true air speed
  case 0242:  // total pressure
    return ((data & m18) >> 10) * 1.0/(1<<7);

  case 0211:  // total air pressure
  case 0213:  // static air pressure
  case 0215:  // impact pressure
    return ((data & m18) >> 10) * 1.0/(1<<9);

  case 0212:  // altitude rate
    return ((data & m18) >> 10) * 1.0/(2<<3);

  case 0217:  // static pressure
    return ((data & m18) >> 10) * 1.0/(1<<12);

  case 0234:  // baro correction #1 mB
  case 0236:  // baro correction #2 mB
    ulPackedBcdData = (data & m19) >> 10;
    for (int i = 0; i < 19; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.1;

  case 0235:  // baro correction #1 inHg
  case 0237:  // baro correction #2 inHg
    ulPackedBcdData = (data & m19) >> 10;
    for (int i = 0; i < 19; i += 4) {
      ulBinaryData += (ulPlaceValue * (ulPackedBcdData & 0xf));
      ulPlaceValue *= 10L;
      ulPackedBcdData >>= 4;
    }
    return ulBinaryData * 0.001;

  case 0244:  // norm angle of attack
    return ((data & m18) >> 10) * 1.0/(1<<17);

  default:
    break;
  }
  return _nanf;
}
