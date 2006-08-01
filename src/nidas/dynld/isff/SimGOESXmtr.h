/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-10-28 14:50:09 -0600 (Fri, 28 Oct 2005) $

    $LastChangedRevision: 3093 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/Socket.h $
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
     * Queue a sample for writing to a GOES transmitter.
    */
    void transmitData(const nidas::util::UTime& at,
    	int configid,const Sample*) throw (nidas::util::IOException);

    void checkId() throw(nidas::util::IOException);

    int checkClock() throw(nidas::util::IOException);

    void printStatus() throw();

    void reset() throw(nidas::util::IOException) {}

private:

    nidas::util::UTime transmitQueueTime;

    nidas::util::UTime transmitAtTime;

    nidas::util::UTime transmitSampleTime;
    
};


}}}	// namespace nidas namespace dynld namespace isff

#endif
