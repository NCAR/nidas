// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2007, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_RAF_BCPD_SERIAL_H
#define NIDAS_DYNLD_RAF_BCPD_SERIAL_H

#include "SppSerial.h"

namespace nidas { namespace dynld { namespace raf {

/**
 * A class for reading PMS1D probes with the DMT interface conversion.
 * RS-422 @ 38400 baud.
 */
class BCPD_Serial : public SppSerial
{
public:

    BCPD_Serial();

    void validate()
        throw(nidas::util::InvalidParameterException);

    void sendInitString() throw(nidas::util::IOException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
        throw();


    // Packet to initialize probe with.
    struct InitBCPD_blk
    {
        char    esc;                                // ESC 0x1b
        char    id;                                 // cmd id
        DMT_UShort  trig_thresh;                    // ADC Lower Threshold
        DMT_UShort  numPbP;                         // # of PbP packets
        DMT_UShort  chanCnt;                        // Channel Count
        DMT_UShort  numBytes;                       // # of bytes of raw data per channel
        DMT_UShort  spare1;
        DMT_UShort  spare2;
        DMT_UShort  OPCthreshold[MAX_CHANNELS];     // OPCthreshold[MAX_CHANNELS]
        DMT_UShort  chksum;                         // cksum
    };

    static const int _InitPacketSize = 96;

    /**
     * Data packet back from probe (all unsigned little-endian):
     *	starting byte	size	contents
     *	--------------------------------
     *	0		32	2-byte cabin channel (* 16)
     *	32		4	4 byte unused
     *	36		2	2 byte unused
     *	38		2	S Noise Bandwidth
     *	40		2	S Baseline Threshold
     *	42		2	P Noise Bandwidth
     *	44		2	P Baseline Threshold
     *	46		4	Oversize Reject
     *	50		4*nchan	4-byte histogram/bin-data (* nchan)
     *	170		6	Timer response header
     *	176		*	PbP Data (12 bytes pere PBP particle)
     *	*               2080    Raw PbP Data
     *	*        	2	2-byte checksum
     * (Total data size without checksum 34 + 4*nchan bytes)
     *
     * The struct below is set up similar to the above, but because of
     * alignment issues (e.g., the 2-byte unused2 above starts at byte
     * 36 but in the struct below the unused2 will probably be aligned
     * 40 bytes into the struct), we CANNOT do a single wholesale copy to
     * move the packet data into the struct.
     *
     * Also, the struct is sized to hold the maximum number of channels, while
     * the actual data packet may contain fewer than the maximum.
     */
    struct DMTBCPD_blk
    {
        DMT_UShort  cabinChan[16];
        DMT_ULong   unused1;
        DMT_UShort  unused2;
        DMT_UShort  SnoiseBandwidth;
        DMT_UShort  SbaselineThreshold;
        DMT_UShort  PnoiseBandwidth;
        DMT_UShort  PbaselineThreshold;
        DMT_ULong   oversizeReject;
        DMT_ULong   OPCchan[MAX_CHANNELS];	// 30 channels only
        DMT_UShort  chksum;
    };

protected:

    int packetLen() const {
        return (52 + 4 * _nChannels);
    }

    static const size_t P_APD_Vdc, P_APD_Temp, FiveVdcRef, BoardTemp, S_APD_Vdc, S_APD_Temp;

};

}}}	// namespace nidas namespace dynld raf

#endif
