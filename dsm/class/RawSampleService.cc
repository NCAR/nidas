
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <RawSampleService.h>
#include <SocketAddress.h>

#include <DOMObjectFactory.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(RawSampleService)

RawSampleService::RawSampleService():
	DSMService("RawSampleService")
{
}

atdUtil::ServiceListenerClient* RawSampleService::clone()
{
    return new RawSampleService(*this);
}

int RawSampleService::run() throw(atdUtil::Exception)
{
    inputStreams.front()->setSocket(socket);

    for (;;) {
        inputStreams.front()->readSamples();
    }
    return 0;
}

void RawSampleService::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    DSMService::fromDOMElement(node);

    if (inputStreams.size() != 1)
	throw atdUtil::InvalidParameterException(
		"RawSampleService::fromDOMElement",
		"input", "one and only one input allowed");

    // This is a simple service that just echoes the input
    // samples to the outputs
    for (std::list<SampleOutputStream*>::iterator oi = outputStreams.begin();
    	oi != outputStreams.end(); ++oi)
	inputStreams.front()->addSampleClient(*oi);

    // call method of ServiceListenerClient base class so that
    // it knows the listen socket address
    setListenSocketAddress(inputStreams.front()->getSocketAddress());

}

DOMElement* RawSampleService::toDOMParent(
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

DOMElement* RawSampleService::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

