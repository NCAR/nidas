// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "XMLConfigService.h"
#include <nidas/core/Project.h>
#include <nidas/core/Site.h>
#include <nidas/core/DSMServer.h>

#include <nidas/core/Datagrams.h>
#include <nidas/core/McSocket.h>

#include <nidas/core/XMLParser.h>
#include <nidas/core/XMLConfigWriter.h>
#include <nidas/core/XMLFdFormatTarget.h>

#include <nidas/util/Logger.h>
#include <nidas/util/Process.h>
#include <nidas/util/auto_ptr.h>

// #include <xercesc/dom/DOMElement.hpp>
#include <xercesc/dom/DOMNodeList.hpp>

#include <algorithm>
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

void XMLConfigService::schedule(bool /* optionalProcessing */)
{
    list<const SampleTag*> dummy;
    list<IOChannel*>::iterator oi = _ochans.begin();
    for ( ; oi != _ochans.end(); ++oi) {
        IOChannel* iochan = *oi;
        iochan->setRequestType(getRequestType());
        iochan->requestConnection(this);
    }
}

void XMLConfigService::interrupt() throw()
{
    DSMService::interrupt();    // call interrupt on workers
    list<IOChannel*>::iterator oi = _ochans.begin();
    for ( ; oi != _ochans.end(); ++oi) {
        IOChannel* iochan = *oi;
        iochan->close();
    }
}

IOChannelRequester* XMLConfigService::connected(IOChannel* iochan) throw()
{
    // Figure out what DSM it came from
    n_u::Inet4Address remoteAddr = iochan->getConnectionInfo().getRemoteSocketAddress().getInet4Address();
    DLOG(("findDSM, addr=") << remoteAddr.getHostAddress());

    string hostname = remoteAddr.getHostName();
    const DSMConfig* dsm = Project::getInstance()->findDSM(hostname);

    if (!dsm) {
        WLOG(("No match by name in config for DSM ") << hostname << " (" << remoteAddr.getHostAddress() << ")");
        dsm = Project::getInstance()->findDSM(remoteAddr);
        if (!dsm)
            PLOG(("No match by address in config for DSM ") << hostname << " (" << remoteAddr.getHostAddress() << ")");
        else
            NLOG(("Match by address to DSM ") << dsm->getName() << " in config for " << hostname << " (" << remoteAddr.getHostAddress() << ")");
    }
    else NLOG(("Match by name in config for DSM ") << hostname << " (" << remoteAddr.getHostAddress() << ")");

    // The iochan should be a new iochan, created from the configured
    // iochans, since it should be a newly connected Socket.
    // If it isn't then we have pointer ownership issues that must
    // resolved.
    list<IOChannel*>::iterator oi = std::find(_ochans.begin(),_ochans.end(),iochan);
    assert(oi == _ochans.end());

    // worker will own and delete the iochan.
    Worker* worker = new Worker(this,iochan,dsm);
    worker->start();
    addSubThread(worker);
    return this;
}

XMLConfigService::Worker::Worker(XMLConfigService* svc,IOChannel*iochan,
    const DSMConfig*dsm):
        Thread(svc->getName()), _svc(svc),_iochan(iochan),_dsm(dsm)
{
    unblockSignal(SIGUSR1);
}
XMLConfigService::Worker::~Worker()
{
    _iochan->close();
    delete _iochan;
}

void XMLConfigService::Worker::interrupt()
{
    try {
        kill(SIGUSR1);
    }
    catch (const n_u::Exception& e) {}
}

int XMLConfigService::Worker::run() 
{
    XMLCachingParser* parser = XMLCachingParser::getInstance();
    // This server has parsed the XML, but perhaps someone has changed it since then,
    // so we'll validate it.
    // Expand any XML includes, so the DSM gets the full XML.
    // Not sure whether all the other options are needed...
    parser->setDOMValidation(true);
    parser->setDOMValidateIfSchema(true);
    parser->setDOMNamespaces(true);
    parser->setXercesSchema(true);
    parser->setXercesSchemaFullChecking(true);
    parser->setXercesHandleMultipleImports(true);
    parser->setXercesDoXInclude(true);

    xercesc::DOMDocument* doc = parser->parse(
        n_u::Process::expandEnvVars(_svc->getDSMServer()->getXMLConfigFileName()));

    xercesc::DOMNodeList* projnodes =
        doc->getElementsByTagName((const XMLCh*)XMLStringConverter("project"));

    /**
     * Overwrite project config attribute with its value
     * from the Project.  The actual configuration name
     * typically comes from a runstring argument to DSMServerApp or DSMEngine.
     */
    for (unsigned int i = 0; i < projnodes->getLength(); i++) {
        xercesc::DOMNode* proj = projnodes->item(i);
        XDOMElement xnode((xercesc::DOMElement*)proj);
        xnode.setAttributeValue("config",
            Project::getInstance()->getConfigName());
    }
    // delete projnodes;

    XMLFdFormatTarget formatter(_iochan->getName(),_iochan->getFd());

    n_u::auto_ptr <XMLConfigWriter> writer;
    if (_dsm)
        writer.reset( new XMLConfigWriter(_dsm) );
    else
        writer.reset( new XMLConfigWriter() );

    XMLStringConverter convname(_iochan->getName());
    xercesc::DOMLSOutput *output;
    output = XMLImplementation::getImplementation()->createLSOutput();
    output->setByteStream(&formatter);
    output->setSystemId((const XMLCh*)convname);
    writer->writeNode(output,*doc);

    int ndsm = writer->getNumDSM();
    // Generally this should write one <dsm> node, or all of them.
    if (ndsm == 0)
        WLOG(("Wrote ") << ndsm << " <dsm> nodes to XMLConfigService output");
    else if (ndsm < 0)
        ILOG(("Wrote ") << " all <dsm> nodes to XMLConfigService output");
    else
        ILOG(("Wrote ") << ndsm << " <dsm> nodes to XMLConfigService output");

    output->release();

    _iochan->close();
    return RUN_OK;
}

