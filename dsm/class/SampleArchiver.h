
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_SAMPLEARCHIVER_H
#define DSM_SAMPLEARCHIVER_H

#include <SampleIOProcessor.h>

namespace dsm {

class SampleArchiver: public SampleIOProcessor
{
public:
    
    SampleArchiver();

    virtual ~SampleArchiver();

    SampleIOProcessor* clone() const;

    bool singleDSM() const { return true; }

    void connect(dsm::SampleInput*) throw(atdUtil::IOException);

    void disconnect(dsm::SampleInput*) throw(atdUtil::IOException);

    void addInput(SampleInput*);

    void connected(SampleOutput* output) throw();

    void disconnected(SampleOutput* output) throw();

protected:


    std::list<SampleInput*> inputs;

    atdUtil::Mutex inputListMutex;

};

}

#endif
