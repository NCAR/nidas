// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_RAF_PSI9116_SENSOR_H
#define NIDAS_DYNLD_RAF_PSI9116_SENSOR_H

#include <nidas/core/CharacterSensor.h>

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

/**
 * Support for sampling a PSI 9116 pressure scanner from EsterLine
 * Pressure Systems.  This is a networked sensor, accepting connections
 * on TCP port 9000 and capable of receiving UDP broadcasts on port 7000.
 * Currently this class does not use UDP.
 */
class PSI9116_Sensor: public CharacterSensor
{

public:

    PSI9116_Sensor();

    ~PSI9116_Sensor() { }

    IODevice* buildIODevice() throw(nidas::util::IOException);

    void open(int flags)
    	throw(nidas::util::IOException,nidas::util::InvalidParameterException);

    void addSampleTag(SampleTag* stag)
            throw(nidas::util::InvalidParameterException);

    bool process(const Sample* samp,std::list<const Sample*>& results)
    	throw();

    /**
     * Stop data streams, set valve position to PURGE.
     */
    void startPurge() throw(nidas::util::IOException);

    /**
     * Set valve position back to RUN from PURGE,
     * then restart data streams.
     */
    void stopPurge() throw(nidas::util::IOException);

    void startStreams() throw(nidas::util::IOException);

    void stopStreams() throw(nidas::util::IOException);

    void executeXmlRpc(XmlRpc::XmlRpcValue& params, XmlRpc::XmlRpcValue& result) throw();

protected:

    std::string sendCommand(const std::string& cmd,int readlen = 0)
    	throw(nidas::util::IOException);

    int _msecPeriod;

    /**
     * Number of sampled channels.
     */
    int _nchannels;

    dsm_sample_id_t _sampleId;

    /**
     * Conversion factor to apply to PSI data. 
     * PSI9116 by default reports data in psi.
     * A factor 68.94757 will convert to millibars.
     */
    float _psiConvert;

    unsigned int _sequenceNumber;

    size_t _outOfSequence;

    // Stuff to handle when output data samples are broken 
    // over two input samples (as happened on ICE-T)
    union flip {
        unsigned int lval;
        float fval;
        char bytes[4];
    } _prevPartial;
    bool _partialFirst, _partialSecond;
    SampleT<float> *_firstPrevious, *_secondPrevious;
    int _nPrevSampVals;
    bool _gotOne;
    int _prevPartNBytes;

private:

    /** No copying. */
    PSI9116_Sensor(const PSI9116_Sensor&);

    /** No assignment. */
    PSI9116_Sensor& operator=(const PSI9116_Sensor&);

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
