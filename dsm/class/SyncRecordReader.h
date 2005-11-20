
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-06-01 15:27:18 -0600 (Wed, 01 Jun 2005) $

    $LastChangedRevision: 2169 $

    $LastChangedBy: maclean $

    $HeadURL: http://localhost:8080/svn/hiaper/ads3/dsm/class/SyncRecordProcessor.h $
 ********************************************************************

*/

#ifndef DSM_SYNCRECORDREADER_H
#define DSM_SYNCRECORDREADER_H

#include <SampleInput.h>
#include <SampleTag.h>
#include <SyncRecordVariable.h>

#include <atdUtil/Thread.h>

#include <semaphore.h>

namespace dsm {

class SyncRecHeaderException: public atdUtil::Exception 
{
public:
    SyncRecHeaderException(const std::string& expect, const std::string& got):
    	atdUtil::Exception("SyncRecHeaderException",
		std::string("expected: \"") + expect +
		"\", got: \"" + got + "\"")
    {
    }
    SyncRecHeaderException(const std::string& msg):
    	atdUtil::Exception("SyncRecHeaderException",msg)
    {
    }

    /**
     * Copy constructor.
     */
    SyncRecHeaderException(const SyncRecHeaderException& x):
    	atdUtil::Exception(x)
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

    /**
     * Get the list of variables in a sync record.
     */
    const std::list<const SyncRecordVariable*> getVariables()
    	throw(atdUtil::Exception);

    /**
     * Get a pointer to a SyncRecordVariable, searching by name.
     * Returns NULL if not found.
     */
    const SyncRecordVariable* getVariable(const std::string& name) const;

    /**
     * Get number of floats in a sync record.
     */
    size_t getNumFloats() const { return numFloats; }

    /**
     * Read a sync record.
     * @param tt Pointer to a dsm_time_t variable to store the
     *           sync record time tag (microseconds since 1970 Jan 1 00:: GMT).
     * @param ptr Pointer to the float array which the caller has allocated.
     * @param len Number of floats to read. Use getNumFloats() to find
     *            out the number of floats in a sync record.
     * @returns @p len on success or 0 on eof or failure.
     */
    size_t read(dsm_time_t* tt, float *ptr,size_t len) throw(atdUtil::IOException);

private:

    void scanHeader(const Sample* samp) throw();

    SampleInputStream inputStream;

    std::string getQuotedString(std::istringstream& str);
    
    std::string getKeyedValue(std::istringstream& header,const std::string& key)
    	throw(SyncRecHeaderException);

    SyncRecHeaderException* headException;

    std::list<SampleTag*> sampleTags;

    std::list<const SyncRecordVariable*> variables;

    std::map<std::string,const SyncRecordVariable*> variableMap;

    size_t numFloats;

    std::string projectName;

    std::string aircraftName;

    std::string flightName;

};

}

#endif
