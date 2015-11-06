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

#include <nidas/core/DOMable.h>
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
        if (_include) return false;
        return _eofState;
    }

    const std::string& getTimeZone() const { return _timeZone; }

    void setTimeZone(const std::string& val)
    {
        _timeZone = val;
        _utcZone = _timeZone == "GMT" || _timeZone == "UTC";
    }

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
     */
    nidas::util::UTime search(const nidas::util::UTime& tsearch)
        throw(nidas::util::IOException,nidas::util::ParseException);

    /**
     * Read the time and data from the current record.
     * The return value may be less than ndata, in which case
     * values in data after n will be filled with NANs.
     * As part of this call, the next time in the file is also
     * read, and its result is available with nextTime().
     * This method uses a mutex so that multi-threaded calls
     * should not result in crashes or unparseable data.
     * However two threads reading the same CalFile will "steal"
     * each other's data, meaning each thread won't read a full
     * copy of the CalFile.
     */
    int readCF(nidas::util::UTime& time, float* data, int ndata)
        throw(nidas::util::IOException,nidas::util::ParseException);

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

    void fromDOMElement(const xercesc::DOMElement* node)
	throw(nidas::util::InvalidParameterException);

protected:

    nidas::util::UTime parseTime() throw(nidas::util::ParseException);

    void readLine() throw(nidas::util::IOException,nidas::util::ParseException);

    void openInclude(const std::string& name)
        throw(nidas::util::IOException,nidas::util::ParseException);

private:

    /** 
     * Read the time from the next record. If the EOF is found
     * the returned time will be a huge value, far off in the
     * mega-distant future. Does not return an EOFException
     * on EOF.
     */
    nidas::util::UTime readTime()
        throw(nidas::util::IOException,nidas::util::ParseException);

    int readCFNoLock(nidas::util::UTime& time, float* data, int ndata)
        throw(nidas::util::IOException,nidas::util::ParseException);

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

    static void compileREs() throw(nidas::util::ParseException);

    static std::vector<std::string> _allPaths;

    nidas::util::Mutex _mutex;
};

}}	// namespace nidas namespace core

#endif
