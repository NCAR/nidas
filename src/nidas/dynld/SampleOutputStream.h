/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef NIDAS_DYNLD_SAMPLEOUTPUTSTREAM_H
#define NIDAS_DYNLD_SAMPLEOUTPUTSTREAM_H


#include <nidas/core/SampleOutput.h>

namespace nidas { namespace dynld {

using namespace nidas::core;

/**
 * A class for serializing Samples on an OutputStream.
 */
class SampleOutputStream: public SampleOutputBase
{
public:

    SampleOutputStream();

    /**
     * Create a SampleOutputStream with a connected IOChannel.
     */
    SampleOutputStream(IOChannel* iochan);

    virtual ~SampleOutputStream();

    /**
     * Implementation of IOChannelRequester::connected().
     * How an IOChannel indicates that it has received a connection.
     */
    SampleOutput* connected(IOChannel* ochan) throw();

    /**
     * Get the IOStream of this SampleOutputStream.
     * SampleOutputStream owns the pointer and
     * will delete the IOStream in its destructor.
     * The IOStream is available after a SammpleOutputStream is 
     * constructed with an connected IOChannel, or after the connected()
     * method has been called and before close().
     */
    IOStream* getIOStream() { return _iostream; }

    void close() throw(nidas::util::IOException);

    bool receive(const Sample *s) throw();

    void finish() throw();

    size_t write(const void* buf, size_t len, bool streamFlush)
    	throw(nidas::util::IOException);

    /**
     * Outgoing data is buffered in an IOStream.
     * The stream will be flushed when the difference between
     * successive time tags exceeds this value.
     * This is a useful parameter for real-time applications.
     * @param val Number of seconds between physical writes.
     *        Default: 0.25
     */
    void setMaxSecBetweenWrites(float val) { _maxUsecs = (int)rint((double)val * USECS_PER_SEC); }

protected:

    SampleOutputStream* clone(IOChannel* iochannel);

    /**
     * Copy constructor, with a new IOChannel.
     */
    SampleOutputStream(SampleOutputStream&,IOChannel*);

    size_t write(const Sample* samp, bool streamFlush) throw(nidas::util::IOException);

    IOStream* _iostream;

private:

    /**
     * Maximum number of microseconds between physical writes.
     */
    int _maxUsecs;

    /**
     * Timetag of last flush of IOStream.
     */
    dsm_time_t _lastFlushTT;

    /**
     * No copy.
     */
    SampleOutputStream(const SampleOutputStream&);

    /**
     * No assignment.
     */
    SampleOutputStream& operator=(const SampleOutputStream&);

};

}}	// namespace nidas namespace core

#endif
