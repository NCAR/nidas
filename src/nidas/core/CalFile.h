/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-02-21 08:31:42 -0700 (Tue, 21 Feb 2006) $

    $LastChangedRevision: 3297 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/src/data_dump.cc $
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_CALFILE_H
#define NIDAS_DYNLD_CALFILE_H

#include <nidas/core/DOMable.h>
#include <nidas/core/DSMConfig.h>
#include <nidas/util/UTime.h>
#include <nidas/util/IOException.h>
#include <nidas/util/EOFException.h>

#include <fstream>
#include <list>

#include <regex.h>

namespace nidas { namespace core {

/**
 * A class for reading ASCII files containing a time series of
 * calibration data.
 *
 * CalFile supports reading files like the following:
 *
 *  # example cal file
 *  # dateFormat = "%Y %b %d %H:%M:%S"
 *  # timeZone = "US/Mountain"
 *  #
 *  2006 Sep 23 00:00:00    0.0 1.0
 *  # Offset of 1.0 after Sep 29 01:13:00
 *  2006 Sep 29 01:13:00    1.0 1.0
 *  # use calibrations for ACME sensor SN#99 after Oct 1
 *  2006 Oct 01 00:00:00    include "acme_sn99.dat"
 *
 * As shown, comment lines begin with a '#'.  There are two
 * special comment lines, of the form 'dateFormat="blahblah"'
 * and 'timeZone="blahblah".  These specify the format
 * of time in the calibration file.  The dateFormat
 * should contain date and time format descriptors as 
 * supported by the UNIX strftime function or as 
 * supported by the java.text.SimpleDateFormat class.
 * Typically the dateFormat comment is found once
 * in the file, before any data records.
 * 
 * If the dateFormat comment is not found in a file, and the
 * dateTimeFormat attribute is not set on the instance of
 * CalFile, then times must be in a format supported by
 * nidas::util::UTime::parse.
 *
 * <table>
 *  <tr>
 *    <th>field<th>example<th>UNIX<th>java
 *  <tr>
 *    <td>year<td>2006<td>%Y<td>YYYY
 *    <td>month abrev<td>Sep<td>%b<td>MMM
 *    <td>numeric month<td>9<td>%m<td>MM
 *    <td>day of month<td>9<td>%d<td>dd
 *    <td>day of year (1-366)<td>252<td>%j<td>DDD
 *    <td>hour in day (0-23)<td>13<td>%H<td>HH
 *    <td>minute(0-59)<td>47<td>%M<td>mm
 *    <td>second(0-59)<td>47<td>%M<td>ss
 *    <td>millisecond(0-999)<td>447<td>%03F<td>SSS
 *  </table>
 * 
 *  Following the time fields in each record should be either
 *  numeric values or an "include" directive.
 *
 *  The numeric values should be in a form compatible with
 *  floating point input, or the strings "na" or "nan" in
 *  either upper or lower case, representing not-a-number.
 *  Since math using nan results in a nan, a calibration
 *  record containing a nan values is a way to overwrite
 *  bad data with nan, indicating missing data.
 *
 *  An "include" directive causes another calibration
 *  file to be opened for input.  The included file will
 *  be sequentially read to set the input position to the
 *  latest record with a time less than or equal to the time
 *  value of the "include" directive.
 *
 *  An included file can also contain "include" directives.
 * 
 *  A typical usage of a CalFile is as follows:
 * <pre>
 *
 *  CalFile calfile;
 *  calfile.setFile("acme_sn1.dat")
 *  calfile.setPath("$ROOT/projects/$PROJECT/cal_files:$ROOT/cal_files");
 *  dsm_time_t calTime = 0;
 *
 *  ...
 *  while (tsample > calTime) {
 *      float caldata[5];
 *      try {
 *          int n = calfile.readData(caldata,5);
 *          for (int i = 0; i < n; i++) coefs[i] = caldata[i];
 *          // read the time of the next calibration record
 *          calTime = calfile.readTime();
 *      }
 *      catch(const nidas::util::IOException& e) {
 *          log(e.what);
 *      }
 *      catch(const nidas::util::ParseException& e) {
 *          log(e.what);
 *      }
 *  }
 *  // use coefs[] to calibrate sample.
 *      
 */
class CalFile: public nidas::core::DOMable {
public:

    CalFile();

    /**
     * Copy constructor. Copies the value of getFileName() and
     * getPath() attributes.  The CalFile will not be
     * opened in the new copy.
     */
    CalFile(const CalFile&);

    /**
     * Closes the file if necessary.
     */
    ~CalFile();

    const std::string& getFileName() const;

    /**
     * Set the base name of the file to be opened.
     */
    void setFileName(const std::string& val);

    /**
     * Set the search path to find the file, and any
     * included files: one or more directory paths
     * separated by colons ':'.
     */
    const std::string& getPath() const;

    /**
     * Set the search path to find the file, and any
     * included files: one or more directory paths
     * separated by colons ':'.
     */
    void setPath(const std::string& val);

    /**
     * Return the full file path of the current file.
     */
    const std::string& getName() const
    {
        return fullFileName;
    }

    /** 
     * Open the file. It is not necessary to call open().
     * If the user has not done an open() it will
     * be done in the first readTime, readData, or search().
     */
    void open() throw(nidas::util::IOException);

    /**
     * Close file. An opened CalFile is closed in the destructor,
     * so it is not necessary to call close.
     */
    void close();

    /**
     * Have we reached eof.
     */
    bool eof() const { return eofState; }

    const std::string& getTimeZone() const { return timeZone; }

    void setTimeZone(const std::string& val)
    {
        timeZone = val;
        utcZone = timeZone == "GMT" || timeZone == "UTC";
    }

    const std::string& getDateTimeFormat() const
    {
        return dateTimeFormat;
    }

    /**
     * Set the format for reading the date & time from the file.
     * If a "dateFormat" comment is found at the beginning of the
     * file, this attribute will be set to that value.
     */
    void setDateTimeFormat(const std::string& val);

    /** 
     * Search forward in a file, and position the input pointer
     * so the next record read wil be the last one in the
     * file with a time less than equal to tsearch.
     */
    void search(const nidas::util::UTime& tsearch)
        throw(nidas::util::IOException,nidas::util::ParseException);

    /** 
     * Read the time from the next record. If the EOF is found
     * the returned time will be a huge value, far off in the
     * mega-distant future. Does not return an EOFException
     * on EOF.
     */
    nidas::util::UTime readTime()
        throw(nidas::util::IOException,nidas::util::ParseException);

    /**
     * Read the data from the current record. The return
     * value may be less than ndata, in which case
     * values in data after n will be filled with NANs.
     */
    int readData(float* data, int ndata)
        throw(nidas::util::IOException,nidas::util::ParseException);

    /**
     * Set the DSM associated with this CalFile.
     * CalFile may needs this in order to substitute
     * for tokens like $DSM in the file or path names.
     * Otherwise it is not necessary to setDSMConfig.
     */
    void setDSMConfig(const nidas::core::DSMConfig* val);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

    xercesc::DOMElement* toDOMParent(xercesc::DOMElement* parent)
    	throw(xercesc::DOMException);

    xercesc::DOMElement* toDOMElement(xercesc::DOMElement* node)
    	throw(xercesc::DOMException);

protected:

    nidas::util::UTime parseTime() throw(nidas::util::ParseException);

    void readLine() throw(nidas::util::IOException,nidas::util::ParseException);

    void openInclude(const std::string& name)
        throw(nidas::util::IOException,nidas::util::ParseException);

private:

    std::string fileName;

    std::string path;

    std::string fullFileName;

    std::string timeZone;

    bool utcZone;

    std::string dateTimeFormat;

    std::ifstream fin;

    std::string curline;

    std::list<std::string> savedLines;

    int curpos;

    bool eofState;

    int nline;

    nidas::util::UTime curTime;

    nidas::util::UTime timeAfterInclude;

    CalFile* include;

    nidas::core::DSMConfig* dsm;

    static nidas::util::Mutex reMutex;

    static int reUsers;

    static bool reCompiled;

    static regex_t dateFormatPreg;

    static regex_t timeZonePreg;

    static regex_t includePreg;

    static void freeREs();

    static void compileREs() throw(nidas::util::ParseException);
};

}}	// namespace nidas namespace core

#endif
