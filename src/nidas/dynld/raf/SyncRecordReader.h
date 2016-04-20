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

#ifndef NIDAS_DYNLD_RAF_SYNCRECORDREADER_H
#define NIDAS_DYNLD_RAF_SYNCRECORDREADER_H

#include <deque>

#include <nidas/dynld/SampleInputStream.h>
#include <nidas/core/SampleTag.h>
#include "SyncRecordVariable.h"
#include "SyncServer.h"

#include <nidas/util/ThreadSupport.h>

#ifdef SYNC_RECORD_JSON_OUTPUT
#include <jsoncpp/json/json.h>
#endif

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

/**
 * SyncRecordReader handles sync samples and provides an interface to
 * access Variables and read sync record data.  It gets the Variables and
 * other information from the sync header, and then data in the special
 * sync record layout are copied directly from sync samples.
 *
 * The sync samples can be received in one of two ways.  It can read from
 * an IOChannel on which it blocks waiting for new sync samples, or it can
 * read samples through a SyncServer instance until a new sync sample is
 * distributed to this reader.  For now these methods correspond to
 * real-time or post-processing.  In real-time a DSM server generates the
 * sync records and provides a sample output to which an IOChannel connect.
 * In post-processing, a SyncServer is setup with input files and the
 * output of it's processing chain is connected to this reader.  In the
 * latter case the SyncRecordReader behaves like an instance of a
 * SampleClient.
 **/
class SyncRecordReader : public nidas::core::SampleClient
{
public:

    /**
     * Constructor of a SyncRecordReader to a connected IOChannel.
     * SyncRecordReader will own the IOChannel pointer and
     * will delete it when done.
     */
    SyncRecordReader(IOChannel* iochan);

    /**
     * Constructor for a SyncRecordReader connected directly as a
     * SampleClient of a SyncServer instance.
     **/
    SyncRecordReader(SyncServer* ss);

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

    const std::string &
    textHeader()
    {
        return _header;
    }

    virtual bool
    receive(const Sample *s) throw();

    virtual void
    flush() throw();

    /**
     * Signal the end of the sample stream, meaning EOF is reached once the
     * queue is empty.
     **/
    void
    endOfStream();

    int
    getSyncRecOffset(const nidas::core::Variable* var) 
        throw (SyncRecHeaderException);

    int
    getLagOffset(const nidas::core::Variable* var)
        throw (SyncRecHeaderException);

    /**
     * After creating a SyncRecordReader on a socket, this method returns
     * the config name from the SampleInputStream header.
     **/
    const std::string&
    getConfigName()
    {
        return _sampleStreamConfigName;
    }

private:

    void init();

    void scanHeader(const Sample* samp) throw();

    const Sample*
    nextSample();

    SampleInputStream* inputStream;
    SyncServer* syncServer;

    /// When true, explicitly read from the SyncServer, if given.
    /// Otherwise run the SyncServer thread to push samples through it's
    /// pipeline to the SyncRecordReader.
    bool _read_sync_server;

    std::string getQuotedString(std::istringstream& str);
    
    void readKeyedQuotedValues(std::istringstream& header)
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

    std::string _header;

    nidas::util::Cond _qcond;
    bool _eoq;

    /** Place to stash sample records received as a SampleClient. */
    std::deque<const Sample*> _syncRecords;

    std::string _sampleStreamConfigName;

    /** No copying. */
    SyncRecordReader(const SyncRecordReader&);

    /** No assignment. */
    SyncRecordReader& operator=(const SyncRecordReader&);
};

#ifdef SYNC_RECORD_JSON_OUTPUT
inline void
write_sync_record_header_as_json(std::ostream& json,
                                 const std::string& textheader)
{
	Json::Value root;

	std::istringstream iss(textheader);
	std::vector<std::string> lines;
	std::string line;
	while (getline(iss, line))
	{
        // Skip empty lines, especially the last one.
        if (line.length())
        {
            lines.push_back(line);
        }
	}
	Json::Value header;
	header.resize(lines.size());
	for (unsigned int i = 0; i < lines.size(); ++i)
	{
	    header[i] = lines[i];
	}
	root["header"] = header;
	json << root;
}


std::string
json_sync_record_header_as_string(Json::Value& root)
{
  Json::Value& header = root["header"];
  
  std::ostringstream oss;
  for (unsigned int i = 0; i < header.size(); ++i)
  {
    oss << header[i].asString() << "\n";
  }
  return oss.str();
}


inline void
write_sync_record_data_as_json(std::ostream& json,
                               dsm_time_t tt,
                               const double* rec,
                               size_t numValues)
{
    Json::Value root;
    root["time"] = tt;
    root["numValues"] = (int)numValues;
    // Unfortunately the JSON spec does not support NAN, and so
    // the data values are written as strings. NANs have a
    // string form like 'nan' which strtod() can reliably
    // convert back to a double.  Likewise for infinity (inf*),
    // but those are not as likely to be seen in nidas data.
    Json::Value data;
    data.resize(numValues);
    char buf[64];
    for (unsigned int i = 0; i < numValues; ++i)
    {
        snprintf(buf, sizeof(buf), "%.16g", rec[i]);
        data[i] = buf;
    }
    root["data"] = data;
    json << root;
}           

inline std::string
double_to_string(double value)
{
    char buf[64];
    snprintf(buf, sizeof(buf), "%.16g", value);
    return buf;
}

inline
Json::Value
write_sync_variable_as_json(const SyncRecordVariable* var,
                            dsm_time_t tt,
                            const double* rec)
{
    Json::Value variable;
    Json::Value values(Json::arrayValue);

    // Unfortunately the JSON spec does not support NAN, and so
    // the data values are written as strings. NANs have a
    // string form like 'nan' which strtod() can reliably
    // convert back to a double.  Likewise for infinity (inf*),
    // but those are not as likely to be seen in nidas data.
    size_t varoffset = var->getSyncRecOffset();
    int irate = (int)ceil(var->getSampleRate());
    int vlen = var->getLength();

    values.resize(vlen*irate);
    for (int i = 0; i < vlen*irate; ++i)
    {
        values[i] = double_to_string(rec[varoffset+i]);
    }
    variable["name"] = var->getName();
    variable["values"] = values;

    size_t lagoffset = var->getLagOffset();
    // int deltatUsec = (int)rint(USECS_PER_SEC / var->getSampleRate());
    dsm_time_t vtime = tt;
    if (!isnan(rec[lagoffset]))
        vtime += (int) rec[lagoffset];
    variable["time"] = vtime;
    variable["lagoffset"] = double_to_string(rec[lagoffset]);
    return variable;
}           

inline
void
write_sync_record_as_json(std::ostream& json, dsm_time_t tt,
                          const double* record, int nvalues,
                          std::vector<const SyncRecordVariable*>& vars)
{
    Json::Value root;
    root["time"] = tt;
    root["numValues"] = nvalues;
    Json::Value& data = root["data"];
    std::vector<const SyncRecordVariable*>::const_iterator it;
    for (it = vars.begin(); it != vars.end(); ++it)
    {
        Json::Value variable = write_sync_variable_as_json(*it, tt, record);
        data.append(variable);
    }
    json << root;
}
#endif

}}}	// namespace nidas namespace dynld namespace raf

#endif
