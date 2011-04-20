
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_RAF_SYNCRECORDREADER_H
#define NIDAS_DYNLD_RAF_SYNCRECORDREADER_H

#include <nidas/dynld/SampleInputStream.h>
#include <nidas/core/SampleTag.h>
#include <nidas/dynld/raf/SyncRecordVariable.h>

#include <nidas/util/Thread.h>

namespace nidas { namespace dynld { namespace raf {

class SyncRecHeaderException: public nidas::util::Exception 
{
public:
    SyncRecHeaderException(const std::string& expect, const std::string& got):
    	nidas::util::Exception("SyncRecHeaderException",
		std::string("expected: \"") + expect +
		"\", got: \"" + got + "\"")
    {
    }
    SyncRecHeaderException(const std::string& msg):
    	nidas::util::Exception("SyncRecHeaderException",msg)
    {
    }

    /**
     * Copy constructor.
     */
    SyncRecHeaderException(const SyncRecHeaderException& x):
    	nidas::util::Exception(x)
    {
    }
};

class SyncRecordReader
{
public:

    /**
     * Constructor of a SyncRecordReader to a connected IOChannel.
     * SyncRecordReader will own the IOChannel pointer and
     * will delete it when done.
     */
    SyncRecordReader(IOChannel* iochan);

    virtual ~SyncRecordReader();

    const std::string& getProjectName() const { return projectName; }

    const std::string& getTailNumber() const { return aircraftName; }

    const std::string& getFlightName() const { return flightName; }

    const std::string& getSoftwareVersion() const { return softwareVersion; }

    /**
     * Get UNIX time of the start time of data in the SyncRecords.
     */
    time_t getStartTime() const { return startTime; }

    /**
     * Get the list of variables in a sync record.
     */
    const std::list<const SyncRecordVariable*> getVariables()
    	throw(nidas::util::Exception);

    /**
     * Get a pointer to a SyncRecordVariable, searching by name.
     * Returns NULL if not found.
     */
    const SyncRecordVariable* getVariable(const std::string& name) const;

    /**
     * Get number of data values in a sync record.  This includes data and
     * dynamic lag values.
     */
    size_t getNumValues() const { return numDataValues; }

    /**
     * Read a sync record.
     * @param tt Pointer to a dsm_time_t variable to store the
     *           sync record time tag (microseconds since 1970 Jan 1 00:: GMT).
     * @param ptr Pointer to the array which the caller has allocated.
     * @param len Number of values to read. Use getNumValues() to find
     *            out the number of values in a sync record.
     * @returns @p len on success or 0 on eof or failure.
     */
    size_t read(dsm_time_t* tt, double *ptr,size_t len) throw(nidas::util::IOException);

private:

    void scanHeader(const Sample* samp) throw();

    SampleInputStream inputStream;

    std::string getQuotedString(std::istringstream& str);
    
    void readKeyedValues(std::istringstream& header)
    	throw(SyncRecHeaderException);

    SyncRecHeaderException* headException;

    std::list<SampleTag*> sampleTags;

    std::list<const SyncRecordVariable*> variables;

    std::map<std::string,const SyncRecordVariable*> variableMap;

    size_t numDataValues;

    std::string projectName;

    std::string aircraftName;

    std::string flightName;

    std::string softwareVersion;

    time_t startTime;

    bool _debug;
};

}}}	// namespace nidas namespace dynld namespace raf

#endif
