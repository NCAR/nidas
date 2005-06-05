
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_SAMPLEARCHIVER_H
#define DSM_SAMPLEARCHIVER_H

#include <SampleIOProcessor.h>
#include <SampleSorter.h>

namespace dsm {

class SampleArchiver: public SampleIOProcessor
{
public:
    
    SampleArchiver();

    /**
     * Copy constructor.
     */
    SampleArchiver(const SampleArchiver& x);

    virtual ~SampleArchiver();

    SampleIOProcessor* clone() const;

    bool singleDSM() const { return false; }

    void connect(dsm::SampleInput*) throw(atdUtil::IOException);

    void disconnect(dsm::SampleInput*) throw(atdUtil::IOException);

    void connected(SampleOutput* output) throw();

    void disconnected(SampleOutput* output) throw();

    void sendHeader(dsm_time_t thead,IOStream* iostream)
    	throw(atdUtil::IOException);

protected:

    SampleInput* input;
};

}

#endif
