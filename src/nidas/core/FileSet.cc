/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "FileSet.h"
#include "Bzip2FileSet.h"

#include "DSMConfig.h"
#include "Site.h"
#include "Project.h"

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
    	IOChannel(x),_fset(x._fset->clone()),
        _name(x._name),_requester(0),_mount(0)
{
    if (x._mount) _mount = new FsMount(*x._mount);
}

FileSet::~FileSet()
{
    delete _fset;
    delete _mount;
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

void FileSet::setFileName(const std::string& val)
{
    if (getDSMConfig())
	_fset->setFileName(getDSMConfig()->expandString(val));
    else if (Project::getInstance())
	_fset->setFileName(Project::getInstance()->expandString(val));
    else _fset->setFileName(val);
    setName(string("FileSet: ") + _fset->getPath());
}

void FileSet::setDir(const std::string& val)
{
    if (getDSMConfig())
	_fset->setDir(getDSMConfig()->expandString(val));
    else if (Project::getInstance())
	_fset->setDir(Project::getInstance()->expandString(val));
    else _fset->setDir(val);
    setName(string("FileSet: ") + _fset->getPath());
}

IOChannel* FileSet::connect()
{
    // synchronous mount
    if (_mount) _mount->mount();
    return this;
}

void FileSet::requestConnection(IOChannelRequester* rqstr)
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

void FileSet::close()
{
    _fset->closeFile();
    if (_mount) {
        _mount->cancel();
        _mount->unmount();
    }
}

dsm_time_t FileSet::createFile(dsm_time_t t, bool exact)
{
    n_u::UTime ut(t);
    ut = _fset->createFile(ut,exact);
    return ut.toUsecs();
}

void FileSet::fromDOMElement(const xercesc::DOMElement* node)
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
	    else if (aname == "compress");
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

/* static */
FileSet* FileSet::getFileSet(const list<string>& filenames)
{
    int bzFile = -1;
    list<string>::const_iterator fi = filenames.begin();
    for ( ; fi != filenames.end(); ++fi) {
        if (fi->find(".bz2") != string::npos) {
            if (bzFile == 0)
                throw n_u::InvalidParameterException(*fi,"open","cannot mix bzipped and non-bzipped files");
#ifdef HAVE_BZLIB_H
            bzFile = 1;
#else
            throw n_u::InvalidParameterException(*fi,"open","bzip2 compression/uncompression not supported. If you want it, install bzip2-devel, and rebuild with scons --config=force");
#endif
        }
        else {
            if (bzFile == 1)
                throw n_u::InvalidParameterException(*fi,"open","cannot mix bzipped and non-bzipped files");
            bzFile = 0;
        }
    }
    FileSet* fset;
#ifdef HAVE_BZLIB_H
    if (bzFile == 1) fset = new Bzip2FileSet();
    else fset = new FileSet();
#else
    fset = new FileSet();
#endif

    fi = filenames.begin();
    for ( ; fi != filenames.end(); ++fi)
        fset->addFileName(*fi);
    return fset;
}


/* static */
FileSet* 
FileSet::createFileSet(const std::string& filename)
{
    FileSet* fset = 0;
    if (filename.find(".bz2") != string::npos)
    {
#ifdef HAVE_BZLIB_H
        fset = new nidas::core::Bzip2FileSet();
#else
        throw n_u::InvalidParameterException(filename, "open",
            "bzip2 compression/uncompression not available.");
#endif
    }
    else
    {
        fset = new nidas::core::FileSet();
    }
    fset->setFileName(filename);
    return fset;
}
