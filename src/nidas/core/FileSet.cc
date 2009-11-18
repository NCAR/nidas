/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-03-26 22:35:58 -0600 (Thu, 26 Mar 2009) $

    $LastChangedRevision: 4548 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/FileSet.cc $
 ********************************************************************

*/

#include <nidas/core/FileSet.h>

#include <nidas/core/DSMConfig.h>
#include <nidas/core/Site.h>
#include <nidas/core/Project.h>

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

FileSet::FileSet():  _fset(new nidas::util::FileSet()),
     _name("FileSet"),_requester(0),_mount(0) {}

FileSet::FileSet(n_u::FileSet* fset):
    _fset(fset),
    _name("FileSet"),_requester(0),_mount(0)
{
}

/* Copy constructor. */
FileSet::FileSet(const FileSet& x):
    	IOChannel(x),_fset(new n_u::FileSet(*x._fset)),
        _name(x._name),_requester(0),_mount(0)
{
    if (x._mount) _mount = new FsMount(*x._mount);
}

FileSet::~FileSet()
{
    delete _fset;
    delete _mount;
}

#ifdef TMP_NEEDED
void FileSet::setDSMConfig(const DSMConfig* val) 
{
    IOChannel::setDSMConfig(val);
    _fset->setFileName(val->expandString(_fset->getFileName()));
    _fset->setDir(val->expandString(_fset->getDir()));
    setName(string("FileSet: ") + _fset->getDir() + _fset->pathSeparator + _fset->getFileName());
#ifdef DEBUG
    cerr << "FileSet::setDSMConfig: " << getName() << endl;
#endif
}
#endif

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
	_fset->setFileName(getDSMConfig()->expandString(val));
    else if (Project::getInstance())
	_fset->setFileName(Project::getInstance()->expandString(val));
    else _fset->setFileName(val);
    setName(string("FileSet: ") + _fset->getDir() + _fset->pathSeparator + _fset->getFileName());
}

void FileSet::setDir(const string& val)
{
    if (getDSMConfig())
	_fset->setDir(getDSMConfig()->expandString(val));
    else if (Project::getInstance())
	_fset->setDir(Project::getInstance()->expandString(val));
    else _fset->setDir(val);
    setName(string("FileSet: ") + _fset->getDir() + _fset->pathSeparator + _fset->getFileName());
}

IOChannel* FileSet::connect()
       throw(n_u::IOException)
{
    // synchronous mount
    if (_mount) _mount->mount();
    return this;
}

void FileSet::requestConnection(IOChannelRequester* rqstr)
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
    _fset->closeFile();
    if (_mount) {
        _mount->cancel();
        _mount->unmount();
    }
}

dsm_time_t FileSet::createFile(dsm_time_t t,bool exact)
	throw(n_u::IOException)
{
    n_u::UTime ut(t);
    ut = _fset->createFile(ut,exact);
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

