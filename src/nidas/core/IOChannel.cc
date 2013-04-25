// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/Config.h>   // HAVE_BZLIB_H

#include <nidas/core/IOChannel.h>
#include <nidas/core/Socket.h>
#include <nidas/util/Process.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

IOChannel::IOChannel(): _dsm(0),_conInfo()
{
}

/* static */
IOChannel* IOChannel::createIOChannel(const xercesc::DOMElement* node)
            throw(n_u::InvalidParameterException)
{
    XDOMElement xnode(node);
    const string& elname = xnode.getNodeName();

    DOMable* domable;

    if (elname == "socket")
    	domable = Socket::createSocket(node);

    else if (elname == "fileset") {
	string classAttr = xnode.getAttributeValue("class");
	string fileAttr = n_u::Process::expandEnvVars(xnode.getAttributeValue("file"));
	if (classAttr.length() == 0) {
#ifdef HAVE_BZLIB_H
            if (fileAttr.find(".bz2") != string::npos) classAttr = "Bzip2FileSet";
            else classAttr = "FileSet";
#else
            if (fileAttr.find(".bz2") != string::npos) 
                throw n_u::InvalidParameterException(elname,fileAttr,"bzip2 compression/uncompression not supported");
            else classAttr = "FileSet";
#endif
        }
    	domable = DOMObjectFactory::createObject(classAttr);
    }
    else if (elname == "postgresdb")
    	domable = DOMObjectFactory::createObject("psql.PSQLChannel");

    else if (elname == "ncserver")
    	domable = DOMObjectFactory::createObject("isff.NcServerRPC");

    else if (elname == "goes") {
	string classAttr = xnode.getAttributeValue("class");
	if (classAttr.length() == 0) classAttr = "isff.SE_GOESXmtr";
	domable = DOMObjectFactory::createObject(classAttr);
    }
    else throw n_u::InvalidParameterException(
	    "IOChannel::createIOChannel","unknown element",elname);

    IOChannel* channel;
    if (!(channel = dynamic_cast<IOChannel*>(domable)))
	throw n_u::InvalidParameterException(
	    "IOChannel::createIOChannel",elname,"is not an IOChannel");
    return channel;
}
