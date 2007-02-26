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

namespace n_u = nidas::util;

/* Copy constructor. */
FileSet::FileSet(const FileSet& x):
    	IOChannel(x),nidas::util::FileSet(x),
        expandedFileName(false),expandedDir(false),
        name(x.name),requester(0),mount(0)
{
    if (x.mount) mount = new FsMount(*x.mount);
}

void FileSet::setDSMConfig(const DSMConfig* val) 
{
    IOChannel::setDSMConfig(val);
    n_u::FileSet::setFileName(val->expandString(getFileName()));
    n_u::FileSet::setDir(val->expandString(getDir()));
    setName(string("FileSet: ") + getDir() + pathSeparator + getFileName());
#ifdef DEBUG
    cerr << "FileSet::setDSMConfig: " << getName() << endl;
#endif
}


const std::string& FileSet::getName() const
{
    if (getCurrentName().length() > 0) return getCurrentName();
    return name;
}

void FileSet::setName(const std::string& val)
{
    name = val;
}

void FileSet::setFileName(const string& val)
{
    if (getDSMConfig())
	n_u::FileSet::setFileName(getDSMConfig()->expandString(val));
    else if (Project::getInstance())
	n_u::FileSet::setFileName(Project::getInstance()->expandString(val));
    else n_u::FileSet::setFileName(val);
    setName(string("FileSet: ") + getDir() + pathSeparator + getFileName());
}

void FileSet::setDir(const string& val)
{
    if (getDSMConfig())
	n_u::FileSet::setDir(getDSMConfig()->expandString(val));
    else if (Project::getInstance())
	n_u::FileSet::setDir(Project::getInstance()->expandString(val));
    else n_u::FileSet::setDir(val);
    setName(string("FileSet: ") + getDir() + pathSeparator + getFileName());
}

IOChannel* FileSet::connect()
       throw(n_u::IOException)
{
    // synchronous mount
    if (mount) mount->mount();
    return clone();
}

void FileSet::requestConnection(ConnectionRequester* rqstr)
       throw(n_u::IOException)
{
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
    if (mount && mount->isMounted()) requester->connected(this);
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

void FileSet::fromDOMElement(const xercesc::DOMElement* node)
	throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    // const string& elname = xnode.getNodeName();
    if(node->hasAttributes()) {
	// get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
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

    xercesc::DOMNode* child;
    for (child = node->getFirstChild(); child != 0;
	    child=child->getNextSibling())
    {
	if (child->getNodeType() != xercesc::DOMNode::ELEMENT_NODE) continue;
	XDOMElement xchild((xercesc::DOMElement*) child);
	const string& elname = xchild.getNodeName();

	if (!elname.compare("mount")) {
	    mount = new FsMount();
	    mount->fromDOMElement((const xercesc::DOMElement*) child);
	}
	else throw n_u::InvalidParameterException("mount",
		    "unrecognized child element", elname);
    }
}

