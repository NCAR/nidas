/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <FileSet.h>
#include <DSMConfig.h>
#include <Site.h>
#include <Project.h>

#include <atdUtil/Logger.h>

using namespace dsm;
using namespace std;
using namespace xercesc;

CREATOR_FUNCTION(FileSet)

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
    atdUtil::FileSet::setFileName(val);
    setName(string("FileSet: ") + getDir() + pathSeparator + getFileName());
}

IOChannel* FileSet::connect()
       throw(atdUtil::IOException)
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
       atdUtil::Logger::getInstance()->log(LOG_WARNING,
       	"%s: no sample tags at connect time",
		getName().c_str());
    }

    // synchronous mount
    if (mount) mount->mount();
    return clone();
}

void FileSet::requestConnection(ConnectionRequester* rqstr)
       throw(atdUtil::IOException)
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
       atdUtil::Logger::getInstance()->log(LOG_WARNING,
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

void FileSet::close() throw(atdUtil::IOException)
{
    if (mount) {
        mount->cancel();
	mount->unmount();
    }
    atdUtil::FileSet::closeFile();
}

dsm_time_t FileSet::createFile(dsm_time_t t,bool exact)
	throw(atdUtil::IOException)
{
    return (dsm_time_t)atdUtil::FileSet::createFile(
    	(time_t)(t/USECS_PER_SEC),exact) * USECS_PER_SEC;
}

void FileSet::fromDOMElement(const DOMElement* node)
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
	    if (!aname.compare("dir")) setDir(aval);
	    else if (!aname.compare("file")) setFileName(aval);
	    else if (!aname.compare("length")) {
		istringstream ist(aval);
		int val;
		ist >> val;
		if (ist.fail())
		    throw atdUtil::InvalidParameterException(getName(),
			aname, aval);
		setFileLengthSecs(val);
	    }
	    else throw atdUtil::InvalidParameterException(getName(),
			"unrecognized attribute", aname);
	}
    }

    DOMNode* child;
    DOMable* domable;
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
	else throw atdUtil::InvalidParameterException("mount",
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

