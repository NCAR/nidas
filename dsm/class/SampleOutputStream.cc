/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <SampleOutputStream.h>
#include <SocketAddress.h>
#include <DSMTime.h>

#include <iostream>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(SampleOutputStream)

SampleOutputStream::SampleOutputStream():
	outputStream(0),fullSampleTimetag(0),t0day(0),questionableTimetags(0)
{
}

SampleOutputStream::~SampleOutputStream()
{
    delete outputStream;
}

void SampleOutputStream::setSocketAddress(atdUtil::Inet4SocketAddress& saddr)
{
    delete outputStream;
    socketAddress = saddr;
}
const atdUtil::Inet4SocketAddress& SampleOutputStream::getSocketAddress() const
{
    return socketAddress;
}

void SampleOutputStream::setSocket(atdUtil::Socket& sock)
{
    delete outputStream;
    outputStream = OutputStreamFactory::createOutputStream(sock);
    type = SIMPLE;
}
                                                                                
void SampleOutputStream::setFileSet(atdUtil::OutputFileSet& fset)
{
    delete outputStream;
    outputStream = OutputStreamFactory::createOutputStream(fset);
    type = TIMETAG_DEPENDENT;
}

bool SampleOutputStream::receive(const Sample *samp)
         throw(SampleParseException, atdUtil::IOException)
{
    if (type == TIMETAG_DEPENDENT) {
	if (samp->getId() == CLOCK_SAMPLE_ID &&
		samp->getType() == LONG_LONG_ST &&
		samp->getDataLength() == 1) {
	    fullSampleTimetag = ((long long*)samp->getConstVoidDataPtr())[0];
	    t0day = timeFloor(fullSampleTimetag,86400000);
	}

	if (fullSampleTimetag == 0) return false;

	dsm_sys_time_t tsamp = t0day + samp->getTimeTag();

	/* midnight rollover */
	if (tsamp < (tsampLast - MSECS_PER_DAY / 2)) {
	    t0day += MSECS_PER_DAY;
	    tsamp += MSECS_PER_DAY;
	}
	if (abs(tsamp - fullSampleTimetag) > 60000) {
	    cerr << "questionable sample time" << endl;
	    questionableTimetags++;
	}
	else tsampLast = tsamp;

	if (nextFileTime == 0) nextFileTime = tsamp;

	if (tsamp >= nextFileTime)
	    nextFileTime = outputStream->createFile(nextFileTime);
    }
    if (!outputStream) return false;

    const void* bufs[2];
    size_t lens[2];
    bufs[0] = samp->getHeaderPtr();
    lens[0] = samp->getHeaderLength();

    bufs[1] = samp->getConstVoidDataPtr();
    lens[1] = samp->getDataByteLength();

    return outputStream->write(bufs,lens,2);
}

void SampleOutputStream::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    const string& elname = xnode.getNodeName();
    if(node->hasAttributes()) {
    // get all the attributes of the node
        DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	}
    }

    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((DOMElement*) child);
        const string& cname = xchild.getNodeName();

	if (!cname.compare("socket")) {
	    SocketAddress saddr;
	    saddr.fromDOMElement((DOMElement*)child);
	    setSocketAddress(saddr);
	}
	else if (!cname.compare("fileset")) {
	    DSMOutputFileSet fset;
	    fset.fromDOMElement((DOMElement*)child);
	    setFileSet(fset);
	}
	else throw atdUtil::InvalidParameterException(
	    "SampleOutputStream::fromDOMElement",cname,"tag not supported");
    }
}

DOMElement* SampleOutputStream::toDOMParent(
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
                                                                                
DOMElement* SampleOutputStream::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

