
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

    // SyncRecordGenerator(const SyncRecordGenerator&);

    virtual ~SyncRecordGenerator();

    // SyncRecordGenerator* clone() const;

    virtual bool cloneOnConnection() const { return false; }

    void connect(SampleInput* input) throw();
    
    void disconnect(SampleInput* input) throw();

    void connect(SampleOutput*,SampleOutput* output) throw();

    void disconnect(SampleOutput* output) throw();

    /**
     * Method called to write a header to an SampleOutput.
     */
    void sendHeader(dsm_time_t thead,SampleOutput* output)
        throw(nidas::util::IOException);

    const std::list<const SampleTag*>& getSampleTags() const
    {
        return _syncRecSource.getSampleTags();
    }

    void printStatus(std::ostream&,float deltat,int&) throw();

protected:
    void init() throw();

    void scanSensors(const std::list<DSMSensor*>& sensors);

    void allocateRecord(dsm_time_t timetag);

protected:

    SampleInput* _input;

    SampleOutput* _output;

    SyncRecordSource _syncRecSource;

    nidas::util::Mutex _statusMutex;

    size_t _numInputSampsLast;

    size_t _numOutputSampsLast;

    long long _numInputBytesLast;

    long long _numOutputBytesLast;

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
