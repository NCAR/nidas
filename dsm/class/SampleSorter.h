/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_SAMPLESORTER_H
#define DSM_SAMPLESORTER_H

#include <SampleSource.h>
#include <SampleClient.h>
#include <SortedSampleSet.h>

#include <atdUtil/Thread.h>
#include <atdUtil/ThreadSupport.h>

namespace dsm {

class SampleSorter : public atdUtil::Thread,
	public SampleClient, public SampleSource {
public:

    SampleSorter(int buflenInMilliSec,const std::string& name);
    virtual ~SampleSorter();

    void interrupt();

    bool receive(const Sample *s) throw();

    unsigned long size() const { return samples.size(); }

    // void setDebug(bool val) { debug = val; }

    /**
     * flush all samples from buffer, distributing them to SampleClients.
     */
    void flush() throw (atdUtil::IOException);

protected:

    /**
     * Thread function.
     */
    virtual int run() throw(atdUtil::Exception);

    int buflenInMillisec;
    SortedSampleSet samples;
    SampleT<char> dummy;

private:

    atdUtil::Cond samplesAvail;

    int threadSignalFactor;

    int sampleCtr;

    // bool debug;

};
}
#endif
