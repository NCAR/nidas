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

class SampleInputHeader
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

    void setSiteName(const std::string& val) { siteName = val; }
    const std::string& getSiteName() const { return siteName; }

    void setObsPeriodName(const std::string& val) { obsPeriodName = val; }
    const std::string& getObsPeriodName() const { return obsPeriodName; }


protected:

    struct headerField {

	/* a tag in the file header */
	const char* tag;

	/* ptr to setXXX member function for setting an attribute of this
	 * class, based on the value of the tag from the IOStream.
	 */
	void (SampleInputHeader::* setFunc)(const std::string&);

	/* ptr to getXXX member function for getting an attribute of this
	 * class, in order to write the value of the tag to the IOStream.
	 */
	const std::string& (SampleInputHeader::* getFunc)() const;

    };

    static struct headerField headers[];

    std::string archiveVersion;
    std::string softwareVersion;
    std::string xMLName;
    std::string xMLVersion;
    std::string projectName;
    std::string siteName;
    std::string obsPeriodName;

};

}

#endif
