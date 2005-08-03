/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef XMLRPCTHREAD_H
#define XMLRPCTHREAD_H

#include <atdUtil/Thread.h>
#include <xmlrpc++/XmlRpc.h>

namespace dsm {

/**
 * A thread that provides XML-based Remote Procedure Calls
 * to web interfaces.
 */
class XmlRpcThread: public atdUtil::Thread
{
public:
    
    /** Constructor. */
    XmlRpcThread(const std::string& name);
    ~XmlRpcThread();

 protected:

  XmlRpc::XmlRpcServer* _xmlrpc_server;
};

}

#endif
