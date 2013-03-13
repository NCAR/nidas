// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_RAF_SYNCRECORDGENERATOR_H
#define NIDAS_DYNLD_RAF_SYNCRECORDGENERATOR_H

#include <nidas/core/SampleIOProcessor.h>
#include <nidas/dynld/raf/SyncRecordSource.h>

#include <vector>
#include <map>
#include <string>

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
    void addSampleClient(SampleClient* client) throw()
    {
        _syncRecSource.addSampleClient(client);
    }

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

protected:
    void init() throw();

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
