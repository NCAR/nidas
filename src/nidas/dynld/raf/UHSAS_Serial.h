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

#ifndef NIDAS_DYNLD_RAF_UHSAS_SERIAL_H
#define NIDAS_DYNLD_RAF_UHSAS_SERIAL_H

#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/util/EndianConverter.h>

namespace nidas { namespace core {
    class VariableConverter;
}}

namespace nidas { namespace dynld { namespace raf {

/**
 * A class for reading the UHSAS probe.  This appears to be an updated PCASP.
 * RS-232 @ 115,200 baud.
 */
class UHSAS_Serial : public DSMSerialSensor
{
public:

    UHSAS_Serial();

    ~UHSAS_Serial();

    void open(int flags)
        throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    /**
     * Setup whatever is necessary for process method to work.
     */
    void init() throw(nidas::util::InvalidParameterException);

    void sendInitString() throw(nidas::util::IOException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
          throw();

    void setSendInitBlock(bool val)
    {
        _sendInitBlock = val;
    }

    bool getSendInitBlock() const
    {
        return _sendInitBlock;
    }

    static unsigned const char* findMarker(unsigned const char* ip,unsigned const char* eoi,
        unsigned char* marker, int len);

private:

    static const nidas::util::EndianConverter * fromLittle;

    /**
     * Total number of floats in the processed output sample.
     */
    int _noutValues;

    /**
     * Number of histogram bins to be read. Probe puts out 100
     * histogram values, but it appears that largest bin is not to be used,
     * so there are 99 usable bins on this probe.
     * To be compatible with old datasets, the XML may specify 100 bins, and
     * first bin will be zeroed.
     */
    int _nValidChannels;

    /**
     * Number of housekeeping channels.  9 of 12 possible are unpacked.
     */
    int _nHousekeep;

    /**
     * Housekeeping scale factors.
     */
    float _hkScale[12];

    /**
     * Some UHSAS insturments generate ASCII.
     */
    bool _binary;

    /**
     * UHSAS sample-rate, currently used for scaling the sum of the bins.
     */
    float _sampleRate;

    bool _sendInitBlock;

    int _nOutBins;

    bool _sumBins;

    unsigned int _nDataErrors;

    /**
     * sample period in microseconds.
     */
    int _dtUsec;

    /**
     * number of stitch sequences encountered.
     */
    unsigned int _nstitch;

    /**
     * Number of times that the number of bytes between the histogram markers
     * (ffff04 and ffff05) exceeds the expected number of 200.
     */
    unsigned int _largeHistograms;

    unsigned int _totalHistograms;

    std::vector<nidas::core::VariableConverter*> _converters;

};

}}}	// namespace nidas namespace dynld raf

#endif
