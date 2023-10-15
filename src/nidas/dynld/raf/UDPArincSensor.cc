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

#include <nidas/util/UTime.h>
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

const int UDPArincSensor::MAX_CHANNELS = 8;


UDPArincSensor::UDPArincSensor() :
    _prevAPMPseqNum(0), _badAPMPseqCnt(0), _badStatusCnt(0), _ipAddr(),
    _statusPort(0), configStatus(), _ctrl_pid(0), _arincSensors()
{
    for (int i = 0; i < MAX_CHANNELS; ++i)
        _prevRXPseqNum[i] = _badRXPseqCnt[i] = 0;
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

    if (_badAPMPseqCnt > 1) // always one for start up.
        cerr << getClassName() << ": Number of APMP sequence count errors = " << _badAPMPseqCnt-1 << std::endl;

    for (int i = 0; i < MAX_CHANNELS; ++i)
        if (_badRXPseqCnt[i] > 1) // always one for start up.
            cerr << getClassName() << ": Number of RXP sequence count errors for channel "
                 << i << " = " << _badRXPseqCnt[i]-1 << std::endl;
}

void UDPArincSensor::validate()
{
    UDPSocketSensor::validate();

    const Parameter *p;
    p = getParameter("ip"); // device IP address.
    if (!p) throw n_u::InvalidParameterException(getName(),
          "IP", "not found");
    _ipAddr = p->getStringValue(0);

    p = getParameter("status_port"); // Status port for ARINCSTATUS
    if (!p) throw n_u::InvalidParameterException(getName(),
          "status_port", "not found");
    _statusPort = (unsigned int)p->getNumericValue(0);

}

void UDPArincSensor::open(int flags)
{
    char *args[20], port[32];
    int argc = 0;

// Do this, and start arinc_ctrl manually outisde of nidas, until get it debugged.
//UDPSocketSensor::open(flags);
//return;

    args[argc++] = (char *)"arinc_ctrl";
    if (_ipAddr.length() > 0) {
            args[argc++] = (char *)"-i";
            args[argc++] = (char *)_ipAddr.c_str();
    }

    std::string dev = getDeviceName();
    size_t pos = dev.find("::");
    if (pos != std::string::npos) {
            args[argc++] = (char *)"-p";
            args[argc++] = &((char *)dev.c_str())[pos+2];
    }

    std::map<int, DSMArincSensor*>::iterator it;
    for (it = _arincSensors.begin(); it != _arincSensors.end(); ++it) {
            std::stringstream sp;
            args[argc++] = (char *)"-s";
            sp << it->first << "," << (it->second)->Speed();
            args[argc] = new char[sp.str().size()+1];
            strcpy(args[argc++], sp.str().c_str());
    }

    if (_statusPort > 0) {
            sprintf(port, "%u", _statusPort);
            args[argc++] = (char *)"-u";
            args[argc++] = (char *)port;
    }
    args[argc] = (char *)0;

    ILOG(("forking command %s", args[0]));
    _ctrl_pid = fork();

    if (_ctrl_pid == -1)
    {
        PLOG(("UDPArincSensor: error forking errno = %d", errno));
    }
    else
    if (_ctrl_pid == 0)
    {
        if (execvp(args[0], args) == -1)
        {
            ELOG(("UDPArincSensor: error executing command: ") << args[0] <<
            ": error " << errno << ": " << n_u::Exception::errnoToString(errno));
            exit(1);
        }
    }
    else
        UDPSocketSensor::open(flags);
}

void UDPArincSensor::close()
{

    if (_ctrl_pid > 0)
    {
        UDPSocketSensor::close();

        if (kill(_ctrl_pid, SIGTERM) == -1)
        {
            ELOG(("UDPArincSensor: kill() error: ") << ": error " << errno
			<< " : " << n_u::Exception::errnoToString(errno));
        }

        if (waitpid(_ctrl_pid, 0, 0) == -1)
        {
            ELOG(("UDPArincSensor: waitpid() error: ") << ": error " << errno
			<< " : " << n_u::Exception::errnoToString(errno));
        }
    }
    _ctrl_pid = 0;
}

bool UDPArincSensor::process(const Sample * samp,
                           list < const Sample * >&results)
{
    const unsigned char *input = (const unsigned char *)samp->getConstVoidDataPtr();
    const APMP_hdr *hSamp = (const APMP_hdr *)input;

    if (strncmp((const char *)input, (const char *)"STATUS", 6) == 0)
    {
        UDPSocketSensor::process(samp, results);

        if (results.empty()) return false;
        return true;
    }

    dsm_time_t tt = samp->getTimeTag();

    if (bigEndian->uint32Value(hSamp->alta) != 0x414c5441)
    {
      WLOG(("%s %s : bad magic cookie 0x%08x, should be 0x414c5441\n", getName().c_str(),
        n_u::UTime(samp->getTimeTag()).format(true,"%H:%M:%S.%3f").c_str(),
        bigEndian->uint32Value(hSamp->alta)));
      return false;
    }

    if (bigEndian->uint32Value(hSamp->mode) != 1 || (bigEndian->uint32Value(hSamp->status) & 0xFFFF) != 0)
    {
      _badStatusCnt++;
      WLOG(("%s %s : bad packet received mode = %d, status = %u\n", getName().c_str(),
        n_u::UTime(samp->getTimeTag()).format(true,"%H:%M:%S.%3f").c_str(),
        bigEndian->uint32Value(hSamp->mode), bigEndian->uint32Value(hSamp->status) & 0xffff));
      return false;
    }


    int payloadSize = bigEndian->uint32Value(hSamp->payloadSize);
    int nFields = (payloadSize - 16) / sizeof(rxp);
    uint32_t seqNum = bigEndian->uint32Value(hSamp->seqNum);
    long long PE = bigEndian->uint32Value(hSamp->PEtimeHigh);
    PE = ((PE << 32) | bigEndian->uint32Value(hSamp->PEtimeLow)) / 50;  // microseconds

    uint32_t startTime = (decodeIRIG((unsigned char *)&hSamp->IRIGtimeLow) * 1000) + 1000;

    DLOG(( "%s : APMP: nFields=%3u seqNum=%u, pSize=%u - PE %lld IRIG julianDay=%x %s",
        n_u::UTime(samp->getTimeTag()).format(true,"%H:%M:%S.%3f").c_str(),
        nFields, seqNum, payloadSize, PE,
        bigEndian->uint32Value(hSamp->IRIGtimeHigh), irigHHMMSS ));

    if (seqNum != _prevAPMPseqNum+1)
        _badAPMPseqCnt++;
    _prevAPMPseqNum = seqNum;

    int nOutFields[MAX_CHANNELS];
    unsigned char *outData[MAX_CHANNELS];
    for (int i = 0; i < MAX_CHANNELS; i++) {
        nOutFields[i] = 0;
        outData[i] = new unsigned char [payloadSize]; // oversized...
    }

    const rxp *pSamp = (const rxp *) (input + sizeof(APMP_hdr));
    for (int i = 0; i < nFields; i++)
    {
        unsigned int channel = pSamp[i].control & 0x0000000F;    // extract without byte swapping
        if (channel < MAX_CHANNELS)
        {
            txp packet;

            packet.time = startTime + ((decodeTIMER(pSamp[i]) - PE) / 1000);   // milliseconds since midnight...
//DLOG((" UAS: %lu = %lu + (%lld - %lld) %lld", packet.time, startTime, decodeTIMER(pSamp[i]), PE, ((decodeTIMER(pSamp[i]) - PE) / 1000) ));
            packet.data = bigEndian->uint32Value(pSamp[i].data);
            memcpy(&outData[channel][nOutFields[channel]++ * sizeof(txp)], &packet, sizeof(txp));

            seqNum = (pSamp[i].control & 0x0000FF00) >> 8;  // extract without byte swapping
            if (seqNum % 256 != (_prevRXPseqNum[channel]+1) % 256)
            {
                _badRXPseqCnt[channel]++;
                WLOG(( "%s : RXP out of seq, channel=%d, APMP seq #%d, RXP# in APMP=%d/%d, data=%u, prevSeq=%d, thisSeq=%d",
                    n_u::UTime(samp->getTimeTag()).format(true,"%H:%M:%S.%3f").c_str(),
                    channel, _prevAPMPseqNum, i, nFields, (packet.data & 0x000000ff),
                    _prevRXPseqNum[channel], seqNum ));
            }
            _prevRXPseqNum[channel] = seqNum;
        }
        else
            WLOG(( "%s: received channel number %d, outside 0-3, ignoring.", getName().c_str(), channel ));
    }

    for (int i = 0; i < MAX_CHANNELS; i++) {
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
long long UDPArincSensor::decodeTIMER(const rxp& samp)
{
  long long ttime;

  /* Make 64-bit 20nsec/50Mhz Clock Ticks to 64-bit uSecs */
  ttime = bigEndian->uint32Value(samp.timeHigh);
  ttime = ((ttime << 32) | bigEndian->uint32Value(samp.timeLow)) / 50;

#ifdef DEBUG
  static long long prevTime = 0;
  printf("  rxp irig %llu usec, dT=%lld\n", ttime, ttime - prevTime);
  printf("           %llu msec, %llu sec\n", (ttime/1000), (ttime/1000000));
  prevTime = ttime;
#endif

  return ttime; // return microseconds
}


void UDPArincSensor::extractStatus(const char *msg, int len)
{
    const char *p = &msg[7];    // skip STATUS, keyword
    int cnt = 0;    // comma count
    char s[len];

    for (int i = 7; i < len && cnt < 4; ++i) {
        if (*p++ == ',')
            ++cnt;
        if (cnt == 1) {
            len -= (p-msg);
            ::memcpy(s, p, len); s[len]=0;
            configStatus["PBIT"] = atoi(s);
        }
        if (cnt == 3) {
            len -= (p-msg);
            ::memcpy(s, p, len); s[len]=0;
            configStatus["PPS"] = atoi(s);
        }
    }
}


void UDPArincSensor::printStatus(std::ostream& ostr) throw()
{
    DSMSensor::printStatus(ostr);
    if (getReadFd() < 0) {
        ostr << "<td align=left><font color=red><b>not active</b></font></td></tr>" << endl;
        return;
    }


    ostr << "<td align=left>";
    bool firstPass = true;
    for (map<string,int>::iterator it = configStatus.begin(); it != configStatus.end(); ++it)
    {
        bool iwarn = false;
        if (!firstPass) ostr << ',';

        if ((it->first).compare("PBIT") == 0) {
            if (it->second != 0)
                iwarn = true;
        }
        else
        if ((it->first).compare("PPS") == 0) {
            if (it->second != 0)
                iwarn = true;
        }
        else
        if (it->second == false)
            iwarn = true;

        if (iwarn) ostr << "<font color=red><b>";
        ostr << it->first;
        if (iwarn) ostr << "</b></font>";
        firstPass = false;
    }
    ostr << "</td></tr>";
}

