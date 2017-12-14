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

#ifndef NIDAS_DYNLD_RAF_SYNCRECORDGENERATOR_H
#define NIDAS_DYNLD_RAF_SYNCRECORDGENERATOR_H

#include <nidas/core/SampleIOProcessor.h>
#include "SyncRecordSource.h"

namespace nidas { namespace dynld { namespace raf {

using namespace nidas::core;

class SyncRecordGenerator: public SampleIOProcessor, public HeaderSource
{
public:
    
    /**
     * Constructor.
     */
    SyncRecordGenerator();

    virtual ~SyncRecordGenerator();

    /**
     * Implementation of SampleIOProcessor::connect(SampleSource*).
     */
    void connect(SampleSource* source) throw();
    
    /**
     * Implementation of SampleIOProcessor::disconnect(SampleSource*).
     */
    void disconnect(SampleSource* source) throw();

    /**
     * Implementation of SampleConnectionRequester::connect(SampleOutput*).
     */
    void connect(SampleOutput* output) throw();

    /**
     * Implementation of SampleConnectionRequester::disconnect(SampleOutput*).
     */
    void disconnect(SampleOutput* output) throw();

    /**
     * Method called to write a header to an SampleOutput.
     */
    void sendHeader(dsm_time_t thead,SampleOutput* output)
        throw(nidas::util::IOException);

    void init(dsm_time_t sampleTime) throw();

    std::list<const SampleTag*> getSampleTags() const
    {
        return _syncRecSource.getSampleTags();
    }

    /**
     * Implementation of SampleSource::getSampleTagIterator().
     */
    SampleTagIterator getSampleTagIterator() const
    {
        return _syncRecSource.getSampleTagIterator();
    }

    /**
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClient(SampleClient* client) throw();

    void removeSampleClient(SampleClient* client) throw()
    {
        _syncRecSource.removeSampleClient(client);
    }

    /**
     * Add a Client for a given SampleTag.
     * Implementation of SampleSource::addSampleClient().
     */
    void addSampleClientForTag(SampleClient* client,const SampleTag* tag) throw()
    {
        _syncRecSource.addSampleClientForTag(client,tag);
    }

    void removeSampleClientForTag(SampleClient* client,const SampleTag* tag) throw()
    {
        _syncRecSource.removeSampleClientForTag(client,tag);
    }

    int getClientCount() const throw()
    {
        return _syncRecSource.getClientCount();
    }

    /**
     * Implementation of SampleSource::flush().
     */
    void flush() throw()
    {
        _syncRecSource.flush();
    }

    const SampleStats& getSampleStats() const
    {
        return _syncRecSource.getSampleStats();
    }

    void printStatus(std::ostream&,float deltat,int&) throw();

    SyncRecordSource*
    getSyncRecordSource()
    {
        return &_syncRecSource;
    }

protected:

    void scanSensors(const std::list<DSMSensor*>& sensors);

    void allocateRecord(dsm_time_t timetag);

private:

    nidas::util::Mutex _connectionMutex;

    std::set<SampleSource*> _connectedSources;

    std::set<SampleOutput*> _connectedOutputs;

    SyncRecordSource _syncRecSource;

    size_t _numInputSampsLast;

    size_t _numOutputSampsLast;

    long long _numInputBytesLast;

    long long _numOutputBytesLast;

    /** No copying. */
    SyncRecordGenerator(const SyncRecordGenerator&);

    /** No assignment. */
    SyncRecordGenerator& operator=(const SyncRecordGenerator&);

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
