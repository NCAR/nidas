/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_CALFILE_H
#define NIDAS_CORE_CALFILE_H

#include <nidas/core/DOMable.h>
#include <nidas/util/UTime.h>
#include <nidas/util/IOException.h>
#include <nidas/util/EOFException.h>

#include <fstream>
#include <list>

#include <regex.h>

namespace nidas { namespace core {

class DSMSensor;

/**
 * A class for reading ASCII files containing a time series of
 * calibration data.
 *
 * CalFile supports reading files like the following:
 *
 * <pre>
 *  # example cal file
 *  # dateFormat = "%Y %b %d %H:%M:%S"
 *  # timeZone = "US/Mountain"
 *  #
 *  2006 Sep 23 00:00:00    0.0 1.0
 *  # Offset of 1.0 after Sep 29 01:13:00
 *  2006 Sep 29 01:13:00    1.0 1.0
 *  # use calibrations for ACME sensor SN#99 after Oct 1
 *  2006 Oct 01 00:00:00    include "acme_sn99.dat"
 * </pre>
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
 *    <td>year<td>2006<td>\%Y<td>YYYY
 *  <tr>
 *    <td>month abrev<td>Sep<td>\%b<td>MMM
 *  <tr>
 *    <td>numeric month<td>9<td>\%m<td>MM
 *  <tr>
 *    <td>day of month<td>14<td>\%d<td>dd
 *  <tr>
 *    <td>day of year (1-366)<td>257<td>\%j<td>DDD
 *  <tr>
 *    <td>hour in day (00-23)<td>13<td>\%H<td>HH
 *  <tr>
 *    <td>minute(00-59)<td>24<td>\%M<td>mm
 *  <tr>
 *    <td>second(00-59)<td>47<td>\%S<td>ss
 *  <tr>
 *    <td>millisecond(000-999)<td>447<td>\%3f<br>(UTime extension)<td>SSS
 *  </table>
 * 
 *  Following the time fields in each record should be either
 *  numeric values or an "include" directive.
 *
 *  The numeric values should be space or tab separated
 *  values compatible with the usual floating point syntax,
 *  or the strings "na" or "nan" in either upper or lower case,
 *  representing not-a-number, or NaN.
 *  Since math operations using NaN result in a NaN, a
 *  calibration record containing a NaN value is a way to
 *  generate output values of NaN, indicating non-recoverable
 *  data.
 *
 *  An "include" directive causes another calibration
 *  file to be opened for input.  The included file will
 *  be sequentially searched to set the input position to the
 *  last record with a time less than or equal to the time
 *  value of the "include directive. What this means
 *  is that the next readData() will return data
 *  from the included file which is valid for the
 *  time of the include directive.
 *
 *  An included file can also contain "include" directives.
 *
 *  The include directive is useful when sensors are swapped
 *  during a data acquisition period.  One can keep the
 *  sensor specific calibrations in separate files, and
 *  then create a CalFile which includes the sensor
 *  calibrations for the periods that a sensor was deployed.
 *  Example:
 * <pre>
 *  # initially krypton hygrometer 1101 at this site
 *  2006 Sep 23 00:00:00    include "krypton1101"
 *  # Replaced 1101 with 1393 on Oct 3.
 *  2006 Oct  3 01:13:00    include "krypton1393"
 * </pre>
 *
 *  A typical usage of a CalFile is as follows:
 *
 * <pre>
 *  CalFile calfile;
 *  calfile.setFile("acme_sn1.dat")
 *  calfile.setPath("$ROOT/projects/$PROJECT/cal_files:$ROOT/cal_files");
 *  dsm_time_t calTime = 0;
 *
 *  ...
 *  while (tsample > calTime) {
 *      try {
     *      float caldata[5];
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
 * </pre>
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

    const std::string& getFile() const;

    /**
     * Set the base name of the file to be opened.
     */
    void setFile(const std::string& val);

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
    const std::string& getCurrentFileName() const
    {
        if (include) return include->getCurrentFileName();
        return currentFileName;
    }

    int getLineNumber() const
    {
        if (include) return include->getLineNumber();
        return nline;
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
    bool eof() const {
        if (include) return false;
        return eofState;
    }

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
     * Set the DSMSensor associated with this CalFile.
     * CalFile may need this in order to substitute
     * for tokens like $DSM and $HEIGHT in the file or path names.
     * Otherwise it is not necessary to setDSMSensor.
     */
    void setDSMSensor(const DSMSensor* val);

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:

    nidas::util::UTime parseTime() throw(nidas::util::ParseException);

    void readLine() throw(nidas::util::IOException,nidas::util::ParseException);

    void openInclude(const std::string& name)
        throw(nidas::util::IOException,nidas::util::ParseException);

private:

    std::string fileName;

    std::string path;

    std::string currentFileName;

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

    const DSMSensor* _sensor;

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
