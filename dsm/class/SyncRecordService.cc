
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <SyncRecordService.h>


using namespace dsm;
using namespace std;
using namespace xercesc;

#include <SocketAddress.h>

CREATOR_ENTRY_POINT(SyncRecordService)

SyncRecordService::SyncRecordService() :
	DSMService("SyncRecordService")
{
}

atdUtil::ServiceListenerClient* SyncRecordService::clone()
{
    return new SyncRecordService(*this);
}

int SyncRecordService::run() throw(atdUtil::Exception)
{
    inputStreams.front()->setSocket(socket);

    for (;;) {
        inputStreams.front()->readSamples();
    }
    return 0;
}

void SyncRecordService::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    DSMService::fromDOMElement(node);

    if (!inputStreams.size() != 1)
	throw atdUtil::InvalidParameterException(
		"SyncRecordService::fromDOMElement",
		"input", "only one input supported as of now");

    for (std::list<SampleOutputStream*>::iterator oi = outputStreams.begin();
    	oi != outputStreams.end(); ++oi)
	inputStreams.front()->addSampleClient(*oi);

    // call method of ServiceListenerClient base class so that
    // it knows the listen socket address
    setListenSocketAddress(inputStreams.front()->getSocketAddress());

}

DOMElement* SyncRecordService::toDOMParent(
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

DOMElement* SyncRecordService::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

