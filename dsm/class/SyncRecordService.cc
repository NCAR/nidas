
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
    return 0;
}

void SyncRecordService::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    if(node->hasAttributes()) {
    // get all the attributes of the node
        DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((DOMAttr*) pAttributes->item(i));
            // get attribute name
            const string& aname = attr.getName();
            const string& aval = attr.getValue();
	}
    }
    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((DOMElement*) child);
        const string& elname = xchild.getNodeName();

       if (!elname.compare("socket")) {
	    SocketAddress saddr;
	    saddr.fromDOMElement((DOMElement*)child);
	    setListenSocketAddress(saddr);
	}
    }
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

