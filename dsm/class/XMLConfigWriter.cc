/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <XMLConfigWriter.h>
#include <XDOM.h>

#include <xercesc/dom/DOMImplementationLS.hpp>
#include <xercesc/dom/DOMNamedNodeMap.hpp>

#include <iostream>
#include <list>

using namespace dsm;
using namespace std;
using namespace xercesc;

XMLConfigWriter::XMLConfigWriter() throw (atdUtil::Exception): filter(0)
{

    impl = XMLImplementation::getImplementation();
    writer = ((DOMImplementationLS*)impl)->createDOMWriter();
}

XMLConfigWriter::XMLConfigWriter(atdUtil::Inet4Address dsmAddr)
	throw (atdUtil::Exception)
{

    impl = XMLImplementation::getImplementation();
    writer = ((DOMImplementationLS*)impl)->createDOMWriter();
    filter = new XMLConfigWriterFilter(dsmAddr);
    writer->setFilter(filter);
}


XMLConfigWriter::~XMLConfigWriter() 
{
    //
    //  Delete the writer.
    //
    delete filter;
    cerr << "writer release" << endl;
    writer->release();

}


void XMLConfigWriter::writeNode(XMLFormatTarget* const dest,
	const DOMNode& nodeToWrite)
    throw (atdUtil::IOException)
{
    writer->writeNode(dest,nodeToWrite);
}

XMLConfigWriterFilter::XMLConfigWriterFilter(atdUtil::Inet4Address dsm) : dsmAddr(dsm)
{
    setWhatToShow(
    	(1<<(DOMNode::ELEMENT_NODE-1)) | (1<<(DOMNode::DOCUMENT_NODE-1)));
}

short XMLConfigWriterFilter::acceptNode(const DOMNode* node) const
{
    XDOMElement xnode((DOMElement*)node);
    if ((getWhatToShow() & (1 << (node->getNodeType() - 1))) == 0) {
	cerr << "getWhatToShow() rejecting node " << xnode.getNodeName() <<endl;
	return DOMNodeFilter::FILTER_REJECT;
    }

    if (!xnode.getNodeName().compare("aircraft")) {
	// scan dsms of this aircraft. If we find a matching dsm
	// then pass this aircraft node on
	DOMNode* child;
	for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
	{
	    if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	    XDOMElement xchild((DOMElement*) child);
	    const string& elname = xchild.getNodeName();
	    // cerr << "element name=" << elname << endl;
	    if (!elname.compare("dsm") && acceptDSMNode(node))
	    	return DOMNodeFilter::FILTER_ACCEPT;
	}
	// dsm not found for this aircraft
	cerr << "rejecting aircraft node, name=" << xnode.getAttributeValue("name") << endl;
	return DOMNodeFilter::FILTER_REJECT;
    }
    else if (!xnode.getNodeName().compare("dsm")) {
        return acceptDSMNode(node);
    }
    return DOMNodeFilter::FILTER_ACCEPT;
}

short XMLConfigWriterFilter::acceptDSMNode(const DOMNode* node) const
{
    XDOMElement xnode((DOMElement*) node);
    if (xnode.getNodeName().compare("dsm"))
	return DOMNodeFilter::FILTER_REJECT;	// not a dsm node
    if(!node->hasAttributes()) 
	return DOMNodeFilter::FILTER_REJECT;	// no attribute

    DOMNamedNodeMap *pAttributes = node->getAttributes();
    int nSize = pAttributes->getLength();
    for(int i=0;i<nSize;++i) {
	XDOMAttr attr((DOMAttr*) pAttributes->item(i));
	// get attribute name
	const string& aname = attr.getName();
	const string& aval = attr.getValue();
	if (!aname.compare("name")) {
	    try {
		// grab all IP addresses for this name
		list<atdUtil::Inet4Address> addrs =
			atdUtil::Inet4Address::getAllByName(aval);
		
		list<atdUtil::Inet4Address>::const_iterator ai;
		for (ai = addrs.begin(); ai != addrs.end(); ++ai) 
		    if (*ai == dsmAddr) {
			cerr << "accepting dsm node, name=" << xnode.getAttributeValue("name") << endl;
			return DOMNodeFilter::FILTER_ACCEPT;	// match!
		    }
	    }
	    catch (const atdUtil::UnknownHostException& uhe) {}
	}
    }
    cerr << "rejecting dsm node, name=" << xnode.getAttributeValue("name") << endl;
    return DOMNodeFilter::FILTER_REJECT;	// no match
}

void XMLConfigWriterFilter::setWhatToShow(unsigned long val)
{
    whatToShow = val;
}

unsigned long XMLConfigWriterFilter::getWhatToShow() const
{
    return whatToShow;
}
