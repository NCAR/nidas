// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_ISFF_GOESOUTPUT_H
#define NIDAS_DYNLD_ISFF_GOESOUTPUT_H

#include <nidas/dynld/isff/GOESXmtr.h>
#include <nidas/core/SampleOutput.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/Thread.h>

#include <vector>

namespace nidas {

namespace core {
class SampleTag;
class IOChannel;
}

namespace dynld { namespace isff {

/**
 * A SampleOutput for packaging data for a GOES DCP Transmitter.
 */
class GOESOutput: public nidas::core::SampleOutputBase, public nidas::util::Runnable {

public:

    /**
     * Constructor.
     */
    GOESOutput(nidas::core::IOChannel* ioc = 0,SampleConnectionRequester* rqstr=0);

    /**
     * Destructor.
     */
    ~GOESOutput();

    /**
     * Implementation of SampleClient::flush().
     */
    void flush() throw() {}

    void setIOChannel(nidas::core::IOChannel* val);

    /**
     * Clone invokes copy constructor.
     */
    GOESOutput* clone(nidas::core::IOChannel* iochannel=0)
    {
        return new GOESOutput(*this, iochannel);
    }

    /**
     * Implemention of IOChannelRequester::connected().
     * Once we have the connection to the transmitter,
     * initialize things before sending packets.
     */
    SampleOutput* connected(nidas::core::IOChannel* ochan) throw();

    /**
     * The GOES transmit interval, in seconds.
     */
    int getXmitInterval() const
    { 
        if (_goesXmtr) return _goesXmtr->getXmitInterval();
	return -1;
    }

    /**
     * The GOES transmit offset, in seconds.
     */
    int getXmitOffset() const
    { 
        if (_goesXmtr) return _goesXmtr->getXmitOffset();
	return -1;
    }

    void addRequestedSampleTag(nidas::core::SampleTag* tag)
	throw(nidas::util::InvalidParameterException);

    void addSourceSampleTag(const nidas::core::SampleTag* tag)
        throw(nidas::util::InvalidParameterException);

    void close() throw();

    /**
    * Raw write not supported.
    */
    size_t write(const void*, size_t)
    	throw (nidas::util::IOException)
    {
	throw nidas::util::IOException(getName(),"default write","not supported");
    }

    /**
     * Send a data record to the RPC server.
    */
    bool receive(const Sample*) throw ();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    int run() throw(nidas::util::Exception);

    void interrupt();

    bool isInterrupted() const
    {
        return _interrupted;
    }

protected:

    /**
     * Copy constructor, with a new IOChannel*. Used by clone().
     */
    GOESOutput(GOESOutput&,nidas::core::IOChannel*);

    void joinThread() throw();

    void cancelThread() throw();

    void killThread() throw();

private:

    GOESXmtr* _goesXmtr;

    std::map<dsm_sample_id_t,std::vector<std::vector<std::pair<int,int> > > > _sampleMap;

    std::vector<SampleT<float>*> _outputSamples;

    nidas::util::Mutex _sampleMutex;

    nidas::util::ThreadRunnable* _xmitThread;

    bool _interrupted;

    int _configid;

    int _stationNumber;

    /**
     * Not sure what the plan was for this. Not currently used.
     */
    long long _maxPeriodUsec;

    /**
     * No normal copy.
     */
    GOESOutput(const GOESOutput&);

    /**
     * No assignment.
     */
    GOESOutput& operator=(const GOESOutput&);

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
