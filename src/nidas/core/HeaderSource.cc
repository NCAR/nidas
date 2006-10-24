/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-06-05 17:56:34 -0600 (Sun, 05 Jun 2005) $

    $LastChangedRevision: 2212 $

    $LastChangedBy: maclean $

    $HeadURL: http://localhost:8080/svn/hiaper/ads3/dsm/class/SampleArchiver.cc $
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


