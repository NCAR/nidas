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

#ifndef _nidas_dynld_raf_arinc_udp_h_
#define _nidas_dynld_raf_arinc_udp_h_

#include <nidas/dynld/UDPSocketSensor.h>
#include <nidas/util/EndianConverter.h>
#include "AltaEnet.h"


namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;


class DSMArincSensor;


/**
 * ARINC over UDP, data received from the Alta ARINC to Ethernet appliance.
 * ADC, GPS, IRS ARINC sensor will register themselves with this class.
 * This class will parse the samples in process() and pass them to the ARINC
 * class process() method.
 */
class UDPArincSensor : public UDPSocketSensor
{

public:
    UDPArincSensor();
    virtual ~UDPArincSensor();

    virtual void validate();

    virtual void open(int flags);

    virtual void close();

    /**
     * Over-ride default nextSample() so we can extract some status bits as
     * the data passes through.  This is used in real-time for the status page.
     */
    virtual Sample* nextSample()
    {
        Sample *samp = UDPSocketSensor::nextSample();

        if (samp) {
            const char *input = (const char *)samp->getConstVoidDataPtr();
            if (input[0] == 'S')      // status packet
                extractStatus(input, samp->getDataByteLength());
        }
        return samp;
    }

    void extractStatus(const char *msg, int len);

    void printStatus(std::ostream& ostr) throw();


    bool process(const Sample* samp,std::list<const Sample*>& results);

    void registerArincSensor(int channel, DSMArincSensor* sensor)
    { _arincSensors[channel] = sensor; }


protected:
    // Methods to decode pieces of the APMP and RXP packets.
    unsigned long decodeIRIG(unsigned char *);
    long long decodeTIMER(const rxp&);
    int bcd_to_decimal(unsigned char x) { return x - 6 * (x >> 4); }

    /// Broadcast UDP the latest attitude variables.
    void udpAttitude();


    // Data ships Big Endian.
    static const nidas::util::EndianConverter * bigEndian;

//@{
    unsigned int    _prevAPMPseqNum, _badAPMPseqCnt;
    unsigned int    _prevRXPseqNum[8], _badRXPseqCnt[8];
    unsigned int    _badStatusCnt;
//@}

    /// IP address of the Alta ARINC ENET appliance
    std::string     _ipAddr;

    /// Status port number for the alta_ctrl program.
    unsigned int    _statusPort;

    static const int MAX_CHANNELS;

    /**
     * This contains the status of config verification between what we read
     * off the device and what is in the XML.  These are then used in
     * printStatus for the status page.  nextSample() is over-ridden in order
     * to get the PPS status.
     */
    std::map<std::string, int> configStatus;

    /// Latest pitch, roll & heading to broadcast.
    float _latest_pitch, _latest_roll, _latest_hdg;

private:
    //  PID for process that intializes and controls ENET unit.
    pid_t _ctrl_pid;

    // Decoded IRIG time for a given APMP packet.
    char irigHHMMSS[32];

    // Channel #, AltaARINC sensor.
    std::map<int, DSMArincSensor*> _arincSensors;
};

}}}                     // namespace nidas namespace dynld namespace raf
#endif
