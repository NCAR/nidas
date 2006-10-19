/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/dynld/FileSet.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/core/Site.h>
#include <nidas/core/Project.h>

#include <nidas/util/Logger.h>

NIDAS_CREATOR_FUNCTION(FileSet);

using namespace nidas::dynld;
using namespace std;
using namespace xercesc;

namespace n_u = nidas::util;

/* Copy constructor. */
FileSet::FileSet(const FileSet& x):
    	IOChannel(x),nidas::util::FileSet(x),requester(0),mount(0)
{
    if (x.mount) mount = new FsMount(*x.mount);
}

const std::string& FileSet::getName() const
{
    return name;
}

void FileSet::setName(const std::string& val)
{
    name = val;
}

void FileSet::setFileName(const string& val)
{
    n_u::FileSet::setFileName(val);
    setName(string("FileSet: ") + getDir() + pathSeparator + getFileName());
}

IOChannel* FileSet::connect()
       throw(n_u::IOException)
{
    const SampleTag* stag = 0;
    if (getSampleTags().size() > 0) stag = *getSampleTags().begin();

    if (stag > 0) {
	// expand the file and directory names. We wait til now
	// because these may contain tokens that depend on 
	// our SampleTags.
	setDir(stag->expandString(getDir()));
	setFileName(stag->expandString(getFileName()));
	setName(string("FileSet: ") + getDir() + pathSeparator + getFileName());
    }
    else {
       n_u::Logger::getInstance()->log(LOG_WARNING,
       	"%s: no sample tags at connect time",
		getName().c_str());
    }

    // synchronous mount
    if (mount) mount->mount();
    return clone();
}

void FileSet::requestConnection(ConnectionRequester* rqstr)
       throw(n_u::IOException)
{
    // expand the file and directory names. We wait til now
    // because these may contain tokens that depend on the
    // SampleTag and we may not know then until now.
    const SampleTag* stag = 0;
    if (getSampleTags().size() > 0) stag = *getSampleTags().begin();

    if (stag > 0) {
	setDir(stag->expandString(getDir()));
	setFileName(stag->expandString(getFileName()));
	setName(string("FileSet: ") + getDir() + pathSeparator + getFileName());
    }
    else {
       n_u::Logger::getInstance()->log(LOG_WARNING,
       	"%s: no sample tags at requestConnection time",
		getName().c_str());
    }

    if (mount && !mount->isMounted()) {
	requester = rqstr;
	mount->mount(this);	// start mount request
	return;
    }
    rqstr->connected(this); 
    return;
}

void FileSet::mounted()
{
    cerr << "doing requester->connected..." << endl;
    if (mount && mount->isMounted()) requester->connected(this);
    cerr << "requester->connected done" << endl;
}

void FileSet::close() throw(n_u::IOException)
{
    if (mount) {
        mount->cancel();
	mount->unmount();
    }
    n_u::FileSet::closeFile();
}

dsm_time_t FileSet::createFile(dsm_time_t t,bool exact)
	throw(n_u::IOException)
{
    n_u::UTime ut(t);
    ut = n_u::FileSet::createFile(ut,exact);
    return ut.toUsecs();
}

void FileSet::fromDOMElement(const DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    // const string& elname = xnode.getNodeName();
    if(node->hasAttributes()) {
	// get all the attributes of the node
        DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
	    if (!aname.compare("dir")) setDir(aval);
	    else if (!aname.compare("file")) setFileName(aval);
	    else if (!aname.compare("length")) {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw n_u::InvalidParameterException(getName(),
			aname, aval);
		setFileLengthSecs(val);
	    }
	    else throw n_u::InvalidParameterException(getName(),
			"unrecognized attribute", aname);
	}
    }

    DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((DOMElement*) child);
	const string& elname = xchild.getNodeName();

	if (!elname.compare("mount")) {
	    mount = new FsMount();
	    mount->fromDOMElement((const DOMElement*) child);
	}
	else throw n_u::InvalidParameterException("mount",
		    "unrecognized child element", elname);
    }
}

DOMElement* FileSet::toDOMParent(
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

DOMElement* FileSet::toDOMElement(DOMElement* node)
    throw(DOMException)
{
    return node;
}

