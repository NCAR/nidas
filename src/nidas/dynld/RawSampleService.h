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


#ifndef NIDAS_DYNLD_RAWSAMPLESERVICE_H
#define NIDAS_DYNLD_RAWSAMPLESERVICE_H

#include <nidas/core/DSMService.h>

namespace nidas {

namespace core {
class DSMConfig;
class SamplePipeline;
class SampleInput;
class SampleOutput;
}

namespace dynld {

/**
 * A RawSampleService reads raw Samples from a socket connection
 * and sends the samples to one or more SampleIOProcessors.
 */
class RawSampleService: public nidas::core::DSMService
{
public:
    RawSampleService();

    ~RawSampleService();

    void connect(nidas::core::SampleInput*) throw();

    void disconnect(nidas::core::SampleInput*) throw();

    /**
     * @throws nidas::util::Exception
     **/
    void schedule(bool optionalProcessing);

    void interrupt() throw();

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void fromDOMElement(const xercesc::DOMElement* node);

    void printClock(std::ostream& ostr) throw();

    void printStatus(std::ostream& ostr,float deltat) throw();

    /**
     * Get the length of the SampleSorter of raw Samples, in seconds.
     */
    float getRawSorterLength() const
    {
        return _rawSorterLength;
    }

    /**
     * Set the length of the SampleSorter of raw Samples, in seconds.
     */
    void setRawSorterLength(float val)
    {
        _rawSorterLength = val;
    }

    /**
     * Get the length of the SampleSorter of processed Samples, in seconds.
     */
    float getProcSorterLength() const
    {
        return _procSorterLength;
    }

    /**
     * Set the length of the SampleSorter of processed Samples, in seconds.
     */
    void setProcSorterLength(float val)
    {
        _procSorterLength = val;
    }

    /**
     * Get the size of in bytes of the raw SampleSorter.
     * If the size of the sorter exceeds this value
     * then samples will be discarded.
     */
    size_t getRawHeapMax() const
    {
        return _rawHeapMax;
    }

    /**
     * Set the size of in bytes of the raw SampleSorter.
     * If the size of the sorter exceeds this value
     * then samples will be discarded.
     */
    void setRawHeapMax(size_t val)
    {
        _rawHeapMax = val;
    }

    /**
     * Get the size of in bytes of the processed SampleSorter.
     * If the size of the sorter exceeds this value
     * then samples will be discarded.
     */
    size_t getProcHeapMax() const
    {
        return _procHeapMax;
    }

    /**
     * Set the size of in bytes of the processed SampleSorter.
     * If the size of the sorter exceeds this value
     * then samples will be discarded.
     */
    void setProcHeapMax(size_t val)
    {
        _procHeapMax = val;
    }

    /**
     * Get the size of the late sample cache in the raw sample sorter.
     * See SampleSorter::getLateSampleCacheSize(). Default: 0.
     */
    unsigned int getRawLateSampleCacheSize() const
    {
        return _rawLateSampleCacheSize;
    }

    /**
     * Cache this number of samples with potentially anomalous, late time tags
     * in the raw sample sorter.
     * See SampleSorter::setLateSampleCacheSize(val).
     */
    void setRawLateSampleCacheSize(unsigned int val)
    {
        _rawLateSampleCacheSize = val;
    }

    /**
     * Get the size of the late sample cache in the processed sample sorter.
     * See SampleSorter::getLateSampleCacheSize(). Default: 0.
     */
    unsigned int getProcLateSampleCacheSize() const
    {
        return _procLateSampleCacheSize;
    }

    /**
     * Cache this number of samples with potentially anomalous, late time tags
     * in the processed sample sorter.
     * See SampleSorter::setLateSampleCacheSize(val).
     */
    void setProcLateSampleCacheSize(unsigned int val)
    {
        _procLateSampleCacheSize = val;
    }

private:

    nidas::core::SamplePipeline* _pipeline;

    /**
     * Worker thread that is run when a SampleInputConnection is established.
     */
    class Worker: public nidas::util::Thread
    {
        public:
            Worker(RawSampleService* svc,nidas::core::SampleInput *input);
            ~Worker();
            int run();
            void interrupt();
        private:
            RawSampleService* _svc;
            nidas::core::SampleInput* _input;
            /** No copying. */
            Worker(const Worker&);
            /** No assignment. */
            Worker& operator=(const Worker&);
    };

    /**
     * Keep track of the Worker for each SampleInput.
     */
    std::map<nidas::core::SampleInput*,Worker*> _workers;

    std::map<nidas::core::SampleInput*,const nidas::core::DSMConfig*> _dsms;

    nidas::util::Mutex _workerMutex;

    /**
     * Saved between calls to printStatus in order to compute data rates.
     */
    std::map<void*,size_t> _nsampsLast;

    /**
     * Saved between calls to printStatus in order to compute sample rates.
     */
    std::map<void*,long long> _nbytesLast;

    float _rawSorterLength;

    float _procSorterLength;

    size_t _rawHeapMax;

    size_t _procHeapMax;

    unsigned int _rawLateSampleCacheSize;

    unsigned int _procLateSampleCacheSize;

    /**
     * Copying not supported.
     */
    RawSampleService(const RawSampleService&);

    /**
     * Assignment not supported.
     */
    RawSampleService& operator =(const RawSampleService&);
};

}}	// namespace nidas namespace dynld

#endif
