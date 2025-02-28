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
#ifndef NIDAS_DYNLD_DSC_FREQCOUNTER_H
#define NIDAS_DYNLD_DSC_FREQCOUNTER_H

#include <nidas/core/DSMSensor.h>
#include <nidas/core/Parameter.h>
#include <nidas/util/EndianConverter.h>
#include <nidas/core/TimetagAdjuster.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * Sensor support for a frequency counter device.
 * This implementation supports a device that can
 * configured with a reporting period value ( e.g. 1 sec, 
 * or 1/10 sec), and a number of pulses to count over that time.
 * Samples from the device are two little-endian
 * 4 byte unsigned integers, containing the number of pulses
 * counted along with a number of clock ticks elapsed while
 * counting those pulses.
 */
class DSC_FreqCounter : public DSMSensor {

public:

    DSC_FreqCounter();
    ~DSC_FreqCounter();

    /**
     * @throws nidas::util::IOException
     **/
    IODevice* buildIODevice();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    SampleScanner* buildSampleScanner();

    /**
     * Open a GPIO-MM frequency counter.
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::InvalidParameterException
     **/
    void open(int flags);

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void validate();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void init();

    /**
     * @throw()
     **/
    void printStatus(std::ostream& ostr);

    /**
     * Calculate the input pulse period in microseconds.
     */
    double calculatePeriodUsec(const Sample*) const;

    /**
     * Calculate the input pulse period in microseconds,
     * based on the number of pulses counted (npulses) and the number
     * of clock tics counted while counting npulses.
     */
    double calculatePeriodUsec(unsigned int npulses, unsigned int tics) const;

    /**
     * Process a raw sample, which in this case means convert the
     * counts and elapsed ticks into a frequency.
     */
    bool process(const Sample*,std::list<const Sample*>& result)
        throw();

    /**
     * Return the frequency sample period.
     */
    int getSamplePeriodMsec() const
    {
        return _msecPeriod;
    }


protected:

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    virtual void readParams(const std::list<const Parameter*>& params);

    const SampleTag* _stag;

    dsm_sample_id_t _sampleId;

    int _nvars;

    int _msecPeriod;

    /**
     * Number of input pulses to count.
     */
    int _numPulses;

    /**
     * Rate of reference clock whose tics are counted while
     * _numPulses are counted.
     */
    double _clockRate;

    const nidas::util::EndianConverter* _cvtr;

    /**
     * TimetagAdjusters for my sample.
     **/
    std::map<const SampleTag*, TimetagAdjuster*> _ttadjusters;

private:

    /** No copying. */
    DSC_FreqCounter(const DSC_FreqCounter&);

    /** No assignment. */
    DSC_FreqCounter& operator=(const DSC_FreqCounter&);

};

}}	// namespace nidas namespace dynld

#endif
