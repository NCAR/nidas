/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_SAMPLEFILEHEADER_H
#define NIDAS_CORE_SAMPLEFILEHEADER_H

#include <nidas/core/IOStream.h>

namespace nidas { namespace core {

class SampleInputHeader
{
public:

    void check(IOStream* iostream) throw(nidas::util::IOException);

    void write(SampleOutput* output) throw(nidas::util::IOException);

    void setArchiveVersion(const std::string& val) { archiveVersion = val; }
    const std::string& getArchiveVersion() const { return archiveVersion; }

    void setSoftwareVersion(const std::string& val) { softwareVersion = val; }
    const std::string& getSoftwareVersion() const { return softwareVersion; }
                                                                               
    void setProjectName(const std::string& val) { projectName = val; }
    const std::string& getProjectName() const { return projectName; }

    void setSystemName(const std::string& val) { systemName = val; }
    const std::string& getSystemName() const { return systemName; }

    void setConfigName(const std::string& val) { configName = val; }
    const std::string& getConfigName() const { return configName; }

    void setConfigVersion(const std::string& val) { configVersion = val; }
    const std::string& getConfigVersion() const { return configVersion; }

private:

    void setDummyString(const std::string& val) { }
    const std::string& getDummyString() const { return dummy; }

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

	bool obsolete;

    };

    static struct headerField headers[];

    std::string archiveVersion;

    std::string softwareVersion;

    std::string projectName;

    std::string systemName;

    std::string configName;

    std::string configVersion;

    std::string dummy;

    /**
     * Strings that can occur as the magic value at the beginning
     * of a sample file.
     * magicString[0] is the value written to new sample files.
     * The other strings, magicString[1] etc, are historic values
     * that may be found in existing sample files.
     */
    static const char* magicStrings[];

};

}}	// namespace nidas namespace core

#endif
