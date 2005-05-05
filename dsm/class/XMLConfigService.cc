
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <XMLConfigService.h>
#include <Aircraft.h>
// #include <DSMServer.h>

#include <Datagrams.h>

#include <XMLParser.h>
#include <XMLConfigWriter.h>
#include <XMLFdFormatTarget.h>

#include <atdUtil/Logger.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(XMLConfigService)

XMLConfigService::XMLConfigService():
	DSMService("XMLConfigService"),iochan(0),dsm(0)
{
}

/*
 * Copy constructor.
 */
XMLConfigService::XMLConfigService(const XMLConfigService& x):
        DSMService((const DSMService&)x),iochan(0),dsm(x.dsm)
{
    // cerr << "XMLConfigService copy ctor, x.outout=" << hex << x.iochan << endl;
    if (x.iochan) iochan = x.iochan->clone();
}

XMLConfigService::~XMLConfigService()
{
    if (iochan) {
        iochan->close();
	delete iochan;
    }
}

/*
 * clone myself
 */
DSMService* XMLConfigService::clone() const
{
    // invoke copy constructor.
    XMLConfigService* newserv = new XMLConfigService(*this);
    return newserv;
}

void XMLConfigService::schedule() throw(atdUtil::Exception)
{
    iochan->requestConnection(this,XML_CONFIG);
}

void XMLConfigService::connected(IOChannel* iochan) throw()
{
    // Figure out what DSM it came from
    atdUtil::Inet4Address remoteAddr = iochan->getRemoteInet4Address();
    cerr << "findDSM, addr=" << remoteAddr.getHostAddress() << endl;
    const DSMConfig* dsm = getAircraft()->findDSM(remoteAddr);
    if (!dsm)
	throw atdUtil::Exception(string("can't find DSM for address ") +
	    remoteAddr.getHostAddress());

    cerr << "findDSM, dsm=" << dsm->getName() << endl;
    // make a copy of myself, assign it to a specific dsm
    XMLConfigService* newserv = new XMLConfigService(*this);
    newserv->setDSMConfig(dsm);
    newserv->start();

    addSubService(newserv);
}

int XMLConfigService::run() throw(atdUtil::Exception)
{
    XMLCachingParser* parser = XMLCachingParser::getInstance();

    DOMDocument* doc = parser->parse(DSMServer::getXMLFileName());

    XMLFdFormatTarget formatter(iochan->getName(),iochan->getFd());

    XMLConfigWriter writer(getDSMConfig());
    writer.writeNode(&formatter,*doc);

    iochan->close();
    return RUN_OK;
}

void XMLConfigService::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    int niochan = 0;
    XDOMElement xnode(node);
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((DOMElement*) child);
        const string& elname = xchild.getNodeName();

        if (!elname.compare("output")) {
	    DOMNode* gkid;
	    for (gkid = child->getFirstChild(); gkid != 0;
		    gkid=gkid->getNextSibling())
	    {
		if (gkid->getNodeType() != DOMNode::ELEMENT_NODE) continue;
		XDOMElement xgkid((DOMElement*) gkid);

		const string& gkidname = xgkid.getNodeName();

		iochan = IOChannel::createIOChannel(gkidname);
		// iochan->setDSMService(this);
		iochan->fromDOMElement((DOMElement*)gkid);

		if (++niochan > 1)
		    throw atdUtil::InvalidParameterException(
			"XMLConfigService::fromDOMElement",
			"output", "must have one child element");
	    }

        }
        else throw atdUtil::InvalidParameterException(
                "XMLConfigService::fromDOMElement",
                elname, "unsupported element");
    }
    if (iochan == 0)
	throw atdUtil::InvalidParameterException(
	    "XMLConfigService::fromDOMElement",
	    "output", "one output required");
}

DOMElement* XMLConfigService::toDOMParent(
    DOMElement* parent)
    throw(DOMException)
{
    DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
			DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}

DOMElement* XMLConfigService::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

