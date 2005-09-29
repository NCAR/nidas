
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_PSQLPROCESSOR_H
#define DSM_PSQLPROCESSOR_H

#include <SampleIOProcessor.h>
#include <SampleAverager.h>

namespace dsm {

class PSQLProcessor: public SampleIOProcessor
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

    void connect(dsm::SampleInput*) throw(atdUtil::IOException);

    void disconnect(dsm::SampleInput*) throw(atdUtil::IOException);

    void connected(SampleOutput* output) throw();

    void disconnected(SampleOutput* output) throw();

    /**
     * Set average period, in milliseconds.
     */
    void setAveragePeriod(int val) { averager.setAveragePeriod(val); }

    /**
     * Get average period, in milliseconds.
     */
    int getAveragePeriod() const { return averager.getAveragePeriod(); }


protected:

    SampleInput* input;

    SampleAverager averager;

};

}

#endif
