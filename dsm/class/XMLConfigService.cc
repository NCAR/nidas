
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <XMLConfigService.h>
#include <Aircraft.h>
// #include <DSMServer.h>

#include <Datagrams.h>

#include <XMLParser.h>
#include <XMLConfigWriter.h>
#include <XMLFdFormatTarget.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(XMLConfigService)

XMLConfigService::XMLConfigService():
	DSMService("XMLConfigService"),output(0)
{
}

/*
 * Copy constructor.
 */
XMLConfigService::XMLConfigService(const XMLConfigService& x):
        DSMService((const DSMService&)x),output(0)
{
    cerr << "XMLConfigService copy ctor, x.outout=" << hex << x.output << endl;
    if (x.output) output = x.output->clone();
}

XMLConfigService::~XMLConfigService()
{
    if (output) {
        output->close();
	delete output;
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
    output->requestConnection(this,XML_CONFIG);
}

void XMLConfigService::connected(IOChannel* output)
{
    // Figure out what DSM it came from
    atdUtil::Inet4Address remoteAddr = output->getRemoteInet4Address();
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
    getServer()->addThread(newserv);
}

int XMLConfigService::run() throw(atdUtil::Exception)
{
    XMLCachingParser* parser = XMLCachingParser::getInstance();

    xercesc::DOMDocument* doc = parser->parse(DSMServer::getXMLFileName());

    XMLFdFormatTarget formatter(output->getName(),output->getFd());

    XMLConfigWriter writer(getDSMConfig());
    writer.writeNode(&formatter,*doc);

    output->close();
    return RUN_OK;
}

void XMLConfigService::fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    int noutputs = 0;
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
		output = IOChannel::fromIOChannelDOMElement((xercesc::DOMElement*)gkid);
		if (++noutputs > 1)
		    throw atdUtil::InvalidParameterException(
			"XMLConfigService::fromDOMElement",
			"output", "one and only one output allowed");
	    }

        }
        else throw atdUtil::InvalidParameterException(
                "XMLConfigService::fromDOMElement",
                elname, "unsupported element");
    }
    if (noutputs == 0)
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

