/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_ISFF_NETCDFRPCOUTPUT_H
#define NIDAS_DYNLD_ISFF_NETCDFRPCOUTPUT_H

#include <nidas/core/SampleOutput.h>

namespace nidas { namespace dynld { namespace isff {

using namespace nidas::core;

class NetcdfRPCChannel;

/**
 * A perversion of a simple IOChannel.  This sends data
 * to a NetCDF via RPC calls.
 */
class NetcdfRPCOutput: public SampleOutputBase {

public:

    /**
     * Constructor.
     */
    NetcdfRPCOutput(IOChannel* ioc = 0);

    /**
     * Copy constructor.
     */
    NetcdfRPCOutput(const NetcdfRPCOutput&);

    NetcdfRPCOutput(const NetcdfRPCOutput&,IOChannel*);

    /**
     * Destructor.
     */
    ~NetcdfRPCOutput();

    void setIOChannel(IOChannel* val);

    /**
     * Clone invokes default copy constructor.
     */
    NetcdfRPCOutput* clone(IOChannel* iochannel=0) const
    {
        return new NetcdfRPCOutput(*this);
    }

    void addSampleTag(const SampleTag*);

    /**
    * Raw write not supported.
    */
    size_t write(const void* buf, size_t len)
    	throw (nidas::util::IOException)
    {
	throw nidas::util::IOException(getName(),"default write","not supported");
    }

    /**
     * Send a data record to the RPC server.
    */
    bool receive(const Sample*) throw ();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

private:

    NetcdfRPCChannel* ncChannel;

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
