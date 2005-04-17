/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: $

    Taken from the Honeywell Installation Manual PN HG2021GB02/GD02
    Table 207 "GNSSU ARINC 429 Output Data" (GPS)  (pages 217-219).

 ******************************************************************
*/

#include <GPS_HW_HG2021GB02.h>

using namespace std;
using namespace dsm;

CREATOR_ENTRY_POINT(GPS_HW_HG2021GB02);

/**
 * Since each sample contains it's own time tag then the block sample's time tag
 * (obtained from samp->getTimeTag()) is useless.
 */
bool GPS_HW_HG2021GB02::process(const Sample* samp,list<const Sample*>& results)
  throw()
{
  const tt_data_t *pSamp = (const tt_data_t*) samp->getConstVoidDataPtr();
  int nfields = samp->getDataByteLength() / sizeof(tt_data_t);

  // time at 00:00 GMT of day.
  dsm_time_t t0day = samp->getTimeTag() -
    	(samp->getTimeTag() % MSECS_PER_DAY);

  for (int i=0; i<nfields; i++) {

//     if (i == nfields-1)
//       err("sample[%3d]: %8lu %4o 0x%08lx", i, pSamp[i].time,
//           (int)(pSamp[i].data & 0xff), (pSamp[i].data & (unsigned long)0xffffff00) );

    SampleT<float>* outs = getSample<float>(1);
    // pSamp[i].time is the number of milliseconds since midnight
    // for the individual label. Use it to create a correct
    // time tag for the label.
    outs->setTimeTag(t0day + pSamp[i].time);

    unsigned short label = pSamp[i].data && 0xff;

    // set the sample id to sum of sensor id and label
    outs->setShortId(getId() + label);
    outs->setDataLength(1);
    float* d = outs->getDataPtr();

    // check the SSM...
    if (pSamp[i].data & 0x60000000 == 0x60000000)

      switch (pSamp[i].data & 0xff) {
      case 0065:  // SV Position X               (m)
      case 0070:  // SV Position Y               (m)
      case 0072:  // SV Position Z               (m)
      case 0110:  // GPS Latitude                (deg)
      case 0111:  // GPS Longitude               (deg)
        processLabel(pSamp[i].data);  // partial process
        break;
      default:
        d[0] = processLabel(pSamp[i].data);
        break;
      }
    else
      d[0] = _nanf;

    results.push_back(outs);
  }

  return true;
}

float GPS_HW_HG2021GB02::processLabel(const unsigned long data)
  throw(atdUtil::IOException)
{
//err("%4o 0x%08lx", (int)(data & 0xff), (data & (unsigned long)0xffffff00) );

  switch (data & 0xff) {
  case 0060:  // Measurement Status          ()
    break; // DIS

  case 0061:  // Pseudo Range                (m)
    return ((data & 0x0fffff00) >> 8) * 256.0;

  case 0062:  // Pseudo Range Fine           (m)
    return ((data & m11) >> 10) * 0.125;

  case 0063:  // Range Rate                  (m/s)
  case 0064:  // Delta Range                 (m)
    return ((data & 0x0fffff00) >> 8) * 1.0/(1<<8);

  case 0065:  // SV Position X               (m)
  case 0070:  // SV Position Y               (m)
  case 0072:  // SV Position Z               (m)
    return ((data & 0x0fffff00) >> 8) * 64.0;

  case 0066:  // SV Position X Fine          (m)
  case 0071:  // SV Position Y Fine          (m)
  case 0073:  // SV Position Z Fine          (m)
    return ((data & m14) >> 10) * 1.0/(1<<8);

  case 0074:  // UTC Measure Time            (s)
    return ((data & 0x0fffff00) >> 8) * 0.000009536743;

  case 0076:  // GPS Altitude (MSL)          (ft)
    return ((data & 0x0fffff00) >> 8) * 0.125;

  case 0101:  // Horz Dilution of Precision  ()
  case 0102:  // Vert Dilution of Precision  ()
    break;

  case 0103:  // GPS Track Angle             (deg)
    return ((data & m15) >> 10) * 0.125;

  case 0110:  // GPS Latitude                (deg)
  case 0111:  // GPS Longitude               (deg)
  case 0112:  // GPS Ground Speed            (knot)
  case 0120:  // GPS Lat Fine                (deg)
  case 0121:  // GPS Lon Fine                (deg)
    return ((data & 0x0ffffc00) >> 10) * 0.0;

  case 0125:  // Universal Time Code         (hr:mn)
    break;

  case 0130:  // Aut Horz Integrity Limit    (nm)
  case 0133:  // Aut Vert Integrity Limit    (ft)
  case 0135:  // Approach Area VIL           (ft)
  case 0136:  // Vert Figure of Merit        (ft)
  case 0140:  // UTC Fine                    (sec)
  case 0141:  // UTC Fine Fractions          (sec)
  case 0143:  // Approach Area HIL           (nm)
    return ((data & 0x0ffffc00) >> 10) * 0.0;

  case 0150:  // Universal Time Code         (hr:mn:sc)
    break;

  case 0156:  // Maintenance                 ()
  case 0157:  // Maintenance                 ()
    break; // DIS

  case 0162:  // Dest Wpt ETA                (hr:mn)
  case 0163:  // Altr Wpt ETA                (hr:mn)
    break;

  case 0165:  // Vertical Velocity           (ft/min)
  case 0166:  // N/S Velocity                (knot)
  case 0174:  // E/W Velocity                (knot)
  case 0247:  // Horz Figure of Merit        (nm)
    return ((data & 0x0ffffc00) >> 10) * 0.0;

  case 0260:  // UTC Date                    (dy:mn:yr)
    break;

  case 0273:  // GPS Sensor Status           ()
    break; // DIS

  case 0343:  // Dest Wpt HIL                (nm)
  case 0347:  // Altr Wpt HIL                (nm)
    if ((data & 0x00001c00) != 0) /* Integrity Seq Number */
      return ((data & 0x0ffe0000) >> 17) * 0.0078;

  case 0352:  // Maintenance                 ()
    break; // DIS

  case 0354:  // Counter                     (sec)
    return ((data & 0x0ffffc00) >> 10) * 1.0;

  case 0355:  // Maintenance                 ()
  case 0377:  // equipment id                ()
    break; // DIS

  default:
    break;
  }
  return _nanf;
}
