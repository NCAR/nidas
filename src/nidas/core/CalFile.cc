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

#include <nidas/core/CalFile.h>
#include <nidas/core/DSMSensor.h>
#include <nidas/core/Sample.h>      // floatNAN
#include <nidas/core/Project.h>
#include <nidas/util/Logger.h>
#include <nidas/util/util.h>

#include <sys/stat.h>

#include <iomanip>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* static */
n_u::Mutex CalFile::_reMutex;

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
void CalFile::compileREs() throw(n_u::ParseException)
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
    _fileName(),_path(),_currentFileName(),
    _timeZone("GMT"),_utcZone(true),
    _dateTimeFormat(),_fin(),
    _curlineLength(INITIAL_CURLINE_LENGTH),
    _curline(new char[_curlineLength]),
    _curpos(0),_eofState(false),_nline(0),
    _curTime(LONG_LONG_MIN),_includeTime(LONG_LONG_MIN),
    _timeAfterInclude(LONG_LONG_MIN),_timeFromInclude(LONG_LONG_MIN),
    _include(0),_sensor(0)
{
    _curline[0] = '\0';
    setTimeZone("GMT");
    n_u::Synchronized autoLock(_reMutex);
    _reUsers++;
}

CalFile::CalFile(const CalFile& x):
    _fileName(x._fileName),_path(x._path),_currentFileName(),
    _timeZone("GMT"),_utcZone(true),
    _dateTimeFormat(x._dateTimeFormat),_fin(),
    _curlineLength(INITIAL_CURLINE_LENGTH),
    _curline(new char[_curlineLength]),
    _curpos(0),_eofState(false),_nline(0),
    _curTime(LONG_LONG_MIN),_includeTime(LONG_LONG_MIN),
    _timeAfterInclude(LONG_LONG_MIN),_timeFromInclude(LONG_LONG_MIN),
    _include(0),
    _sensor(x._sensor)
{
    _curline[0] = '\0';
    setTimeZone(x.getTimeZone());
    n_u::Synchronized autoLock(_reMutex);
    _reUsers++;
}

CalFile& CalFile::operator=(const CalFile& rhs)
{
    if (&rhs != this) {
        close();
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

    n_u::Synchronized autoLock(_reMutex);
    if (--_reUsers == 0 && _reCompiled) freeREs();

    delete [] _curline;
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
}

void CalFile::open() throw(n_u::IOException)
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
    if (_fin.fail()) throw n_u::IOException(getPath() + ' ' + getFile(),"open",errno);
    n_u::Logger::getInstance()->log(LOG_INFO,"CalFile: %s",_currentFileName.c_str());
    _eofState = false;
    _curline[0] = '\0';
    _curpos = 0;
}

void CalFile::close()
{
    if (_include) {
        _include->close();
        delete _include;
        _include = 0;
    }
    if (_fin.is_open()) _fin.close();
    _nline = 0;
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
    throw(n_u::IOException,n_u::ParseException)
{
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
    return parseTime();
}

n_u::UTime CalFile::parseTime()
    throw(n_u::ParseException)
{

    n_u::UTime t;

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

/*
 * Read time from a CalFile.
 * On EOF, it will return a time very far in the future.
 */
n_u::UTime CalFile::readTime() throw(n_u::IOException,n_u::ParseException)
{ 
    if (_include) {
        _curTime = _include->readTime();
        if (_curTime >= _timeAfterInclude) {
            _include->close();
            delete _include;
            _include = 0;
            _curTime = _timeAfterInclude;
        }
        // If more than one records have the same time as that returned
        // by the search of the include file, set their time to the
        // time of the include record.
        if (_curTime == _timeFromInclude) _curTime = _includeTime;
    }
    else {
        readLine();
        if (eof()) {
            close();
            _curTime = n_u::UTime(LONG_LONG_MAX);
        }
        else _curTime = parseTime();
    }
    return _curTime;
}

/*
 * Read forward to next non-comment line in CalFile.
 * Place result in curline, and index of first non-space
 * character in curpos.  Set _eofState=true if that is the case.
 * Also scans for and parses special comment lines
 * looking like:
 *    # dateFormat = "xxxxx"
 *    # timeZone = "xxx"
 */
void CalFile::readLine() throw(n_u::IOException,n_u::ParseException)
{
    if (eof()) return;
    if (!_fin.is_open()) open();


    for(;;) {
        _curpos = 0;
        _curline[0] = 0;

        int rlen = _curlineLength;

        _fin.getline(_curline,rlen);

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

        if (_fin.eof()) {
            _eofState = true;
            break;
        }
        // cerr << getCurrentFileName() << ": line=" << _curline << endl;
        if (_fin.bad()) 
            throw n_u::IOException(getCurrentFileName(),"read",errno);
        
        _nline++;

        for (_curpos = 0; _curline[_curpos] && std::isspace(_curline[_curpos]); _curpos++);
        if (!_curline[_curpos]) continue;		// all whitespace

        if (_curline[_curpos] != '#') break;	// actual data line, break

        // comment line, look for # dateFormat or # timeZone
        regmatch_t pmatch[2];
        int nmatch = sizeof pmatch/ sizeof(regmatch_t);

        {
            n_u::Synchronized autoLock(_reMutex);
            if (!_reCompiled) compileREs();
            int regstatus;
            if ((regstatus = ::regexec(&_dateFormatPreg,_curline + _curpos,nmatch,
                pmatch,0)) == 0 && pmatch[1].rm_so >= 0) {
                setDateTimeFormat(string(_curline + _curpos+pmatch[1].rm_so,
                    pmatch[1].rm_eo - pmatch[1].rm_so));
                continue;
            }
            else if (regstatus != REG_NOMATCH) {
                char regerrbuf[64];
                ::regerror(regstatus,&_dateFormatPreg,regerrbuf,sizeof regerrbuf);
                throw n_u::ParseException("regexec dateFormat RE",string(regerrbuf));
            }
            // cerr << "dateTime regstatus=" << regstatus << endl;

            if ((regstatus = ::regexec(&_timeZonePreg,_curline + _curpos,nmatch,
                pmatch,0)) == 0 && pmatch[1].rm_so >= 0) {
                setTimeZone(string(_curline + _curpos+pmatch[1].rm_so,
                    pmatch[1].rm_eo - pmatch[1].rm_so));
            }
            else if (regstatus != REG_NOMATCH) {
                char regerrbuf[64];
                ::regerror(regstatus,&_timeZonePreg,regerrbuf,sizeof regerrbuf);
                throw n_u::ParseException("regexec TimeZone RE",string(regerrbuf));
                continue;
            }
        }
        // cerr << "timezone regstatus=" << regstatus << endl;
    }
}

/*
 * Read numeric data from a CalFile into a float [];
 */
int CalFile::readData(float* data, int ndata)
    throw(n_u::IOException,n_u::ParseException)
{
    if (eof()) throw n_u::EOFException(getCurrentFileName(),"read");
    if (_include) return _include->readData(data,ndata);
    if (_curline[_curpos] == '\0') return 0;

    istringstream sin(_curline + _curpos);

    int id;
    for (id = 0; !sin.eof() && id < ndata; id++) {
        sin >> data[id];
        if (sin.fail()) {
            if (sin.eof()) break;
            // conversion failure
            if (id == 0) {
                // first field, check if it is an include line
                int regstatus;
                regmatch_t pmatch[2];
                int nmatch = sizeof pmatch/ sizeof(regmatch_t);
                string includeName;
                {
                    n_u::Synchronized autoLock(_reMutex);
                    if (!_reCompiled) compileREs();
                    if ((regstatus = ::regexec(&_includePreg,
                        _curline + _curpos,nmatch,pmatch,0)) == 0 &&
                            pmatch[1].rm_so >= 0) {
                        includeName = string(_curline + _curpos + pmatch[1].rm_so,
                                pmatch[1].rm_eo - pmatch[1].rm_so);
                    }
                    else if (regstatus != REG_NOMATCH) {
                        char regerrbuf[64];
                        ::regerror(regstatus,&_includePreg,regerrbuf,sizeof regerrbuf);
                        throw n_u::ParseException("regexec include RE",string(regerrbuf));
                    }
                }
                // include file match
                if (regstatus == 0) {
                    openInclude(includeName);
                    if (!_include->eof()) return _include->readData(data,ndata);
                    // If the include file contains no data (rare situation),
                    // return a value of ndata=0, so that the user can read again.
                    // Otherwise, if we just call readData() here recursively,
                    // then the next data read from this file will be
                    // associated with the time of this include record.
                    for (int i = 0; i < ndata; i++) data[i] = floatNAN;
                    return 0;
                }
            }
            // at this point the read failed and it isn't an "include" line.
            // Check if input field starts with "na", ignoring case, or with '#'.
            sin.clear();

            // setw(N) will read N-1 character fields, and add a NULL,
            // so the character array can be declared to be N.
            char possibleNaN[4];
            sin >> setw(4) >> possibleNaN;
            if (!sin.fail()) {
                if (::strlen(possibleNaN) > 1 && ::toupper(possibleNaN[0]) == 'N' &&
                    ::toupper(possibleNaN[1]) == 'A') {
                        data[id] = floatNAN;
                        continue;
                }
                else if (possibleNaN[0] == '#')
                    break;
            }
            ostringstream ost;
            ost << "invalid contents of field " << id << " in ";
            throw n_u::ParseException(getCurrentFileName(),
                ost.str() + '"' + (_curline + _curpos) + '"',getLineNumber());
        }
        // cerr << "data[" << id << "]=" << data[id] << endl;
        /*
         * The NCAR ISFF C++ code uses +-1.e37 as a no-data flag 
         */
        if (::fabs(data[id]) > 1.e36) data[id] = floatNAN;
    }
    for (int i = id; i < ndata; i++) data[i] = floatNAN;
    return id;
}

/*
 * Open an include file, and set the current position with search()
 * so that the next record read is the last one whose time is less
 * than or equal to the time of the include statement.
 */
void CalFile::openInclude(const string& name)
    throw(n_u::IOException,n_u::ParseException)
{

    // time stamp of  include "file" record
    _includeTime = _curTime;

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
	throw(n_u::InvalidParameterException)
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
	    else throw n_u::InvalidParameterException(xnode.getNodeName(),
			"unrecognized attribute", aname);
	}
    }
}

