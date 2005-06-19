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

/**
 * A SampleClient that sorts its received samples,
 * using an STL multimap, and then sends the
 * sorted samples onto its SampleClients.
 * One specifies a sorter length in the constructor.
 * Samples whose time-tags are less than the time-tag
 * of the latest sample received, minus the sorter length,
 * are sent on to the SampleClients.
 * This is implemented as a Thread, which must be started,
 * otherwise the sorter will grow and no samples will be
 * sent to clients.
 */
class SampleSorter : public atdUtil::Thread,
	public SampleClient, public SampleSource {
public:

    /**
     * Constructor.
     * @param sorterLength Length of the SampleSorter, in milliseconds.
     */
    SampleSorter(int sorterLength,const std::string& name);
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
     * Thread run function.
     */
    virtual int run() throw(atdUtil::Exception);

    /**
     * Length of SampleSorter, in milliseconds.
     */
    int sorterLengthUsec;

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
