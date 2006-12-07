
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_PSQL_PSQLPROCESSOR_H
#define NIDAS_DYNLD_PSQL_PSQLPROCESSOR_H

#include <nidas/core/SampleIOProcessor.h>
#include <nidas/core/SampleAverager.h>

namespace nidas { namespace dynld { namespace psql {

using nidas::core::SampleTag;
using nidas::core::SampleInput;
using nidas::core::SampleOutput;
using nidas::core::SampleAverager;

class PSQLProcessor: public nidas::core::SampleIOProcessor
{
public:
    
    PSQLProcessor();

    /**
     * Copy constructor.
     */
    PSQLProcessor(const PSQLProcessor& x);

    virtual ~PSQLProcessor();

    PSQLProcessor* clone() const;

    bool singleDSM() const { return false; }

    void connect(SampleInput*) throw(nidas::util::IOException);

    void disconnect(SampleInput*) throw(nidas::util::IOException);

    void connected(SampleOutput* orig, SampleOutput* output) throw();

    void disconnected(SampleOutput* output) throw();

    /**
     * Set average period, in milliseconds.
     */
    void setAveragePeriod(int val) { averager.setAveragePeriod(val); }

    /**
     * Get average period, in milliseconds.
     */
    int getAveragePeriod() const { return averager.getAveragePeriod(); }

    const std::set<const SampleTag*>& getSampleTags() const
    {
        return averager.getSampleTags();
    }

protected:

    SampleInput* input;

    SampleAverager averager;

    const nidas::core::Site* site;
};

}}}

#endif // NIDAS_DYNLD_PSQL_PSQLPROCESSOR_H
