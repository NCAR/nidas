/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef DSM_XMLRPCTHREAD_H
#define DSM_XMLRPCTHREAD_H

#include <atdUtil/Thread.h>

#include <xmlrpc++/XmlRpc.h>

namespace dsm {

/**
 * A DSMEngine thread that provides an XML-RPC service to the
 * web interfaces.
 */
class XmlRpcThread: public atdUtil::Thread
{
public:
    
    /**
     * Constructor.
     */
    XmlRpcThread(const std::string& name);
    ~XmlRpcThread();

    int run() throw(atdUtil::Exception);

 private:

  XmlRpc::XmlRpcServer* xmlrpc_server;
};

}

#endif
