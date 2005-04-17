
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
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

    void addInput(SampleInput*);

    void connected(SampleOutput* output) throw();

    void disconnected(SampleOutput* output) throw();

protected:

    SampleSorter sorter;

    atdUtil::Mutex initMutex;

    bool initialized;

};

}

#endif
