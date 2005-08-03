/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    Taken from the Honeywell Installation Manual PN HG2021GB02/GD02
    Table 207 "GNSSU ARINC 429 Output Data" (GPS)  (pages 217-219).

 ******************************************************************
*/
#include <GPS_HW_HG2021GB02.h>

using namespace dsm;

CREATOR_ENTRY_POINT(GPS_HW_HG2021GB02);

float GPS_HW_HG2021GB02::processLabel(const unsigned long data)
{
  static int Pseudo_Range_sign = 1;
  static int SV_Position_X_sign = 1;
  static int SV_Position_Y_sign = 1;
  static int SV_Position_Z_sign = 1;
  static int GPS_Latitude_sign = 1;
  static int GPS_Longitude_sign = 1;

  unsigned long ulPackedBcdData;
  unsigned long ulBinaryData = 0L;
  unsigned long ulPlaceValue = 1L;
  int sign = 1;

//err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );

  switch (data & 0xff) {
  case 0060:  // DIS - Measurement Status          ()
    break;

  case 0061:  // BNR - Pseudo Range                (m)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) Pseudo_Range_sign = -1;
    else                Pseudo_Range_sign = 1;
    return ((data & 0x0fffff00) >> 8) * 256.0 * Pseudo_Range_sign;

  case 0062:  // BNR - Pseudo Range Fine           (m)
    if (data & SSM != SSM) break;
    return ((data & m11) >> 10) * 0.125 * Pseudo_Range_sign;

  case 0063:  // BNR - Range Rate                  (m/s)
    if (data & SSM != NCD) break;
    if (data & (1<<29)) sign = -1;
    return ((data & 0x0fffff00) >> 8) * 1.0/(1<<8) * sign;

  case 0064:  // BNR - Delta Range                 (m)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & 0x0fffff00) >> 8) * 1.0/(1<<8) * sign;

  case 0065:  // BNR - SV Position X               (m)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) SV_Position_X_sign = -1;
    else                SV_Position_X_sign = 1;
    return ((data & 0x0fffff00) >> 8) * 64.0 * SV_Position_X_sign;

  case 0066:  // BNR - SV Position X Fine          (m)
    if (data & SSM != SSM) break;
    return ((data & m14) >> 10) * 1.0/(1<<8) * SV_Position_X_sign;

  case 0070:  // BNR - SV Position Y               (m)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) SV_Position_Y_sign = -1;
    else                SV_Position_Y_sign = 1;
    return ((data & 0x0fffff00) >> 8) * 64.0 * SV_Position_Y_sign;

  case 0071:  // BNR - SV Position Y Fine          (m)
    if (data & SSM != SSM) break;
    return ((data & m14) >> 10) * 1.0/(1<<8) * SV_Position_Y_sign;

  case 0072:  // BNR - SV Position Z               (m)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) SV_Position_Z_sign = -1;
    else                SV_Position_Z_sign = 1;
    return ((data & 0x0fffff00) >> 8) * 64.0 * SV_Position_Z_sign;

  case 0073:  // BNR - SV Position Z Fine          (m)
    if (data & SSM != SSM) break;
    return ((data & m14) >> 10) * 1.0/(1<<8) * SV_Position_Z_sign;

  case 0074:  // BNR - UTC Measure Time            (s)
    if (data & SSM != SSM) break;
    return ((data & 0x0fffff00) >> 8) * 10.0/(1<<20); // no sign

  case 0076:  // BNR - GPS Altitude (MSL)          (ft)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) sign = -1;
    return ((data & 0x0fffff00) >> 8) * 0.125 * sign;

  case 0101:  // BNR - Horz Dilution of Precision  ()
  case 0102:  // BNR - Vert Dilution of Precision  ()
    if (data & SSM != SSM) break;
    return ((data & m15) >> 10) * 1.0/(1<<5); // no sign

  case 0103:  // BNR - GPS Track Angle             (deg)
    if (data & SSM != SSM) break;  // NCD when GPS Ground Speed < 7 knots
    if (data & (1<<26)) sign = -1;
    return ((data & m15) >> 10) * 45.0/(1<<13) * sign;

  case 0110:  // BNR - GPS Latitude                (deg)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) GPS_Latitude_sign = -1;
    else                GPS_Latitude_sign = 1;
    return ((data & 0x0fffff00) >> 10) * 45.0/(1<<18) * GPS_Latitude_sign;

  case 0111:  // BNR - GPS Longitude               (deg)
    if (data & SSM != SSM) break;
    if (data & (1<<29)) GPS_Longitude_sign = -1;
    else                GPS_Longitude_sign = 1;
    return ((data & 0x0fffff00) >> 10) * 45.0/(1<<18) * GPS_Longitude_sign;

  case 0112:  // BNR - GPS Ground Speed            (knot)
    if (data & SSM != SSM) break;
    return ((data & m15) >> 10) * 0.125;  // no sign

  case 0120:  // BNR - GPS Lat Fine                (deg)
    if (data & SSM != SSM) break;
    return ((data & m11) >> 10) * 45.0/(1<<29) * GPS_Latitude_sign;

  case 0121:  // BNR - GPS Lon Fine                (deg)
    if (data & SSM != SSM) break;
    return ((data & m11) >> 10) * 45.0/(1<<29) * GPS_Longitude_sign;

  case 0125:  // BNR - UTC (Universal Time Code)   (hr:mn)
    err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );
    break; // no sign

  case 0130:  // BNR - Aut Horz Integrity Limit    (nm)
    if (data & SSM != SSM) break;
    return ((data & (m17<<1)) >> 11) * 1.0/(1<<13);  // no sign (bit 11 is a detection bit)

  case 0133:  // BNR - Aut Vert Integrity Limit    (ft)
    if (data & SSM != SSM) break;
    return ((data & m18) >> 10) * 0.125;  // no sign

  case 0135:  // BNR - Approach Area VIL           (ft)
    if (data & SSM != SSM) break;
    return ((data & m17) >> 10) * 0.25;  // no sign

//   case 0136:  // BNR - Vert Figure of Merit        (ft)  (for HG2021GD01 GNSSU)
//     if (data & SSM != SSM) break;
//     return ((data & m18) >> 10) * 0.125;  // no sign

  case 0136:  // BNR - Vert Figure of Merit        (m)  (for HG2021GB01 GNSSU)
    err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );
    if (data & SSM != SSM) break;
    return ((data & m15) >> 10) * 1.0/(1<<5);  // no sign

  case 0140:  // BNR - UTC Fine                    (sec)
    if (data & SSM != SSM) break;
    return ((data & 0x0fffff00) >> 8) * 1.0/(1<<20); // no sign

  case 0141:  // BNR - UTC Fine Fractions          (sec)
    if (data & SSM != SSM) break;
    return ((data & m10) >> 10) * 1.0/(1<<30); // no sign

  case 0143:  // BNR - Approach Area HIL           (nm)
    if (data & SSM != SSM) break;
    return ((data & m17) >> 10) * 1.0/(1<<13);

  case 0150:  // BCD - Universal Time Code         (hr:mn:sc)
    // 32|31 30|29|28 27 26 25 24|23 22 21 20 19 18|17 16 15 14 13 12|11|10  9| 8  7  6  5  4  3  2  1
    // --+-----+--+--------------+-----------------+-----------------+--+-----+-----------------------
    // P | SSM |0 |     hours    |     minutes     |     seconds     |1 | SDI |      8-bit label      
    if (data & SSM != SSM) break;
    return ((data & 0x0f800000) >> 23) * 60 * 60 +
           ((data & 0x007e0000) >> 17) * 60 +
           ((data & 0x0001f800) >> 11); // no sign

  case 0156:  // DIS - Maintenance                 ()
  case 0157:  // DIS - Maintenance                 ()
    break;

  case 0162:  // BCD - Dest Wpt ETA                (hr:mn)
  case 0163:  // BCD - Altr Wpt ETA                (hr:mn)
    // 32|31 30|29|28 27 26 25 24|23 22 21 20 19 18|17|16|15 14|13|12|11|10  9| 8  7  6  5  4  3  2  1
    // --+-----+--+--------------+-----------------+--+--+-----+--+--+--+-----+-----------------------
    // P | SSM |0 |     hours    |     minutes     |d?|er| fms |rb|ia|bc| SDI |      8-bit label      
    if (data & SSM != SSM) break;
    if (data & (1<<17)) break;  // set if GNSSU has not determined times.
    return ((data & 0x007e0000) >> 17) * 60 +
           ((data & 0x0001f800) >> 11); // no sign

  case 0165:  // BNR - Vertical Velocity           (ft/min)
    if (data & SSM != SSM) break;
    if (data & (1<<26)) sign = -1;
    return ((data & m15) >> 10) * 0.125 * sign;

  case 0166:  // BNR - N/S Velocity                (knot)
  case 0174:  // BNR - E/W Velocity                (knot)
    if (data & SSM != SSM) break;
    if (data & (1<<26)) sign = -1;
    return ((data & m15) >> 10) * 1.0 * sign;

//   case 0247:  // BNR - Horz Figure of Merit        (nm)  (for HG2021GD01 GNSSU)
//     if (data & SSM != SSM) break;
//     return ((data & m18) >> 10) * 1.0/(1<<14);  // no sign

  case 0247:  // BNR - Horz Figure of Merit        (m)  (for HG2021GB01 GNSSU)
    err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );
    if (data & SSM != SSM) break;
    return ((data & m15) >> 10) * 1.0/(1<<5);  // no sign

  case 0260:  // BCD - UTC Date                    (dy:mn:yr)
    err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );
    break; // no sign

  case 0273:  // DIS - GPS Sensor Status           ()
    break;

  case 0343:  // BNR - Dest Wpt HIL                (nm)
  case 0347:  // BNR - Altr Wpt HIL                (nm)
    // 32|31 30|29|28 27 26 25 24 23 22 21 20 19 18|17|16|15 14|13 12 11|10  9| 8  7  6  5  4  3  2  1
    // --+-----+--+--------------------------------+--+--+-----+--------+-----+-----------------------
    // P | SSM |0 |        integrity limit         |pr|sq| fms |  ISC   | SDI |      8-bit label      
    if (data & SSM != 0)        break; // Normal Operation
    if (data & 0x00001c00 == 0) break; // Integrity Sequence Number
    return ((data & 0x0ffe0000) >> 17) * 1.0/(1<<7); // no sign

  case 0352:  // DIS - Maintenance                 ()
    break;

  case 0354:  // BNR - Counter                     (sec)
    return ((data & m18) >> 10) * 1.0;  // no sign

  case 0355:  // BNR - Maintenance                 ()
  case 0377:  // BNR - equipment id                ()
    break;

  default:
    break;
  }
  return _nanf;
}
