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

#ifndef NIDAS_CORE_SAMPLEFILEHEADER_H
#define NIDAS_CORE_SAMPLEFILEHEADER_H

#include <nidas/core/IOStream.h>
#include <nidas/core/SampleOutput.h>
#include <nidas/util/ParseException.h>

namespace nidas { namespace core {

class SampleInputHeader
{
public:

    SampleInputHeader();

    /**
     * Copy constructor. Should not be used when a header
     * is being parsed.
     */
    SampleInputHeader(const SampleInputHeader&);

    /**
     * Assignment operator. Should not be used when a header
     * is being parsed.
     */
    SampleInputHeader& operator=(const SampleInputHeader&);

    ~SampleInputHeader();

    /**
     * Read IOStream until the SampleInputHeader has been
     * fully read.  This will perform one or more
     * iostream->read()s.
     */
    void read(IOStream* iostream) throw(nidas::util::IOException);

    /**
     * Parse the current contents of the IOStream for the
     * SampleInputHeader.
     * @return true: SampleInputHeader has been fully parsed.
     *      false: SampleInputHeader not completely parsed yet,
     *          another iostream->read() is necessary.
     */
    bool parse(IOStream* iostream) throw(nidas::util::ParseException);

    /**
     * Get length in bytes of the header.
     */
    int getLength() const { return _size; }

    /**
     * Render the header in string form.
     */
    std::string toString() const;

    size_t write(SampleOutput* output) const throw(nidas::util::IOException);

    size_t write(IOStream* iostream) const throw(nidas::util::IOException);

    void setArchiveVersion(const std::string& val) { _archiveVersion = val; }
    const std::string& getArchiveVersion() const { return _archiveVersion; }

    void setSoftwareVersion(const std::string& val) { _softwareVersion = val; }
    const std::string& getSoftwareVersion() const { return _softwareVersion; }
                                                                               
    void setProjectName(const std::string& val) { _projectName = val; }
    const std::string& getProjectName() const { return _projectName; }

    void setSystemName(const std::string& val) { _systemName = val; }
    const std::string& getSystemName() const { return _systemName; }

    void setConfigName(const std::string& val) { _configName = val; }
    const std::string& getConfigName() const { return _configName; }

    void setConfigVersion(const std::string& val) { _configVersion = val; }
    const std::string& getConfigVersion() const { return _configVersion; }

protected:

    bool parseMagic(IOStream* iostream) throw(nidas::util::ParseException);

    bool parseTag(IOStream* iostream) throw(nidas::util::ParseException);

    bool parseValue(IOStream* iostream) throw(nidas::util::ParseException);

private:

    // Set method that does nothing.
    void setDummyString(const std::string&) { }

    const std::string& getDummyString() const { return _dummy; }

    struct headerField {

	/* a tag in the file header */
	const char* tag;

        int taglen;

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

    static const struct headerField headers[];

    std::string _archiveVersion;

    std::string _softwareVersion;

    std::string _projectName;

    std::string _systemName;

    std::string _configName;

    std::string _configVersion;

    std::string _dummy;

    /**
     * Strings that can occur as the magic value at the beginning
     * of a sample file.
     * magicString[0] is the value written to new sample files.
     * The other strings, magicString[1] etc, are historic values
     * that may be found in existing sample files.
     */
    static const char* magicStrings[];

    static const int _nmagic;

    int _minMagicLen;

    int _imagic;

    static const int _ntags;

    int _endTag;

    int _tagMatch;

    /**
     * Size in bytes of the header. The value is saved, so that if we
     * update one or more values, then we might be able to re-write
     * the header without exceeding the original size.
     */
    int _size;

    static const int HEADER_BUF_LEN;

    char* _buf;

    char* _headPtr;

    enum parseStage {PARSE_START, PARSE_MAGIC, PARSE_TAG, PARSE_VALUE, PARSE_DONE} _stage;

};

}}	// namespace nidas namespace core

#endif
