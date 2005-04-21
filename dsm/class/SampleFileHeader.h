/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef DSM_SAMPLEFILEHEADER_H
#define DSM_SAMPLEFILEHEADER_H

#include <IOStream.h>

namespace dsm {

class SampleFileHeader
{
public:

    void check(IOStream* iostream) throw(atdUtil::IOException);

    void write(IOStream* iostream) throw(atdUtil::IOException);

    void setArchiveVersion(const std::string& val) { archiveVersion = val; }
    const std::string& getArchiveVersion() const { return archiveVersion; }
                                                                                
    void setSoftwareVersion(const std::string& val) { softwareVersion = val; }
    const std::string& getSoftwareVersion() const { return softwareVersion; }
                                                                                
    void setXMLName(const std::string& val) { xMLName = val; }
    const std::string& getXMLName() const { return xMLName; }
                                                                                
    void setXMLVersion(const std::string& val) { xMLVersion = val; }
    const std::string& getXMLVersion() const { return xMLVersion; }
                                                                                
    void setProjectName(const std::string& val) { projectName = val; }
    const std::string& getProjectName() const { return projectName; }

protected:
    std::string archiveVersion;
    std::string softwareVersion;
    std::string xMLName;
    std::string xMLVersion;
    std::string projectName;

};

}

#endif
