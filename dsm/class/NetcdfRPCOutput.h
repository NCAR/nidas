/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-10-28 14:50:09 -0600 (Fri, 28 Oct 2005) $

    $LastChangedRevision: 3093 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/Socket.h $
 ********************************************************************

*/

#ifndef DSM_NETCDFRPCOUTPUT_H
#define DSM_NETCDFRPCOUTPUT_H

#include <SampleOutput.h>

namespace dsm {

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

    /**
    * Raw write not supported.
    */
    void write(const void* buf, size_t len)
    	throw (atdUtil::IOException)
    {
	throw atdUtil::IOException(getName(),"default write","not supported");
    }

    /**
     * Send a data record to the RPC server.
    */
    bool receive(const Sample*) throw ();

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

private:

    NetcdfRPCChannel* ncChannel;

};

}

#endif
