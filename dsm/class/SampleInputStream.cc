/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#include <SampleInputStream.h>
#include <SocketAddress.h>
#include <DSMInputFileSet.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(SampleInputStream)

SampleInputStream::SampleInputStream():
    inputStream(0),samp(0),left(0),dptr(0)
{
}

SampleInputStream::~SampleInputStream()
{
    delete inputStream;
}

void SampleInputStream::setSocketAddress(atdUtil::Inet4SocketAddress& saddr)
{
    delete inputStream;
    socketAddress = saddr;
}
const atdUtil::Inet4SocketAddress& SampleInputStream::getSocketAddress() const
{
    return socketAddress;
}

void SampleInputStream::setSocket(atdUtil::Socket& sock)
{
    delete inputStream;
    inputStream = InputStreamFactory::createInputStream(sock);
}

void SampleInputStream::setFileSet(atdUtil::InputFileSet& fset)
{
    delete inputStream;
    inputStream = InputStreamFactory::createInputStream(fset);
}

/**
 * Read a buffer of data and process all samples in the buffer.
 * This
 * A select has determined that there is data available on
 * our file descriptor. Process all available data from the InputStream
 * and distribute() samples to the receive() method of my SampleClients.
 * This will perform only one physical read of the underlying device.
 */
void SampleInputStream::readSamples() throw(dsm::SampleParseException,atdUtil::IOException)
{
    inputStream->read();
    SampleHeader header;
    for (;;) {
	if (!samp) {
	    if (inputStream->available() < header.getSizeOf()) break;
	    inputStream->read(&header,header.getSizeOf());
	    if (header.getType() < 0 || header.getType() >= UNKNOWN_ST)
	        throw SampleParseException("sample type unknown");
	    samp = dsm::getSample((sampleType)header.getType(),
	    	header.getDataByteLength());
	    samp = 0;
	    samp->setTimeTag(header.getTimeTag());
	    samp->setId(header.getId());
	    left = samp->getDataByteLength();
	    dptr = (char*) samp->getVoidDataPtr();
	}
	size_t len = inputStream->available();
	if (left < len) len = left;
	len = inputStream->read(dptr, len);
	dptr += len;
	left -= len;
	if (left == 0) {
	    distribute(samp);
	    samp = 0;
	}
	else break;
    }
}

/**
 * Blocking read of the next sample from the buffer. The caller must
 * call freeReference on the sample when they're done with it.
 */
Sample* SampleInputStream::readSample() throw(SampleParseException,atdUtil::IOException)
{
    // user shouldn't mix the two read methods on one stream, but if they
    // do, checking for non-null samp here should make things work.
    if (!samp) {
	SampleHeader header;
	if (inputStream->available() < header.getSizeOf()) 
	    inputStream->read();

	inputStream->read(&header,header.getSizeOf());
	if (header.getType() < 0 || header.getType() >= UNKNOWN_ST)
	    throw SampleParseException("sample type unknown");
	samp = dsm::getSample((sampleType)header.getType(),
		header.getDataByteLength());
	samp = 0;
	samp->setTimeTag(header.getTimeTag());
	samp->setId(header.getId());
	left = samp->getDataByteLength();
	dptr = (char*) samp->getVoidDataPtr();
    }
    while (left > 0) {
	size_t len = inputStream->available();
	if (left < len) len = left;
	len = inputStream->read(dptr, len);
	dptr += len;
	left -= len;
	if (left == 0) break;
	inputStream->read();
    }
    Sample* tmp = samp;
    samp = 0;
    return tmp;
}
void SampleInputStream::fromDOMElement(const DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);

    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((DOMElement*) child);
        const string& cname = xchild.getNodeName();
	cerr << "SampleInputStream fromDOM, cname=" << cname << endl;

	if (!cname.compare("socket")) {
	    SocketAddress saddr;
	    saddr.fromDOMElement((DOMElement*)child);
	    setSocketAddress(saddr);
	}
	else if (!cname.compare("fileset")) {
	    DSMInputFileSet fset;
	    fset.fromDOMElement((DOMElement*)child);
	    setFileSet(fset);
	}
	else throw atdUtil::InvalidParameterException(
	    "SampleInputStream::fromDOMElement",cname,"tag not supported");
    }
}

DOMElement* SampleInputStream::toDOMParent(
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
                                                                                
DOMElement* SampleInputStream::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

