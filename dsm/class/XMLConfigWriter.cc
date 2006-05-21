/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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

#ifdef NEEDED
XMLConfigWriter::XMLConfigWriter() throw (dsm::XMLException): filter(0)
{
    impl = XMLImplementation::getImplementation();
    writer = ((DOMImplementationLS*)impl)->createDOMWriter();
}
#endif

XMLConfigWriter::XMLConfigWriter(const DSMConfig* dsm)
	throw (dsm::XMLException)
{

    impl = XMLImplementation::getImplementation();
    writer = ((DOMImplementationLS*)impl)->createDOMWriter();
    filter = new XMLConfigWriterFilter(dsm);
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

XMLConfigWriterFilter::XMLConfigWriterFilter(const DSMConfig* dsmarg):
	dsm(dsmarg)
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

    string nodename = xnode.getNodeName();

    if (!nodename.compare("aircraft") || !nodename.compare("site")) {
	// scan dsms of this aircraft/site. If we find a matching dsm
	// then pass this aircraft/site node on
	DOMNode* child;
	for (child = node->getFirstChild(); child != 0;
            child=child->getNextSibling())
	{
	    if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	    XDOMElement xchild((DOMElement*) child);
	    if (!xchild.getNodeName().compare("dsm") &&
	    	acceptDSMNode(child) == DOMNodeFilter::FILTER_ACCEPT)
	    	return DOMNodeFilter::FILTER_ACCEPT;
	}
	// dsm not found for this aircraft/site
	cerr << "rejecting " << nodename << " node, name=" <<
		xnode.getAttributeValue("name") << endl;
	return DOMNodeFilter::FILTER_REJECT;
    }
    else if (!xnode.getNodeName().compare("dsm"))
        return acceptDSMNode(node);
    else if (!xnode.getNodeName().compare("server"))
	return DOMNodeFilter::FILTER_REJECT;
    return DOMNodeFilter::FILTER_ACCEPT;
}

short XMLConfigWriterFilter::acceptDSMNode(const DOMNode* node) const
{
    XDOMElement xnode((DOMElement*) node);
    if (xnode.getNodeName().compare("dsm"))
	return DOMNodeFilter::FILTER_REJECT;	// not a dsm node
    if(!node->hasAttributes()) 
	return DOMNodeFilter::FILTER_REJECT;	// no attribute

    const string& dsmName = xnode.getAttributeValue("name");
    if (!dsmName.compare(dsm->getName())) {
	cerr << "accepting dsm node, name=" << dsmName << endl;
	return DOMNodeFilter::FILTER_ACCEPT;
    }
    cerr << "rejecting dsm node, name=" << dsmName << endl;
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
