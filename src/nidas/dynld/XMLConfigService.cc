
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/dynld/XMLConfigService.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMServer.h>

#include <nidas/core/Datagrams.h>
#include <nidas/core/McSocket.h>

#include <nidas/core/XMLParser.h>
#include <nidas/core/XMLConfigWriter.h>
#include <nidas/core/XMLFdFormatTarget.h>

#include <nidas/util/Logger.h>

// #include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMNodeList.hpp>

#include <iostream>

using namespace nidas::core;
using namespace nidas::dynld;
using namespace std;

namespace n_u = nidas::util;

NIDAS_CREATOR_FUNCTION(XMLConfigService)

XMLConfigService::XMLConfigService():
	DSMService("XMLConfigService")
{
}

XMLConfigService::~XMLConfigService()
{
}

void XMLConfigService::schedule() throw(n_u::Exception)
{
    list<IOChannel*>::iterator oi = _outputs.begin();
    for ( ; oi != _outputs.end(); ++oi) {
        IOChannel* output = *oi;
        output->setRequestNumber(XML_CONFIG);
        output->requestConnection(this);
    }
}

void XMLConfigService::interrupt() throw()
{
    list<IOChannel*>::iterator oi = _outputs.begin();
    for ( ; oi != _outputs.end(); ++oi) {
        IOChannel* output = *oi;
        output->close();
    }
    DSMService::interrupt();
}
void XMLConfigService::connected(IOChannel* output) throw()
{
    // Figure out what DSM it came from
    n_u::Inet4Address remoteAddr = output->getRemoteInet4Address();
    DLOG(("findDSM, addr=") << remoteAddr.getHostAddress());
    const DSMConfig* dsm = Project::getInstance()->findDSM(remoteAddr);

    // perhaps the request came directly from one of my interfaces.
    // If so, see if there is a "localhost" dsm.
    if (!dsm) {
        nidas::core::McSocket* sock = dynamic_cast<nidas::core::McSocket*>(output);
        if (sock) {
            list<n_u::Inet4NetworkInterface> ifaces = sock->getInterfaces();
            list<n_u::Inet4NetworkInterface>::const_iterator ii = ifaces.begin();
            for ( ; !dsm && ii != ifaces.end(); ++ii) {
                n_u::Inet4NetworkInterface iface = *ii;
                // cerr << "iface=" << iface.getAddress().getHostAddress() << endl;
                if (iface.getAddress() == remoteAddr) {
                    remoteAddr = n_u::Inet4Address(INADDR_LOOPBACK);
                    dsm = Project::getInstance()->findDSM(remoteAddr);
                }
            }
        }
    }
    if (!dsm) {
        n_u::Logger::getInstance()->log(LOG_WARNING,
	    "can't find DSM for address %s" ,
	    remoteAddr.getHostAddress().c_str());
        output->close();
        delete output;
	return;
    }

    DLOG(("findDSM, dsm=") << dsm->getName());

    // The output should be a new output, created from the configured
    // outputs, since it should be a newly connected Socket.
    // If it isn't then we have pointer ownership issues that must
    // resolved.
    list<IOChannel*>::iterator oi = std::find(_outputs.begin(),_outputs.end(),output);
    assert(oi == _outputs.end());

    // worker will own and delete the output.
    Worker* worker = new Worker(this,output,dsm);
    worker->start();
    addSubThread(worker);
}

XMLConfigService::Worker::Worker(XMLConfigService* svc,IOChannel*output,
    const DSMConfig*dsm):
        Thread(svc->getName()), _svc(svc),_output(output),_dsm(dsm)
{
    blockSignal(SIGHUP);
    blockSignal(SIGINT);
    blockSignal(SIGTERM);
}
XMLConfigService::Worker::~Worker()
{
    _output->close();
    delete _output;
}
int XMLConfigService::Worker::run() throw(n_u::Exception)
{
    XMLCachingParser* parser = XMLCachingParser::getInstance();

    xercesc::DOMDocument* doc = parser->parse(
        Project::expandEnvVars(DSMServer::getInstance()->getXMLFileName()));

    xercesc::DOMNodeList* projnodes =
        doc->getElementsByTagName((const XMLCh*)XMLStringConverter("project"));

    /**
     * Overwrite project config attribute with its value
     * from the Project.  The actual configuration name
     * typically comes from a runstring argument to DSMServer or DSMEngine.
     */
    for (unsigned int i = 0; i < projnodes->getLength(); i++) {
        xercesc::DOMNode* proj = projnodes->item(i);
        XDOMElement xnode((xercesc::DOMElement*)proj);
        xnode.setAttributeValue("config",
            Project::getInstance()->getConfigName());
    }
    // delete projnodes;

    XMLFdFormatTarget formatter(_output->getName(),_output->getFd());

    XMLConfigWriter writer(_dsm);
    writer.writeNode(&formatter,*doc);

    _output->close();
    return RUN_OK;
}

void XMLConfigService::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    DSMService::fromDOMElement(node);

    if (_outputs.size() == 0)
	throw n_u::InvalidParameterException(
	    "XMLConfigService::fromDOMElement",
	    "output", "one or more outputs required");
}

