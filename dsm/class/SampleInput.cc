/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

#include <SampleInput.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_ENTRY_POINT(SampleInputStream)

SampleInputStream::SampleInputStream():
    input(0),inputStream(0),pseudoPort(0),samp(0),left(0),dptr(0)
{
}

SampleInputStream::SampleInputStream(const SampleInputStream& x):
    input(x.input->clone()),inputStream(0),pseudoPort(x.pseudoPort),
    samp(0),left(0),dptr(0)
{
}

SampleInputStream::~SampleInputStream()
{
    delete inputStream;
    delete input;
}

SampleInput* SampleInputStream::clone() const
{
    // invoke copy constructor
    return new SampleInputStream(*this);
}

void SampleInputStream::requestConnection(atdUtil::SocketAccepter* accepter)
            throw(atdUtil::IOException)
{
    input->requestConnection(accepter,getPseudoPort());
}

void SampleInputStream::setPseudoPort(int val) { pseudoPort = val; }

int SampleInputStream::getPseudoPort() const { return pseudoPort; }

/*
 * pass on the offer, generous aren't we?
 */
void SampleInputStream::offer(atdUtil::Socket* sock) throw(atdUtil::IOException)
{
    input->offer(sock);
}

void SampleInputStream::init() throw(atdUtil::IOException)
{
    cerr << "SampleInputStream::init(), buffer size=" << 
    	input->getBufferSize() << endl;
    delete inputStream;
    inputStream = new InputStream(*input,input->getBufferSize());
}

void SampleInputStream::close() throw(atdUtil::IOException)
{
    if (inputStream) inputStream->close();
    else input->close();
}

int SampleInputStream::getFd() const
{
    if (input) return input->getFd();
    else return -1;
}

/**
 * Read a buffer of data and process all samples in the buffer.
 * This is typically used when a select has determined that there
 * is data available on our file descriptor. Process all available
 * data from the InputStream and distribute() samples to the receive()
 * method of my SampleClients.  This will perform only one physical
 * read of the underlying device.
 */
void SampleInputStream::readSamples() throw(dsm::SampleParseException,atdUtil::IOException)
{
    static int nsamps = 0;
    inputStream->read();
    SampleHeader header;
    for (;;) {
	if (!samp) {
#ifdef DEBUG
	    cerr << "available=" << inputStream->available() << endl;
#endif
	    if (inputStream->available() < header.getSizeOf()) break;
	    inputStream->read(&header,header.getSizeOf());

#ifdef DEBUG
	    cerr << "read header, type=" << header.getType() <<
	    	" getTimeTag=" << header.getTimeTag() <<
	    	" getId=" << header.getId() <<
	    	" getDataByteLength=" << header.getDataByteLength() <<
		endl;
#endif
	    if (header.getType() >= UNKNOWN_ST)
	        throw SampleParseException("sample type unknown");
	    samp = dsm::getSample((sampleType)header.getType(),
	    	header.getDataByteLength());
	    samp->setTimeTag(header.getTimeTag());
	    samp->setId(header.getId());
	    left = samp->getDataByteLength();
	    // cerr << "left=" << left << endl;
	    dptr = (char*) samp->getVoidDataPtr();
	}
	size_t len = inputStream->available();
	// cerr << "left=" << left << " available=" << len << endl;
	if (left < len) len = left;
	len = inputStream->read(dptr, len);
	// cerr << "read len=" << len << endl;
	dptr += len;
	left -= len;
	if (left == 0) {
	    if (!(nsamps++ % 100)) cerr << "read " << nsamps << " samples" << endl;
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
	if (header.getType() >= UNKNOWN_ST)
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

/*
 * process <input> element
 */
void SampleInputStream::fromDOMElement(const xercesc::DOMElement* node)
        throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    if(node->hasAttributes()) {
        // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
        }
    }

    // process <socket>, <fileset> child elements (should only be one)

    int ninputs = 0;
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& elname = xchild.getNodeName();
	input = Input::fromInputDOMElement((xercesc::DOMElement*)child);

	if (++ninputs > 1)
	    throw atdUtil::InvalidParameterException(
		    "SampleInputStream::fromDOMElement",
		    "input", "one and only one input allowed");
    }
    if (!input)
        throw atdUtil::InvalidParameterException(
                "SampleInputStream::fromDOMElement",
                "input", "no inputs specified");
}
                                                           
xercesc::DOMElement* SampleInputStream::toDOMParent(
    xercesc::DOMElement* parent)
    throw(xercesc::DOMException)
{
    xercesc::DOMElement* elem =
        parent->getOwnerDocument()->createElementNS(
                (const XMLCh*)XMLStringConverter("dsmconfig"),
                        DOMable::getNamespaceURI());
    parent->appendChild(elem);
    return toDOMElement(elem);
}
                                                                                
xercesc::DOMElement* SampleInputStream::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}

