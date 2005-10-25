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

CREATOR_FUNCTION(GPS_HW_HG2021GB02);

float GPS_HW_HG2021GB02::processLabel(const long data)
{
//err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );

  switch (data & 0xff) {
  case 0061:  // BNR - Pseudo Range                (m)
    if ((data & SSM) != SSM) break;
    if (data & (1<<28)) Pseudo_Range_sign = -1;
    else                Pseudo_Range_sign = 1;
    return (data<<3>>11) * 256.0;

  case 0062:  // BNR - Pseudo Range Fine           (m)
    if ((data & SSM) != SSM) break;
    return (data<<3>>20) * 0.125 * Pseudo_Range_sign;

  case 0063:  // BNR - Range Rate                  (m/s)
    if ((data & SSM) != NCD) break;
    return (data<<3>>11) * 3.90625e-3;

  case 0064:  // BNR - Delta Range                 (m)
    if ((data & SSM) != SSM) break;
    return (data<<3>>11) * 3.90625e-3;

  case 0065:  // BNR - SV Position X               (m)
    if ((data & SSM) != SSM) break;
    if (data & (1<<28)) SV_Position_X_sign = -1;
    else                SV_Position_X_sign = 1;
    return (data<<3>>11) * 64.0;

  case 0066:  // BNR - SV Position X Fine          (m)
    if ((data & SSM) != SSM) break;
    return (data<<3>>17) * 3.90625e-3 * SV_Position_X_sign;

  case 0070:  // BNR - SV Position Y               (m)
    if ((data & SSM) != SSM) break;
    if (data & (1<<28)) SV_Position_Y_sign = -1;
    else                SV_Position_Y_sign = 1;
    return (data<<3>>11) * 64.0;

  case 0071:  // BNR - SV Position Y Fine          (m)
    if ((data & SSM) != SSM) break;
    return (data<<3>>17) * 3.90625e-3 * SV_Position_Y_sign;

  case 0072:  // BNR - SV Position Z               (m)
    if ((data & SSM) != SSM) break;
    if (data & (1<<28)) SV_Position_Z_sign = -1;
    else                SV_Position_Z_sign = 1;
    return (data<<3>>11) * 64.0;

  case 0073:  // BNR - SV Position Z Fine          (m)
    if ((data & SSM) != SSM) break;
    return (data<<3>>17) * 3.90625e-3 * SV_Position_Z_sign;

  case 0074:  // BNR - UTC Measure Time            (s)
    if ((data & SSM) != SSM) break;
    return (data<<4>>12) * 9.5367431640625e-6; // no sign

  case 0076:  // BNR - GPS Altitude (MSL)          (ft)
    if ((data & SSM) != SSM) break;
    return (data<<3>>11) * 0.125 * FT_MTR;

  case 0101:  // BNR - Horz Dilution of Precision  ()
  case 0102:  // BNR - Vert Dilution of Precision  ()
    if ((data & SSM) != SSM) break;
    return (data<<4>>17) * 3.125e-2; // no sign

  case 0103:  // BNR - GPS Track Angle             (deg)
    if ((data & SSM) != SSM) break;  // NCD when GPS Ground Speed < 7 knots
    return (data<<3>>16) * 5.4931640625e-3; // 180.0/(1<<15)

  case 0110:  // BNR - GPS Latitude                (deg)
    if ((data & SSM) != SSM) break;
    if (data & (1<<28)) GPS_Latitude_sign = -1;
    else                GPS_Latitude_sign = 1;
    return (data<<3>>11) * 1.71661376953125e-4; // 180.0/(1<<20)

  case 0111:  // BNR - GPS Longitude               (deg)
    if ((data & SSM) != SSM) break;
    if (data & (1<<28)) GPS_Longitude_sign = -1;
    else                GPS_Longitude_sign = 1;
    return (data<<3>>11) * 1.71661376953125e-4; // 180.0/(1<<20)

  case 0112:  // BNR - GPS Ground Speed            (knot)
    if ((data & SSM) != SSM) break;
    return (data<<4>>17) * 0.125 * KTS_MS;  // no sign

  case 0120:  // BNR - GPS Lat Fine                (deg)
    if ((data & SSM) != SSM) break;
    return (data<<3>>20) * 8.381903171539306640625e-8 * GPS_Latitude_sign; // 180.0/(1<<31)

  case 0121:  // BNR - GPS Lon Fine                (deg)
    if ((data & SSM) != SSM) break;
    return (data<<3>>20) * 8.381903171539306640625e-8 * GPS_Longitude_sign; // 180.0/(1<<31)

  case 0125:  // BCD - UTC (Universal Time Code)   (hr:mn)
    if ((data & SSM) != SSM) break;
    return (
            ((data & (0x3<<26)) >> 26) *  10.0 +
            ((data & (0xf<<22)) >> 22) *  1.0 +
            (
             ((data & (0xf<<18)) >> 18) * 10.0 +
             ((data & (0xf<<14)) >> 14) * 1.0 +
             ((data & (0xf<<10)) >> 10) * 0.1
             ) / 60.0
            ) * 3600.0; // no sign

  case 0130:  // BNR - Aut Horz Integrity Limit    (nm)
    if ((data & SSM) != SSM) break;
    return (data<<4>>15) * 1.220703125e-4;  // no sign (bit 11 is a detection bit)

  case 0133:  // BNR - Aut Vert Integrity Limit    (ft)
    if ((data & SSM) != SSM) break;
    return (data<<4>>14) * 0.125 * FT_MTR;  // no sign

  case 0135:  // BNR - Approach Area VIL           (ft)
    if ((data & SSM) != SSM) break;
    return (data<<4>>15) * 0.25 * FT_MTR;  // no sign

//case 0136:  // BNR - Vert Figure of Merit        (ft)  (for HG2021GD01 GNSSU)
//  if ((data & SSM) != SSM) break;
//  return (data<<4>>14) * 0.125 * FT_MTR;  // no sign

  case 0136:  // BNR - Vert Figure of Merit        (m)  (for HG2021GB01 GNSSU)
    if ((data & SSM) != SSM) break;
    return (data<<4>>17) * 3.125e-2;  // no sign

  case 0140:  // BNR - UTC Fine                    (sec)
    if ((data & SSM) != SSM) break;
    return (data<<4>>12) * 9.5367431640625e-7; // no sign

  case 0141:  // BNR - UTC Fine Fractions          (sec)
    if ((data & SSM) != SSM) break;
    return (data<<4>>22) * 9.31322574615478515625e-10; // no sign

  case 0143:  // BNR - Approach Area HIL           (nm)
    if ((data & SSM) != SSM) break;
    return (data<<4>>15) * 1.220703125e-4; // no sign

  case 0150:  // BCD - Universal Time Code         (hr:mn:sc)
    // 32|31 30|29|28 27 26 25 24|23 22 21 20 19 18|17 16 15 14 13 12|11|10  9| 8  7  6  5  4  3  2  1
    // --+-----+--+--------------+-----------------+-----------------+--+-----+-----------------------
    // P | SSM |0 |     hours    |     minutes     |     seconds     |1 | SDI |      8-bit label      
    if ((data & SSM) != SSM) break;
    return ((data & (0x1f<<23)) >> 23) * 60 * 60 +
           ((data & (0x3f<<17)) >> 17) * 60 +
           ((data & (0x3f<<11)) >> 11); // no sign

  case 0162:  // BCD - Dest Wpt ETA                (hr:mn)
  case 0163:  // BCD - Altr Wpt ETA                (hr:mn)
    // 32|31 30|29|28 27 26 25 24|23 22 21 20 19 18|17|16|15 14|13|12|11|10  9| 8  7  6  5  4  3  2  1
    // --+-----+--+--------------+-----------------+--+--+-----+--+--+--+-----+-----------------------
    // P | SSM |0 |     hours    |     minutes     |d?|er| fms |rb|ia|bc| SDI |      8-bit label      
    if ((data & SSM) != SSM) break;
    if (data & (1<<17)) break;  // set if GNSSU has not determined times.
    return ((data & (0x1f<<23)) >> 23) * 60 * 60 +
           ((data & (0x3f<<17)) >> 17) * 60; // no sign

  case 0165:  // BNR - Vertical Velocity           (ft/min)
    if ((data & SSM) != SSM) break;
    return (data<<3>>16) * 1.0 * FPM_MPS;

  case 0166:  // BNR - N/S Velocity                (knot)
  case 0174:  // BNR - E/W Velocity                (knot)
    if ((data & SSM) != SSM) break;
    return (data<<3>>16) * 0.125 * KTS_MS;

//case 0247:  // BNR - Horz Figure of Merit        (nm)  (for HG2021GD01 GNSSU)
//  if ((data & SSM) != SSM) break;
//  return (data<<4>>14) * 6.103515625e-5;  // no sign

  case 0247:  // BNR - Horz Figure of Merit        (m)  (for HG2021GB01 GNSSU)
    if ((data & SSM) != SSM) break;
    return (data<<4>>17) * 3.125e-2;  // no sign

  case 0260:  // BCD - UTC Date                    (dy:mn:yr)
    break;
    // 32|31 30|29 28|27 26 25 24|23|22 21 20 19|18 17 16 15|14 13 12 11|10  9| 8  7  6  5  4  3  2  1
    // --+-----+-----+-----------+--+-----------+-----------+-----------+-----+-----------------------
    // P | SSM | 10d |   1 day   |10|   1 mon   |  10 years |   1 year  | SDI |      8-bit label      
// #include <time.h>
//     if ((data & SSM) != SSM) break;
//     struct tm tm;
//     unsigned long time = data;
//     tm.tm_mday = (time<< 3>>30)*10 + (time<< 5>>28);
//     tm.tm_mon  = (time<< 9>>31)*10 + (time<<10>>28);
//     tm.tm_year = (time<<14>>28)*10 + (time<<18>>28);
//     cout << "   tm.tm_mday: " << tm.tm_mday <<
//             "   tm.tm_mon:  " << tm.tm_mon  <<
//             "   tm.tm_year: " << tm.tm_year << endl;
//     return mktime(&tm);  // TODO - how should this value be returned?

  case 0343:  // BNR - Dest Wpt HIL                (nm)
  case 0347:  // BNR - Altr Wpt HIL                (nm)
    // 32|31 30|29|28 27 26 25 24 23 22 21 20 19 18|17|16|15 14|13 12 11|10  9| 8  7  6  5  4  3  2  1
    // --+-----+--+--------------------------------+--+--+-----+--------+-----+-----------------------
    // P | SSM |0 |        integrity limit         |pr|sq| fms |  ISC   | SDI |      8-bit label      
    if ((data & SSM) != 0)      break; // Normal Operation
    if (data & 0x00001c00 == 0) break; // Integrity Sequence Number
    return (data<<4>>21) * 7.8125e-3; // no sign

  case 0354:  // BNR - Counter                     (sec)
    return (data<<4>>14) * 1.0;  // no sign

  case 0060:  // DIS - Measurement Status          ()
  case 0156:  // DIS - Maintenance                 ()
  case 0157:  // DIS - Maintenance                 ()
  case 0273:  // DIS - GPS Sensor Status           ()
  case 0352:  // DIS - Maintenance                 ()
  case 0355:  // DIS - Maintenance                 ()
  case 0377:  // DIS - equipment id                ()
  default:
    break;
  }
  return _nanf;
}
