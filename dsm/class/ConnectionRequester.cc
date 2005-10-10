/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-06-05 17:56:34 -0600 (Sun, 05 Jun 2005) $

    $LastChangedRevision: 2212 $

    $LastChangedBy: maclean $

    $HeadURL: http://localhost:8080/svn/hiaper/ads3/dsm/class/SampleArchiver.cc $
 ********************************************************************
*/

#include <ConnectionRequester.h>
#include <SampleInputHeader.h>
#include <Version.h>
#include <Project.h>
#include <Site.h>

using namespace dsm;
using namespace std;

void SampleConnectionRequester::sendHeader(dsm_time_t thead,IOStream* iostream)
	throw(atdUtil::IOException)
{
    cerr << "ConnectionRequester::sendHeader" << endl;
    SampleInputHeader header;
    header.setArchiveVersion(Version::getArchiveVersion());
    header.setSoftwareVersion(Version::getSoftwareVersion());
    header.setProjectName(Project::getInstance()->getName());

    const Site* csite = Project::getInstance()->getCurrentSite();
    if (csite) header.setSiteName(csite->getName());
    else header.setSiteName("unknown");

    header.setObsPeriodName(Project::getInstance()->getCurrentObsPeriod().getName());
    header.setXMLName(Project::getInstance()->getXMLName());
    header.setXMLVersion(Project::getInstance()->getVersion());
    header.write(iostream);
}


