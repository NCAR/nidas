/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <SampleOutput.h>
#include <DSMTime.h>

#include <iostream>

using namespace dsm;
using namespace std;

CREATOR_ENTRY_POINT(SampleOutputStream)

SampleOutputStream::SampleOutputStream():
	output(0),outputStream(0),pseudoPort(0),
	fullSampleTimetag(0),t0day(0),questionableTimetags(0)
{
}

SampleOutputStream::SampleOutputStream(const SampleOutputStream& x):
	output(x.output->clone()),outputStream(0),pseudoPort(x.pseudoPort),
	fullSampleTimetag(0),t0day(0),questionableTimetags(0)
{
}

SampleOutputStream::~SampleOutputStream()
{
    delete outputStream;
    delete output;
}

SampleOutput* SampleOutputStream::clone() const
{
    // invoke copy constructor
    return new SampleOutputStream(*this);
}

void SampleOutputStream::requestConnection(atdUtil::SocketAccepter* accepter)
	throw(atdUtil::IOException)
{
    output->requestConnection(accepter,getPseudoPort());
}

void SampleOutputStream::setPseudoPort(int val) { pseudoPort = val; }

int SampleOutputStream::getPseudoPort() const { return pseudoPort; }

/*
 * pass on the offer, generous aren't we?
 */
void SampleOutputStream::offer(atdUtil::Socket* sock) throw(atdUtil::IOException)
{
    output->offer(sock);
    init();
}

void SampleOutputStream::init() throw(atdUtil::IOException)
{
    delete outputStream;
    outputStream = new OutputStream(*output,output->getBufferSize());
}

void SampleOutputStream::close() throw(atdUtil::IOException)
{
    if (outputStream) outputStream->close();
    else output->close();
}

int SampleOutputStream::getFd() const
{
    if (output) return output->getFd();
    else return -1;
}

void SampleOutputStream::flush() throw(atdUtil::IOException)
{
    if (outputStream) outputStream->flush();
}

bool SampleOutputStream::receive(const Sample *samp)
         throw(SampleParseException, atdUtil::IOException)
{
    if (type == TIMETAG_DEPENDENT) {

	cerr << "samp->getId()=" << samp->getId() << ", shortId=" <<
		samp->getShortId() << endl;
	if (samp->getShortId() == CLOCK_SAMPLE_ID &&
		samp->getType() == LONG_LONG_ST &&
		samp->getDataLength() == 1) {
	    fullSampleTimetag = ((long long*)samp->getConstVoidDataPtr())[0];
	    t0day = timeFloor(fullSampleTimetag,MSECS_PER_DAY);
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

    write(samp);
}

size_t SampleOutputStream::write(const Sample* samp) throw(atdUtil::IOException)
{
    static int nsamps = 0;
    const void* bufs[2];
    size_t lens[2];
    bufs[0] = samp->getHeaderPtr();
    lens[0] = samp->getHeaderLength();

    bufs[1] = samp->getConstVoidDataPtr();
    lens[1] = samp->getDataByteLength();

    // cerr << "outputStream->write" << endl;
    if (!(nsamps++ % 100)) cerr << "wrote " << nsamps << " samples" << endl;
    return outputStream->write(bufs,lens,2);
}

void SampleOutputStream::fromDOMElement(const xercesc::DOMElement* node)
	throw(atdUtil::InvalidParameterException)
{
    XDOMElement xnode(node);
    const string& elname = xnode.getNodeName();
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

    int noutputs = 0;
    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
    {
        if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
        XDOMElement xchild((xercesc::DOMElement*) child);
        const string& cname = xchild.getNodeName();
	output = Output::fromOutputDOMElement((xercesc::DOMElement*)child);

        if (++noutputs > 1)
            throw atdUtil::InvalidParameterException(
                   "SampleOutputStream::fromDOMElement",
		   "output", "one and only one output allowed");
    }
    if (!output)
        throw atdUtil::InvalidParameterException(
                "SampleOutputStream::fromDOMElement",
                "output", "no outputs specified");
}

xercesc::DOMElement* SampleOutputStream::toDOMParent(
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
                                                                                
xercesc::DOMElement* SampleOutputStream::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}

