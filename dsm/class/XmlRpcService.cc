/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <Project.h>
#include <Site.h>

#include <XmlRpcService.h>
// #include <DSMServer.h>
#include <Datagrams.h>
// #include <XMLParser.h>
// #include <XMLConfigWriter.h>

#include <atdUtil/Logger.h>

#include <iostream>
#include <vector>

using namespace dsm;
using namespace std;
using namespace xercesc;
using namespace XmlRpc;

CREATOR_ENTRY_POINT(XmlRpcService)

// XMLRPC command to obtain a list of DSMs and their locations
class GetDsmList : public XmlRpcServerMethod
{
public:
  GetDsmList(XmlRpcServer* s) : XmlRpcServerMethod("getDsmList", s) {}

  void execute(XmlRpcValue& params, XmlRpcValue& result)
  {
    Project*                project  = Project::getInstance();
    const list<Site*>&      sitelist = project->getSites();
    Site*                   site     = sitelist.front();
    const list<DSMConfig*>& dsms     = site->getDSMConfigs();

    list<DSMConfig*>::const_iterator di;
    for (di = dsms.begin(); di != dsms.end(); ++di) {
      DSMConfig* dsm = *di;
      result[dsm->getName()] = dsm->getLocation();
    }
    cerr << "GetDsmList::execute " << result << endl;
  }
  std::string help() { return std::string("Say help getDsmList"); }
};

XmlRpcService::XmlRpcService():
	DSMService("XmlRpcService")
{
  cerr << "XmlRpcService::XmlRpcService ctor" << endl;
}

/*
 * Copy constructor.
 */
XmlRpcService::XmlRpcService(const XmlRpcService& x):
        DSMService((const DSMService&)x)
{
  cerr << "XmlRpcService::XmlRpcService copy ctor" << endl;
}

/*
 * clone myself by invoking copy constructor.
 */
DSMService* XmlRpcService::clone() const
{
    return new XmlRpcService(*this);
}

XmlRpcService::~XmlRpcService()
{
  cancel();
  join();
}

void XmlRpcService::schedule() throw(atdUtil::Exception)
{
  cerr << ">>>> XmlRpcService::schedule()" << endl;
  start();
}

int XmlRpcService::run() throw(atdUtil::Exception)
{
  cerr << ">>>> XmlRpcService::run" << endl;

  // Create an XMLRPC server
  xmlrpc_server = new XmlRpcServer;

  // These constructors register methods with the XMLRPC server
  GetDsmList getdsmlist (xmlrpc_server);
//   Start      start      (xmlrpc_server);
//   Stop       stop       (xmlrpc_server);
//   Restart    restart    (xmlrpc_server);
//   Quit       quit       (xmlrpc_server);

  // DEBUG - set verbosity of the xmlrpc server HIGH...
  XmlRpc::setVerbosity(5);

  // Create the server socket on the specified port
  xmlrpc_server->bindAndListen(DSM_XMLRPC_PORT);

  // Enable introspection
  xmlrpc_server->enableIntrospection(true);

  // Wait for requests indefinitely
  xmlrpc_server->work(-1.0);

  return RUN_OK;
}

void XmlRpcService::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
  cerr << "XmlRpcService::fromDOMElement" << endl;
//     int niochan = 0;
//     XDOMElement xnode(node);
//     DOMNode* child;
//     for (child = node->getFirstChild(); child != 0;
//             child=child->getNextSibling())
//     {
//         if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
//         XDOMElement xchild((DOMElement*) child);
//         const string& elname = xchild.getNodeName();

//         if (!elname.compare("output")) {
// 	    DOMNode* gkid;
// 	    for (gkid = child->getFirstChild(); gkid != 0;
// 		    gkid=gkid->getNextSibling())
// 	    {
// 		if (gkid->getNodeType() != DOMNode::ELEMENT_NODE) continue;

// 		iochan = IOChannel::createIOChannel((DOMElement*)gkid);
// 		iochan->fromDOMElement((DOMElement*)gkid);

// 		if (++niochan > 1)
// 		    throw atdUtil::InvalidParameterException(
// 			"XmlRpcService::fromDOMElement",
// 			"output", "must have one child element");
// 	    }
//         }
//         else throw atdUtil::InvalidParameterException(
//                 "XmlRpcService::fromDOMElement",
//                 elname, "unsupported element");
//     }
//     if (iochan == 0)
// 	throw atdUtil::InvalidParameterException(
// 	    "XmlRpcService::fromDOMElement",
// 	    "output", "one output required");
}

DOMElement* XmlRpcService::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    cerr << "XmlRpcService::toDOMParent" << endl;
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* XmlRpcService::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    cerr << "XmlRpcService::toDOMParent(node)" << endl;
    return node;
}
