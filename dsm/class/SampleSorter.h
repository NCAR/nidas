/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
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

    SampleSorter(int buflenInMilliSec);
    virtual ~SampleSorter();

    void interrupt();

    bool receive(const Sample *s)
	throw(SampleParseException, atdUtil::IOException);

    unsigned long size() const { return samples.size(); }

    // void setDebug(bool val) { debug = val; }

    /**
     * flush all samples from buffer, distributing them to SampleClients.
     */
    void flush() throw (SampleParseException,atdUtil::IOException);

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
