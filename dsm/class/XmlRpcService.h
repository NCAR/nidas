/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#ifndef DSM_XMLRPCSERVICE_H
#define DSM_XMLRPCSERVICE_H

#include <DSMService.h>

#include <xmlrpc++/XmlRpc.h>

using namespace XmlRpc;

namespace dsm {

/**
 * Starts a thread that provides XML-based Remote Procedure Calls
 * to a web interfaces.
 * @see DSMServer::scheduleServices()
 */
class XmlRpcService : public DSMService
{
public:
    XmlRpcService();

    /**
     * Copy constructor.
     */
    XmlRpcService(const XmlRpcService&);

    ~XmlRpcService();

    DSMService* clone() const;

    int run() throw(atdUtil::Exception);

    void schedule() throw(atdUtil::Exception);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException);

    xercesc::DOMElement*
    	toDOMParent(xercesc::DOMElement* parent)
		throw(xercesc::DOMException);

    xercesc::DOMElement*
    	toDOMElement(xercesc::DOMElement* node)
		throw(xercesc::DOMException);

 private:

  XmlRpc::XmlRpcServer* xmlrpc_server;

/* protected: */

/*     const DSMConfig* dsm; */

};

}

#endif
