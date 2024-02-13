// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; -*-
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

#include "CalFile.h"
#include "DSMSensor.h"
#include "Sample.h"      // floatNAN
#include "Project.h"
#include <nidas/util/Logger.h>
#include <nidas/util/util.h>

#include <sys/stat.h>

#include <iomanip>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;
using nidas::util::UTime;

/* static */
vector<string> CalFile::_allPaths;

/* static */
n_u::Mutex CalFile::_staticMutex;

/* static */
int CalFile::_reUsers = 0;

/* static */
bool CalFile::_reCompiled = false;

/* static */
regex_t CalFile::_dateFormatPreg;

/* static */
regex_t CalFile::_timeZonePreg;

/* static */
regex_t CalFile::_includePreg;

/* static */
void CalFile::compileREs()
{
    // cerr << "compileREs" << endl;
    int regstatus;
    if ((regstatus = ::regcomp(&_dateFormatPreg,
        "^[[:space:]]*#[[:space:]]*dateFormat[[:space:]]*=[[:space:]]*\"([^\"]+)\"",
            REG_EXTENDED)) != 0) {
        char regerrbuf[64];
        regerror(regstatus,&_dateFormatPreg,regerrbuf,sizeof regerrbuf);
        throw n_u::ParseException("CalFile dateFormat regular expression",
            string(regerrbuf));
    }
    if ((regstatus = ::regcomp(&_timeZonePreg,
            "^[[:space:]]*#[[:space:]]*timeZone[[:space:]]*=[[:space:]]*\"([^\"]+)\"",
            REG_EXTENDED)) != 0) {
        char regerrbuf[64];
        regerror(regstatus,&_timeZonePreg,regerrbuf,sizeof regerrbuf);
        throw n_u::ParseException("CalFile timeZone regular expression",
            string(regerrbuf));
    }

    if ((regstatus = ::regcomp(&_includePreg,
            "[[:space:]]*include[[:space:]]*=?[[:space:]]*\"([^\"]+)\"",
            REG_EXTENDED)) != 0) {
        char regerrbuf[64];
        regerror(regstatus,&_includePreg,regerrbuf,sizeof regerrbuf);
        throw n_u::ParseException("CalFile include regular expression",
            string(regerrbuf));
    }
    _reCompiled = true;
}

/* static */
void CalFile::freeREs()
{
    // cerr << "freeREs" << endl;
    ::regfree(&_dateFormatPreg);
    ::regfree(&_timeZonePreg);
    ::regfree(&_includePreg);
    _reCompiled = false;
}

CalFile::CalFile():
    _name(),_fileName(),_path(),_currentFileName(),
    _timeZone("GMT"),_utcZone(true),
    _dateTimeFormat(),_fin(),
    _curlineLength(INITIAL_CURLINE_LENGTH),
    _curline(new char[_curlineLength]),
    _curpos(0),_eofState(false),_nline(0),
    _nextTime(UTime::MIN),
    _currentTime(UTime::MIN),
    _currentFields(),
    _includeTime(UTime::MIN),
    _timeAfterInclude(UTime::MIN),
    _timeFromInclude(UTime::MIN),
    _include(0),_sensor(0),_mutex()
{
    _curline[0] = '\0';
    setTimeZone("GMT");
    n_u::Synchronized autoLock(_staticMutex);
    _reUsers++;
}

CalFile::CalFile(const CalFile& x): DOMable(),
    _name(x._name),_fileName(x._fileName),_path(x._path),_currentFileName(),
    _timeZone("GMT"),_utcZone(true),
    _dateTimeFormat(x._dateTimeFormat),_fin(),
    _curlineLength(INITIAL_CURLINE_LENGTH),
    _curline(new char[_curlineLength]),
    _curpos(0),_eofState(false),_nline(0),
    _nextTime(UTime::MIN),
    _currentTime(UTime::MIN),
    _currentFields(),
    _includeTime(UTime::MIN),
    _timeAfterInclude(UTime::MIN),
    _timeFromInclude(UTime::MIN),
    _include(0),
    _sensor(x._sensor),_mutex()
{
    _curline[0] = '\0';
    setTimeZone(x.getTimeZone());
    n_u::Synchronized autoLock(_staticMutex);
    _reUsers++;
}

CalFile& CalFile::operator=(const CalFile& rhs)
{
    if (&rhs != this) {
        *(DOMable*) this = rhs;
        close();
        _name = rhs._name;
        _fileName = rhs._fileName;
        _path = rhs._path;
        _dateTimeFormat = rhs._dateTimeFormat;
        _curpos = 0;
        _eofState = false;
        _nline = 0;
        _include = 0;
        _sensor = rhs._sensor;
        setTimeZone(rhs.getTimeZone());
    }
    return *this;
}

CalFile::~CalFile()
{
    close();
    delete _include;

    n_u::Synchronized autoLock(_staticMutex);
    if (--_reUsers == 0 && _reCompiled) freeREs();

    delete [] _curline;
}


void
CalFile::
setTimeZone(const std::string& val)
{
    _timeZone = val;
    _utcZone = _timeZone == "GMT" || _timeZone == "UTC";
    VLOG(("") << _fileName << ": timezone=" << _timeZone
         << ", utczone=" << _utcZone);
}


const string& CalFile::getFile() const
{
    return _fileName;
}

void CalFile::setFile(const string& val)
{
    // _fileName = val.replace('\\',File.separatorChar);
    // _fileName = _fileName.replace('/',File.separatorChar);
    _fileName = val;
}

const std::string& CalFile::getPath() const
{
    return _path;
}

void CalFile::setPath(const std::string& val)
{
    _path = val;


    // maintain _appPaths. For readers of cal files paths, order is important,
    // so push_back if it hasn't been seen before.

    string tmp = val;
    string::size_type colon;
    while ((colon = tmp.find(':')) != string::npos) {
        _staticMutex.lock();
        string cpath = tmp.substr(0,colon);
        if (std::find(_allPaths.begin(),_allPaths.end(),cpath) == _allPaths.end())
            _allPaths.push_back(cpath);
        _staticMutex.unlock();
        tmp = tmp.substr(colon+1);
    }

    if (tmp.length() > 0) {
        _staticMutex.lock();
        if (std::find(_allPaths.begin(),_allPaths.end(),tmp) == _allPaths.end())
            _allPaths.push_back(tmp);
        _staticMutex.unlock();
    }

    // _path = _path.replace('\\',File.separatorChar);
    // _path = _path.replace('/',File.separatorChar);

    // _path = _path.replace(';',File.pathSeparatorChar);
    // _path = _path.replace(':',File.pathSeparatorChar);
}

void CalFile::setDSMSensor(const DSMSensor* val)
{
    _sensor = val;
}

const DSMSensor* CalFile::getDSMSensor() const
{
    return _sensor;
}

void CalFile::setDateTimeFormat(const std::string& val)
{
    _dateTimeFormat = val;
    n_u::replaceCharsIn(_dateTimeFormat,"yyyy","%Y");
    n_u::replaceCharsIn(_dateTimeFormat,"DDD","%j");
    n_u::replaceCharsIn(_dateTimeFormat,"MMM","%b");
    n_u::replaceCharsIn(_dateTimeFormat,"MM","%m");
    n_u::replaceCharsIn(_dateTimeFormat,"dd","%d");
    n_u::replaceCharsIn(_dateTimeFormat,"HH","%H");
    n_u::replaceCharsIn(_dateTimeFormat,"mm","%M");
    n_u::replaceCharsIn(_dateTimeFormat,"ss","%S");
    n_u::replaceCharsIn(_dateTimeFormat,"SSS","%3f");
    VLOG(("") << _fileName << ": set datetime format '"
         << _dateTimeFormat << "'");
}

void CalFile::open()
{
    if (_fin.is_open()) _fin.close();

    for (string::size_type ic = 0;;) {

        string::size_type nc = _path.find(':',ic);

        if (nc == string::npos) _currentFileName = _path.substr(ic);
        else _currentFileName = _path.substr(ic,nc-ic);

        if (_currentFileName.length() > 0) _currentFileName += '/';
        _currentFileName += getFile();
        if (_sensor) _currentFileName = _sensor->expandString(_currentFileName);

        struct stat filestat;
        if (::stat(_currentFileName.c_str(),&filestat) == 0 &&
            S_ISREG(filestat.st_mode)) break;

        if (nc == string::npos) throw n_u::IOException(
            _currentFileName,"open",ENOENT);
        ic = nc + 1;
    }

    _fin.open(_currentFileName.c_str());
    if (_fin.fail())
        throw n_u::IOException(getPath() + ' ' + getFile(), "open", errno);
    ILOG(("CalFile: ") << _currentFileName);
    _eofState = false;
    _curline[0] = '\0';
    _curpos = 0;
    _nextTime = UTime::MIN;
}

void CalFile::close() throw()
{
    if (_include) {
        _include->close();
        delete _include;
        _include = 0;
    }
    if (_fin.is_open()) _fin.close();
    _nline = 0;
    // We specifically do not reset these here because the file might be
    // closed after a call to readCF(), even though readCF() just read a
    // valid record that has become the current record.
    //    _currentTime = LONG_LONG_MIN;
    //    _currentFields.clear();
}


const std::vector<std::string>&
CalFile::
getCurrentFields(nidas::util::UTime* time)
{
    if (_include)
    {
        return _include->getCurrentFields(time);
    }
    if (time)
        *time = _currentTime;
    return _currentFields;
}


/*
 * Search forward so that the next record to be read is the last one
 * whose time is less than or equal to tsearch.
 * In other words, set the current position to record i, such that
 *   time[i] <= tsearch
 * Since records are assumed to be time-ordered, this means either
 * that tsearch < time[i+1], or that i is the last record in the file.
 *
 * There may be more than one record with time[i], and this search()
 * will position the file to the first one. This is useful for RAF
 * cal files, which may have multiple records with the same time.
 *
 * Don't open include files during the search.
 * If eof() is true after this search, then the include file contains no data.
 */
n_u::UTime CalFile::search(const n_u::UTime& tsearch)
{
    n_u::Autolock autolock(_mutex);

    if (!_fin.is_open()) open();

    n_u::UTime prevTime(LONG_LONG_MAX);

    istream::pos_type recpos = 0;
    int nline = 0;

    // First time, scan file, saving the file position of the
    // record before the one with time > tsearch.
    for (;;) {
        istream::pos_type pos = _fin.tellg();
        readLine();
        if (eof()) break;
        n_u::UTime t = parseTime();
        if (t > tsearch) break;
        if (t != prevTime) {
            prevTime = t;
            recpos = pos;
            nline = _nline - 1;
        }
    }

    // reposition back to last record <= tsearch
    _fin.clear();
    _fin.seekg(recpos);
    _nline = nline;
    _eofState = false;

    readLine();
    if (eof()) return prevTime;
    _nextTime = parseTime();
    // cerr << "search of " << getCurrentFileName() << " done, _nextTime=" <<
    //     _nextTime.format(true,"%F %T") << endl;
    return _nextTime;
}

n_u::UTime CalFile::parseTime()
{
    n_u::UTime t((long long)0);

    string saveTZ;
    bool changeTZ = !_utcZone &&
        (saveTZ = n_u::UTime::getTZ()) != _timeZone;
    if (changeTZ) n_u::UTime::setTZ(_timeZone.c_str());

    int nchars = 0;
    try {
        if (_dateTimeFormat.length() > 0)
            t = n_u::UTime::parse(_utcZone,_curline + _curpos,
                _dateTimeFormat,&nchars);
        else
            t = n_u::UTime::parse(_utcZone,_curline + _curpos,&nchars);
    }
    catch(const n_u::ParseException& e) {
        if (changeTZ) n_u::UTime::setTZ(saveTZ.c_str());
        throw n_u::ParseException(getCurrentFileName(),e.what(),getLineNumber());
    }
    _curpos += nchars;
    if (changeTZ) n_u::UTime::setTZ(saveTZ.c_str());
    return t;
}

n_u::UTime CalFile::readTime()
{ 
    readLine();
    if (eof())
    {
        close();
        _nextTime = n_u::UTime(LONG_LONG_MAX);
    }
    else
    {
        _nextTime = parseTime();
    }
    return _nextTime;
}

/*
 * Lock a mutex, then read numeric data from a CalFile into
 * a float [].
 */
int CalFile::readCF(n_u::UTime& time, float* data, int ndata,
                    std::vector<std::string>* fields)
{
    n_u::Autolock autolock(_mutex);
    return readCFNoLock(time, data, ndata, fields);
}


int
CalFile::
parseInclude()
{
    // first field, check if it is an include line
    int regstatus;
    regmatch_t pmatch[2];
    int nmatch = sizeof pmatch/ sizeof(regmatch_t);
    string includeName;
    {
        n_u::Synchronized autoLock(_staticMutex);
        if (!_reCompiled)
            compileREs();
        regstatus = ::regexec(&_includePreg, _curline + _curpos,
                              nmatch, pmatch, 0);
        if ((regstatus == 0) && pmatch[1].rm_so >= 0)
        {
            includeName = string(_curline + _curpos + pmatch[1].rm_so,
                                 pmatch[1].rm_eo - pmatch[1].rm_so);
        }
        else if (regstatus != REG_NOMATCH)
        {
            char regerrbuf[64];
            ::regerror(regstatus, &_includePreg, regerrbuf, sizeof regerrbuf);
            throw n_u::ParseException("regexec include RE", string(regerrbuf));
        }
    }
    /* found an "include" record */
    if (regstatus == 0)
    {
        openInclude(includeName);
        return 1;
    }
    return 0;
}



int CalFile::readCFInclude(n_u::UTime& time, float* data, int ndata,
                           std::vector<std::string>* fields_out)
{
    if (! _include)
    {
        return -1;
    }
    n_u::UTime intime = _include->nextTime();

    static n_u::LogContext inclog(LOG_VERBOSE);
    if (inclog.active())
    {
        if (intime.toUsecs() == LONG_LONG_MAX)
            inclog.log() << "include->nextTime" << "MAX"
                         << ", timeAfterInclude="
                         << _timeAfterInclude.format(true,"%F %T");
        else
            inclog.log() << "include->nextTime"
                         << intime.format(true,"%F %T")
                         << ", timeAfterInclude="
                         << _timeAfterInclude.format(true,"%F %T");
    }

    if (intime < _timeAfterInclude)
    {
        // after the read, time should be the same as intime.
        int n = _include->readCF(time, data, ndata, fields_out);

        VLOG(("") << _fileName
             << ": include->read, time=" << time.format(true,"%F %T")
             << ", includeTime=" << _includeTime.format(true,"%F %T"));

        if (time < _includeTime) time = _includeTime;

        intime =  _include->nextTime();
        if (intime > _timeAfterInclude)
            _nextTime = _timeAfterInclude;
        else
            _nextTime = intime;
        return n;
    }

    /* if the next time in include file is >= the time after the
     * "include" record, then we're done with this include file */
    _include->close();
    delete _include;
    _include = 0;
    return -1;
}


/*
 * Private version of readCF which does not hold a mutex.  When
 */
int CalFile::readCFNoLock(n_u::UTime& time, float* data, int ndata,
                          std::vector<std::string>* fields_out)
{
    // Make sure the "current record" looks invalid in case this throws an
    // exception.
    _currentFields.clear();
    _currentTime = UTime::MIN;

    if (_include)
    {
        int n = readCFInclude(time, data, ndata, fields_out);
        if (n >= 0)
            return n;
    }

    if (eof())
    {
        throw n_u::EOFException(getCurrentFileName(),"read");
    }

    /* first call to readCF() for this file */
    if (_nextTime.isMin())
    {
        readTime();
    }
    time = _nextTime;

    // At this point we are on a calfile record line, just past the
    // timestamp, so everything past this point is taken as a calibration
    // field.  The original code parsed numbers first, so a field with
    // "123.456#" would have been parsed as a coefficient followed by a
    // comment, therefore it is not enough to first tokenize the record as
    // space-separated strings.  Instead, first strip any comment
    // characters and anything following.

    char* pound = strchr(_curline + _curpos, '#');
    if (pound)
        *pound = '\0';

    // Now check if this record is an include directive, and if so, recurse
    // into the include file looking for the next cal record.
    if (parseInclude())
    {
        return readCFNoLock(time, data, ndata, fields_out);
    }

    /* Finally, this looks like a regular cal record line which can
     * be parsed into fields. */
    istringstream fin(_curline + _curpos);
    std::string ifield;
    std::vector<std::string> fields;

    while (fin >> ifield)
    {
        fields.push_back(ifield);
    }
    _currentFields = fields;
    _currentTime = _nextTime;

    if (fields_out)
        *fields_out = fields;

    int id = getFields(0, ndata, data);
    readTime();

    return id;
}


int
CalFile::
getFields(int begin, int end, float* data,
          const std::vector<std::string>* fields_in)
{
    if (!fields_in)
        fields_in = &_currentFields;
    const std::vector<std::string>& fields = *fields_in;

    // For as many data elements as need to be filled, try to parse a
    // number out of the field.
    int id = 0;
    int ndata = end - begin;
    for (int fi = begin; fi < end && fi < (int)fields.size(); ++fi, ++id)
    {
        const string& field = fields[fi];

        // The field must either parse as a number or be equal to 'NA' or
        // 'NAN' when converted to upper case.

        istringstream sin(field);
        sin >> data[id];
        // The original code would have accepted a number followed by a
        // non-number character, but then it should have failed trying to
        // parse a number from what followed.  So just make sure the whole
        // field was parsed into a number.
        if (sin.fail() || !sin.eof())
        {
            string possibleNaN(field);
            for (string::iterator ci = possibleNaN.begin();
                 ci != possibleNaN.end(); ++ci)
            {
                *ci = ::toupper(*ci);
            }
            if (possibleNaN == "NAN" || possibleNaN == "NA")
            {
                data[id] = floatNAN;
                continue;
            }
            ostringstream ost;
            ost << "invalid contents of field " << fi+1 << " in "
                <<  '"' << string(_curline + _curpos) << '"';
            throw n_u::ParseException(getCurrentFileName(),
                                      ost.str(), getLineNumber());
        }
        if (::fabs(data[id]) > 1.e36)
            data[id] = floatNAN;
    }
    // If more data values were expected than we have fields, fill them
    // with NaN.
    if (id < ndata)
    {
        for (int i = id; i < ndata; i++)
            data[i] = floatNAN;
    }
    return id;
}


float
CalFile::
getFloatField(int column, const std::vector<std::string>* fields)
{
    float data;
    getFields(column, column+1, &data, fields);
    return data;
}


void CalFile::readLine()
{
    if (!_fin.is_open()) open();

    for(;;) {

        // eof() is used to indicate there is no line left to parse.  So
        // set _eofState here instead of after the getline() below, because
        // getline() may read a line that needs to be parsed but also reach
        // eof.
        if (eof() || _fin.eof())
        {
            _eofState = true;
            VLOG(("readLine: ") << getCurrentFileName() << " at eof");
            return;
        }

        _curpos = 0;
        _curline[0] = 0;

        int rlen = _curlineLength;

        _fin.getline(_curline,rlen);
        VLOG(("readLine: ") << getCurrentFileName() << ": line=" << _curline);

        // if _curline is not large enough for the current line, expand it
        while (_fin.fail() &&_fin.gcount() == rlen - 1) {
            _curlineLength += _curlineLength / 2;
            char* tmpptr = new char[_curlineLength];
            strcpy(tmpptr,_curline);
            delete [] _curline;
            _curline = tmpptr;
            tmpptr = _curline + strlen(_curline);
            rlen = _curlineLength - (tmpptr - _curline);
            _fin.clear();
            _fin.getline(tmpptr,rlen);
        }

        // cerr << getCurrentFileName() << ": line=" << _curline << endl;
        if (_fin.bad())
            throw n_u::IOException(getCurrentFileName(),"read",errno);

        _nline++;

        _curpos = 0;
        while (_curline[_curpos] && std::isspace(_curline[_curpos]))
        {
            _curpos++;
        }
        if (!_curline[_curpos]) continue;		// all whitespace

        if (_curline[_curpos] != '#') break;	// actual data line, break

        parseTimeComments();
    }
}


bool
CalFile::
parseTimeComments()
{
    // comment line, look for # dateFormat or # timeZone
    regmatch_t pmatch[2];
    int nmatch = sizeof pmatch/ sizeof(regmatch_t);

    n_u::Synchronized autoLock(_staticMutex);
    if (!_reCompiled) compileREs();
    int regstatus;
    if ((regstatus = ::regexec(&_dateFormatPreg,_curline + _curpos,nmatch,
                               pmatch,0)) == 0 && pmatch[1].rm_so >= 0) {
        setDateTimeFormat(string(_curline + _curpos+pmatch[1].rm_so,
                                 pmatch[1].rm_eo - pmatch[1].rm_so));
        return true;
    }
    else if (regstatus != REG_NOMATCH) {
        char regerrbuf[64];
        ::regerror(regstatus,&_dateFormatPreg,regerrbuf,sizeof regerrbuf);
        throw n_u::ParseException("regexec dateFormat RE",string(regerrbuf));
    }

    if ((regstatus = ::regexec(&_timeZonePreg,_curline + _curpos,nmatch,
                               pmatch,0)) == 0 && pmatch[1].rm_so >= 0) {
        setTimeZone(string(_curline + _curpos+pmatch[1].rm_so,
                           pmatch[1].rm_eo - pmatch[1].rm_so));
        return true;
    }
    else if (regstatus != REG_NOMATCH) {
        char regerrbuf[64];
        ::regerror(regstatus,&_timeZonePreg,regerrbuf,sizeof regerrbuf);
        throw n_u::ParseException("regexec TimeZone RE",string(regerrbuf));
    }
    return false;
}


/*
 * Open an include file, and set the current position with search()
 * so that the next record read is the last one whose time is less
 * than or equal to the time of the include statement.
 */
void CalFile::openInclude(const string& name)
{

    // time stamp of  include "file" record
    _includeTime = _nextTime;

    // read next time in this file before opening include file
    // We will read records from the include file until
    // they are equal or later than this time.
    _timeAfterInclude = readTime();

    _include = new CalFile(*this);
    _include->setFile(name);
    _include->open();

    // time stamp from include file of last rec with time <= _includeTime
    _timeFromInclude = _include->search(_includeTime);

}

void CalFile::fromDOMElement(const xercesc::DOMElement* node)
{
    XDOMElement xnode(node);
    // const string& elname = xnode.getNodeName();
    if(node->hasAttributes()) {
        // get all the attributes of the node
        xercesc::DOMNamedNodeMap *pAttributes = node->getAttributes();
        int nSize = pAttributes->getLength();
        for(int i=0;i<nSize;++i) {
            XDOMAttr attr((xercesc::DOMAttr*) pAttributes->item(i));
            // get attribute name
            const std::string& aname = attr.getName();
            const std::string& aval = attr.getValue();
            if (aname == "path") setPath(aval);
            else if (aname == "file") setFile(aval);
            else if (aname == "name") setName(aval);
            else throw n_u::InvalidParameterException(xnode.getNodeName(),
                                                      "unrecognized attribute",
                                                      aname);
        }
    }
}

