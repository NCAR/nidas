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
        _name(x._name),_requester(0),_mount(0)
{
    if (x._mount) _mount = new FsMount(*x._mount);
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
    return _name;
}

void FileSet::setName(const std::string& val)
{
    _name = val;
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
    if (_mount) _mount->mount();
    return clone();
}

void FileSet::requestConnection(ConnectionRequester* rqstr)
       throw(n_u::IOException)
{
    if (_mount && !_mount->isMounted()) {
	_requester = rqstr;
	_mount->mount(this);	// start mount request
	return;
    }
    rqstr->connected(this); 
    return;
}

void FileSet::mounted()
{
    if (_mount && _mount->isMounted()) _requester->connected(this);
}

void FileSet::close() throw(n_u::IOException)
{
    if (_mount) {
        _mount->cancel();
	_mount->unmount();
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
	    if (aname == "dir") setDir(aval);
	    else if (aname == "file") setFileName(aval);
	    else if (aname == "length") {
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

	if (elname == "mount") {
	    _mount = new FsMount();
	    _mount->fromDOMElement((const xercesc::DOMElement*) child);
	}
	else throw n_u::InvalidParameterException("mount",
		    "unrecognized child element", elname);
    }
}

