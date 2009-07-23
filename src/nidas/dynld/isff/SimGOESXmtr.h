/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_ISFF_SIMGOESXMTR_H
#define NIDAS_DYNLD_ISFF_SIMGOESXMTR_H

#include <nidas/core/IOChannel.h>
#include <nidas/core/SampleTag.h>
#include <nidas/dynld/isff/GOESXmtr.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>

#include <string>
#include <iostream>
#include <vector>

namespace nidas { namespace dynld { namespace isff {

using namespace nidas::core;

/**
 * An IOChannel implementation to simulate a GOES transmitter.
 */
class SimGOESXmtr: public GOESXmtr {

public:

    /**
     * Constructor.
     */
    SimGOESXmtr();

    /**
     * Copy constructor.
     */
    SimGOESXmtr(const SimGOESXmtr&);

    /**
     * Destructor.
     */
    ~SimGOESXmtr();

    /**
     * Clone invokes default copy constructor.
     */
    SimGOESXmtr* clone() const { return new SimGOESXmtr(*this); }

    /**
     * Set the RF baud rate
     * @param val RF baud, in bits/sec.
     */
    void setRFBaud(long val) throw(nidas::util::InvalidParameterException)
    {
        _rfBaud = val;
    }

    int getRFBaud() const
    {
        return _rfBaud;
    }
    

    void open() throw(nidas::util::IOException);

    /**
     * Do the actual hardware read.
     */
    size_t read(void* buf, size_t len) throw (nidas::util::IOException)
    {
        throw nidas::util::IOException(getName(),"read","not supported");
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const void* buf, size_t len) throw (nidas::util::IOException)
    {
        throw nidas::util::IOException(getName(),"write","not supported");
    }

    /**
    * Do the actual hardware write.
    */
    size_t write(const struct iovec* iov, int iovcnt) throw (nidas::util::IOException)
    {
        throw nidas::util::IOException(getName(),"write","not supported");
    }

    /**
     * Queue a sample for writing to a GOES transmitter.
    */
    void transmitData(const nidas::util::UTime& at,
    	int configid,const Sample*) throw (nidas::util::IOException);

    unsigned long checkId() throw(nidas::util::IOException);

    int checkClock() throw(nidas::util::IOException);

    void printStatus() throw();

    void reset() throw(nidas::util::IOException) {}

    void init() throw(nidas::util::IOException) {}

private:

    nidas::util::UTime _transmitQueueTime;

    nidas::util::UTime _transmitAtTime;

    nidas::util::UTime _transmitSampleTime;

    int _rfBaud;
    
};


}}}	// namespace nidas namespace dynld namespace isff

#endif
