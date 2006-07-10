
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

class nidas::core::DSMSensor;

class SyncRecordGenerator: public SampleIOProcessor
{
public:
    
    /**
     * Constructor.
     */
    SyncRecordGenerator();

    SyncRecordGenerator(const SyncRecordGenerator&);

    virtual ~SyncRecordGenerator();

    SyncRecordGenerator* clone() const;

    virtual bool singleDSM() const { return false; }

    void connect(SampleInput* input) throw(nidas::util::IOException);
    
    void disconnect(SampleInput* input) throw(nidas::util::IOException);

    void connected(SampleOutput*,SampleOutput* output) throw();

    void disconnected(SampleOutput* output) throw();

    void sendHeader(dsm_time_t thead,SampleOutput* output) throw(nidas::util::IOException);

    const std::set<const SampleTag*>& getSampleTags() const
    {
        return syncRecSource.getSampleTags();
    }

protected:
    void init() throw();

    void scanSensors(const std::list<DSMSensor*>& sensors);

    void allocateRecord(dsm_time_t timetag);

protected:

    SampleInput* input;

    SyncRecordSource syncRecSource;

};

}}}	// namespace nidas namespace dynld namespace raf

#endif
