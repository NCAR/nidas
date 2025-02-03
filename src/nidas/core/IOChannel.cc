// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
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

#include <nidas/Config.h>   // HAVE_BZLIB_H

#include "IOChannel.h"
#include "Socket.h"
#include <nidas/util/Process.h>
#include "SampleTag.h"

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

IOChannel::IOChannel(): _dsm(0),_conInfo()
{
}

/* static */
IOChannel* IOChannel::createIOChannel(const xercesc::DOMElement* node)
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
                throw n_u::InvalidParameterException(elname,fileAttr,"bzip2 compression/uncompression not supported. If you want it, install bzip2-devel, and rebuild with scons --config=force");
            else classAttr = "FileSet";
#endif
        }
    	domable = DOMObjectFactory::createObject(classAttr);
    }
    else throw n_u::InvalidParameterException(
        "IOChannel::createIOChannel", "unknown element", elname);

    IOChannel* channel;
    if (!(channel = dynamic_cast<IOChannel*>(domable)))
        throw n_u::InvalidParameterException(
            "IOChannel::createIOChannel", elname, "is not an IOChannel");
    return channel;
}


dsm_time_t
IOChannel::createFile(dsm_time_t /*t*/, bool /*exact*/)
{
    return LONG_LONG_MAX;
}


void
IOChannel::addSampleTag(const nidas::core::SampleTag*)
{}
