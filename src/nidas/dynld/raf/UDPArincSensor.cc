// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2008, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "UDPArincSensor.h"
#include "DSMArincSensor.h"

#include <nidas/util/Logger.h>

#include <csignal>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;
using namespace nidas::dynld::raf;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION_NS(raf, UDPArincSensor);

const n_u::EndianConverter * UDPArincSensor::bigEndian =
    n_u::EndianConverter::getConverter(n_u::EndianConverter::
                                       EC_BIG_ENDIAN);


UDPArincSensor::UDPArincSensor() : _badStatusCnt(0), _ctrl_pid(0), _arincSensors()
{
}

UDPArincSensor::~UDPArincSensor()
{
    close();
    /*
     * Since these sensors do not get added to _allSensors in SensorHandler
     * remove them here.  This could be generalized in SensorHandler for sensors
     * that are not opened.
     */
    std::map<int, DSMArincSensor*>::iterator it;
    for (it = _arincSensors.begin(); it != _arincSensors.end(); ++it)
        delete it->second;
}


void UDPArincSensor::open(int flags)
        throw(n_u::IOException,n_u::InvalidParameterException)
{
    UDPSocketSensor::open(flags);

    _ctrl_pid = fork();

    if (_ctrl_pid == -1)
    {
        ELOG(("UDPArincSensor: error forking errorno = %d", errno));
    }
    else
    if (_ctrl_pid == 0)
    {
        execlp("arinc_ctrl", "arinc_ctrl", "-i", "192.168.84.17", (char *)0);
    }
}

void UDPArincSensor::close()
        throw(n_u::IOException)
{
    UDPSocketSensor::close();

    if (_ctrl_pid > 0)
    {
        int rc = kill(_ctrl_pid, SIGTERM);
        wait(&rc);
    }
    _ctrl_pid = 0;
}

bool UDPArincSensor::process(const Sample * samp,
                           list < const Sample * >&results) throw()
{
    const unsigned char *input = (const unsigned char *)samp->getConstVoidDataPtr();
    const APMP_hdr *hSamp = (const APMP_hdr *)input;

    // absolute time at 00:00 GMT of day.
    dsm_time_t tt = samp->getTimeTag();

    if (bigEndian->uint32Value(hSamp->alta) != 0x414c5441)
    {
      DLOG(("bad magic cookie 0x%08x, should be 0x414c5441\n", bigEndian->uint32Value(hSamp->alta)));
      return false;
    }

    if (bigEndian->uint32Value(hSamp->mode) != 1 || (bigEndian->uint32Value(hSamp->status) & 0xFFFF) != 0)
    {
      _badStatusCnt++;
      DLOG(("bad packet received mode = %d, status = %u\n",
        bigEndian->uint32Value(hSamp->mode), bigEndian->uint32Value(hSamp->status) & 0xffff));
      return false;
    }


    int payloadSize = bigEndian->uint32Value(hSamp->payloadSize);
    int nFields = (payloadSize - 16) / sizeof(rxp);
    unsigned long long PE = bigEndian->uint32Value(hSamp->PEtimeHigh);
    PE = ((PE << 32) | bigEndian->uint32Value(hSamp->PEtimeLow)) / 50;  // microseconds

    uint32_t startTime = (decodeIRIG((unsigned char *)&hSamp->IRIGtimeLow) * 1000) + 1000;

    DLOG(( "nFields=%3u seqNum=%u, pSize=%u - PE %llu IRIG julianDay=%x %s", nFields,
                bigEndian->uint32Value(hSamp->seqNum),
                payloadSize, PE,
                bigEndian->uint32Value(hSamp->IRIGtimeHigh), irigHHMMSS ));


    int nOutFields[8];          // Four possible devices.
    unsigned char *outData[8];  // Four possible devices.
    for (int i = 0; i < 8; i++) {
        nOutFields[i] = 0;
        outData[i] = new unsigned char [payloadSize]; // oversized...
    }

    const rxp *pSamp = (const rxp *) (input + sizeof(APMP_hdr));
    for (int i = 0; i < nFields; i++) {
        int channel = (bigEndian->uint32Value(pSamp[i].control) & 0x0F000000) >> 24;
        if (channel < 8)
        {
            txp packet;
//            packet.control = bigEndian->uint32Value(pSamp[i].control);
//            packet.timeHigh = bigEndian->uint32Value(pSamp[i].timeHigh);
            packet.time = startTime + ((decodeTIMER(pSamp[i]) - PE) / 1000);   // milliseconds since midnight...
//DLOG((" UAS: %lu = %lu + (%llu - %llu) %llu", packet.time, startTime, decodeTIMER(pSamp[i]), PE, ((decodeTIMER(pSamp[i]) - PE) / 1000) ));
            packet.data = bigEndian->uint32Value(pSamp[i].data);
            memcpy(&outData[channel][nOutFields[channel]++ * sizeof(txp)], &packet, sizeof(txp));
        }
        else
            ELOG(( "%s: received channel number %d, outside 0-3, ignoring.", getName().c_str(), channel ));
    }

    for (int i = 0; i < 8; i++) {
        if (nOutFields[i] > 0)
            _arincSensors[i]->processAlta(tt, outData[i], nOutFields[i], results);

        delete [] outData[i];
    }

    return true;
}

/* -------------------------------------------------------------------- */
unsigned long UDPArincSensor::decodeIRIG(unsigned char *irig_bcd)
{
  int h, m, s;

  h = bcd_to_decimal(irig_bcd[1]);
  m = bcd_to_decimal(irig_bcd[2]);
  s = bcd_to_decimal(irig_bcd[3]);

  sprintf(irigHHMMSS, "%02d:%02d:%02d", h, m, s);
  return h * 3600 + m * 60 + s;
}

/* -------------------------------------------------------------------- */
unsigned long long UDPArincSensor::decodeTIMER(const rxp& samp)
{
  unsigned long long ttime;

  /* Make 64-bit 20nsec/50Mhz Clock Ticks to 64-bit uSecs */
  ttime = bigEndian->uint32Value(samp.timeHigh);
  ttime = ((ttime << 32) | bigEndian->uint32Value(samp.timeLow)) / 50;

#ifdef DEBUG
  static unsigned long long prevTime = 0;
  printf("  rxp irig %llu usec, dT=%lld\n", ttime, ttime - prevTime);
  printf("           %llu msec, %llu sec\n", (ttime/1000), (ttime/1000000));
  prevTime = ttime;
#endif

  return ttime; // return microseconds
}

