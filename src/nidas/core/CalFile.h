// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_CALFILE_H
#define NIDAS_CORE_CALFILE_H

#include "DOMable.h"
#include <nidas/util/UTime.h>
#include <nidas/util/IOException.h>
#include <nidas/util/EOFException.h>

#include <vector>
#include <fstream>

#include <regex.h>

namespace nidas { namespace core {

class DSMSensor;

/**
 * A class for reading ASCII files containing a time series of
 * calibration data.
 *
 * CalFile supports reading files like the following:
 *
 * Records are assumed to be time-ordered.
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
 *  is that the next readCF() will return data
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
 *
 *  ...
 *  while (tsample > calfile.nextTime()) {
 *      try {
 *          dsm_time_t calTime;
 *          float caldata[5];
 *          int n = calfile.readCF(calTime, caldata, 5);
 *          for (int i = 0; i < n; i++) coefs[i] = caldata[i];
 *      }
 *      catch(const nidas::util::IOException& e) {
 *          log(e.what);
 *          break;
 *      }
 *      catch(const nidas::util::ParseException& e) {
 *          log(e.what);
 *          break;
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
     * Assignment operator, like the copy constructor.
     * If a file is currently open it will be closed
     * before the assignment.
     */
    CalFile& operator=(const CalFile&);

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
     * Return all the paths that have been set in all CalFile instances,
     * in the order they were seen.  These have been separated at the colons.
     */
    static std::vector<std::string> getAllPaths()
    {
        _staticMutex.lock();
        std::vector<std::string> tmp = _allPaths;
        _staticMutex.unlock();
        return tmp;
    }

    /**
     * Return the full file path of the current file.
     */
    const std::string& getCurrentFileName() const
    {
        if (_include) return _include->getCurrentFileName();
        return _currentFileName;
    }

    /**
     * An instance of CalFile can have a name. Then more than one CalFile
     * can be associated with an object, such as a DSMSensor, and it
     * can differentiate them by name.
     */
    void setName(const std::string& val) 
    {
        _name = val;
    }

    const std::string& getName() const
    {
        return _name;
    }

    int getLineNumber() const
    {
        if (_include) return _include->getLineNumber();
        return _nline;
    }

    /** 
     * Open the file. It is not necessary to call open().
     * If the user has not done an open() it will
     * be done in the first readCF(), or search().
     *
     * @throws nidas::util::IOException
     */
    void open();

    /**
     * Close file. An opened CalFile is closed in the destructor,
     * so it is not necessary to call close.
     */
    void close() throw();

    /**
     * Have we reached eof.
     */
    bool eof() const {
        if (_include) return false;
        return _eofState;
    }

    const std::string& getTimeZone() const { return _timeZone; }

    /**
     * Set the timezone for the dates & times read from the file.  If a
     * "timeZone" comment is found at the beginning of the file, this
     * attribute will be set to that value.
     */
    void
    setTimeZone(const std::string& val);

    const std::string& getDateTimeFormat() const
    {
        return _dateTimeFormat;
    }

    /**
     * Set the format for reading the date & time from the file.
     * If a "dateFormat" comment is found at the beginning of the
     * file, this attribute will be set to that value.
     */
    void setDateTimeFormat(const std::string& val);

    /** 
     * Search forward in a file, returning the time of the last record
     * in the file with a time less than or equal to tsearch.
     * The time is available by calling nextTime().
     * The next call to readCF() will return that record.
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::ParseException
     */
    nidas::util::UTime search(const nidas::util::UTime& tsearch);

    /**
     * Read the time and data from the current record, and return the
     * number of values read.  The return value may be less than ndata, in
     * which case values in data after n will be filled with NANs.  As part
     * of this call, the next time in the file is also read, and its result
     * is available with nextTime().  This method uses a mutex so that
     * multi-threaded calls should not result in crashes or unparseable
     * data.  However two threads reading the same CalFile will "steal"
     * each other's data, meaning each thread won't read a full copy of the
     * CalFile.
     *
     * If @p fields is not null, then it points to a string vector to which
     * all the fields in the calfile record will be assigned.  So all the
     * numeric fields parsed and stored in @p data will also be included in
     * @p fields, followed by any fields past the last parsed numeric
     * field.
     *
     * For example, given this calfile line:
     *
     * @code
     * 2016 may 1 00:00:00 0.00 0.00 0.00 0.00 0 16.70 0.0 1.0 flipped
     * @endcode
     *
     * Then 9 strings will be added to @p fields: "0.00", ..., "1.0",
     * "flipped", but the returned value will still be 8, same as if fields
     * had been null.  If a caller only wants string fields, then it can
     * retrieve them like so:
     *
     * @code
     * std::vector<std::string> fields;
     * readCF(time, 0, 0, &fields);
     * @endcode
     *
     * Cal files can have non-numeric columns interspersed with the numeric
     * columns, in which case all the columns can be read into the @p
     * fields vector, and then individual fields can be converted to
     * numbers using getField() and getFields().
     *
     * After successfully reading a record with readCF(), the fields of the
     * current record are also stashed in this CalFile and can be retrieved
     * with getCurrentFields().  The fields are not valid except after
     * calling readCF().
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::ParseException
     */
    int readCF(nidas::util::UTime& time, float* data, int ndata,
               std::vector<std::string>* fields=0);

    /**
     * Return the time and fields of the current record, the one last read
     * with readCF().  If there is no current record, then the return
     * vector will be empty and the time will be LONG_LONG_MIN.  If an
     * include file is being read, then this returns the current fields of
     * the included file.
     **/
    const std::vector<std::string>&
    getCurrentFields(nidas::util::UTime* time = 0);

    nidas::util::UTime
    getCurrentTime()
    {
        return _currentTime;
    }

    /**
     * Convert the field at index @p column in the fields vector to a
     * number, and return the number.  Throws nidas::util::ParseException
     * if the field cannot be converted to a number, and the message
     * indicates which column caused the error.  Column is a 0-based index
     * into fields.  If @p fields is null, then use the same fields as
     * getCurrentFields() would return.
     **/
    float
    getFloatField(int column, const std::vector<std::string>* fields = 0);

    /**
     * Parse a range of columns from the fields vector as numbers and store
     * them in the array @p data.  @p begin is the index of the first field
     * to parse, and @p end is one greater than the index of the last field
     * to parse.  The first field is index 0.  If there are fewer fields
     * than numbers, the remaining numbers are filled with nan.  So the
     * data array must point to memory for at least (end - begin) numbers.
     * The return value is the number of fields that were parsed, so it may
     * be less than the number of data values filled in.
     *
     * Like getField(), throws nidas::util::ParseException if a field
     * cannot be converted to a number.
     *
     * If @p fields is null, then use the same fields as
     * getCurrentFields() would return.
     **/
    int
    getFields(int begin, int end, float* data,
              const std::vector<std::string>* fields = 0);

    /*
     * Return the value of the next time in the file.
     * If there is no next record in the file, the returned value
     * will be far off in the future.
     */
    nidas::util::UTime nextTime() throw()
    {
        return _nextTime;
    }

    /**
     * Set the DSMSensor associated with this CalFile.
     * CalFile may need this in order to substitute
     * for tokens like $DSM and $HEIGHT in the file or path names.
     * Otherwise it is not necessary to setDSMSensor.
     */
    void setDSMSensor(const DSMSensor* val);

    const DSMSensor* getDSMSensor() const;

    /**
     * @throws nidas::util::InvalidParameterException
     **/
    void fromDOMElement(const xercesc::DOMElement* node);

protected:

    /**
     * @throws nidas::util::ParseException
     **/
    nidas::util::UTime parseTime();

    /**
     * Read forward to next non-comment line in CalFile.  Place result in
     * _curline, and index of first non-space character in _curpos.  Set
     * _eofState=true if that is the case.  Also parses special comment
     * lines like below, using parseTimeComments():
     *
     *    # dateFormat = "xxxxx"
     *    # timeZone = "xxx"
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::ParseException
     */
    void readLine();

    /**
     * Check the current line for special comments with timezone and
     * datetime format settings.  Return true if such a comment was found
     * and handled.  Throw ParseException if there is an error with the
     * regular expression matching.
     */
    bool
    parseTimeComments();

    /**
     * If the current calfile record line is an include directive, parse
     * the include filename, open it, and return 1.  Otherwise return 0.
     * It is up to the caller to recurse into the include file to read the
     * next cal record.
     */
    int
    parseInclude();

    /**
     * @throws nidas::util::IOException,nidas::util::ParseException
     **/
    void openInclude(const std::string& name);

    /**
     * Internal version of readCF() which reads records from the current
     * include file, if any.  Returns the result from readCF(), or else -1
     * if the include file has been exhausted of records and closed.
     **/
    int
    readCFInclude(nidas::util::UTime& time, float* data, int ndata,
                  std::vector<std::string>* fields_out);


private:

    /** 
     * Read lines, parsing special comment lines and skipping other
     * comments or blank lines, until a record line is found.  Then parse
     * and return the time from that record. On EOF, the returned time will
     * be a huge value, far off in the mega-distant future. Does not return
     * an EOFException on EOF.  After parsing the time from a record,
     * curline contains that record, and curpos points to the character
     * after the datetime field.  Also sets _nextTime to the returned time.
     *
     * @throws nidas::util::IOException
     * @throws nidas::util::ParseException
     */
    nidas::util::UTime readTime();

    /**
     * @throws nidas::util::IOException
     * @throws nidas::util::ParseException
     **/
    int readCFNoLock(nidas::util::UTime& time, float* data, int ndata,
                     std::vector<std::string>* fields);

    std::string _name;

    std::string _fileName;

    std::string _path;

    std::string _currentFileName;

    std::string _timeZone;

    bool _utcZone;

    std::string _dateTimeFormat;

    std::ifstream _fin;

    static const int INITIAL_CURLINE_LENGTH = 128;

    int _curlineLength;

    char *_curline;

    int _curpos;

    bool _eofState;

    int _nline;

    nidas::util::UTime _nextTime;

    nidas::util::UTime _currentTime;
    std::vector<std::string> _currentFields;

    /**
     * Time stamp of include "file" record.
     */
    nidas::util::UTime _includeTime;

    /**
     * Time stamp of record after include "file".
     */
    nidas::util::UTime _timeAfterInclude;

    /**
     * Time stamp of last record in include file with time <= _includeTime.
     */
    nidas::util::UTime _timeFromInclude;

    CalFile* _include;

    const DSMSensor* _sensor;

    static nidas::util::Mutex _staticMutex;

    static int _reUsers;

    static bool _reCompiled;

    static regex_t _dateFormatPreg;

    static regex_t _timeZonePreg;

    static regex_t _includePreg;

    static void freeREs();

    /**
     * @throws nidas::util::ParseException
     **/
    static void compileREs();

    static std::vector<std::string> _allPaths;

    nidas::util::Mutex _mutex;
};

}}	// namespace nidas namespace core

#endif
