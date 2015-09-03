/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#include <nidas/core/HeaderSource.h>
#include <nidas/core/SampleInputHeader.h>
#include <nidas/core/Version.h>
#include <nidas/core/Project.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* static */
void HeaderSource::sendDefaultHeader(SampleOutput* output)
	throw(n_u::IOException)
{
    // cerr << "ConnectionRequester::sendHeader" << endl;
    SampleInputHeader header;
    header.setArchiveVersion(Version::getArchiveVersion());
    header.setSoftwareVersion(Version::getSoftwareVersion());
    header.setProjectName(Project::getInstance()->getName());

    string sysname = Project::getInstance()->getSystemName();
    header.setSystemName(sysname);

    header.setConfigName(Project::getInstance()->getConfigName());
    header.setConfigVersion(Project::getInstance()->getConfigVersion());
    header.write(output);
}


