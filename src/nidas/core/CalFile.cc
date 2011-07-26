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
    _timeZone("GMT"),_utcZone(true),
    _curpos(0),_eofState(false),_nline(0),
    _include(0),_sensor(0)
{
    setTimeZone("GMT");

    n_u::Synchronized autoLock(_reMutex);
    _reUsers++;
}

CalFile::CalFile(const CalFile& x):
    _fileName(x._fileName),_path(x._path),
    _dateTimeFormat(x._dateTimeFormat),
    _curpos(0),_eofState(false),_nline(0),_include(0),
    _sensor(x._sensor)
{
    setTimeZone(x.getTimeZone());

    n_u::Synchronized autoLock(_reMutex);
    _reUsers++;
}

CalFile::~CalFile()
{
    close();
    delete _include;

    n_u::Synchronized autoLock(_reMutex);
    if (--_reUsers == 0 && _reCompiled) freeREs();
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
    _curline = "";
    _curpos = 0;
}

void CalFile::close()
{
    if (_include) _include->close();
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
void CalFile::search(const n_u::UTime& tsearch)
    throw(n_u::IOException,n_u::ParseException)
{
    if (!_fin.is_open()) open();

    n_u::UTime prevTime(0LL);

    // First time, scan file, saving the time previous to
    // the one greater than tsearch
    for (;;) {
        readLine();
        if (eof()) break;
        n_u::UTime t = parseTime();
        if (t > tsearch) break;
        prevTime = t;
    }

    // rewind
    _fin.clear();
    _fin.seekg(0);
    _nline = 0;
    _eofState = false;

    // read until time is == prevTime.
    for (;;) {
        readLine();
        if (eof()) break;
        n_u::UTime t = parseTime();
        if (t >= prevTime) break;
    }
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
            t = n_u::UTime::parse(_utcZone,_curline.substr(_curpos),
                _dateTimeFormat,&nchars);
        else
            t = n_u::UTime::parse(_utcZone,_curline.substr(_curpos),&nchars);
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

    _curpos = 0;
    char cbuf[1024];
    for(;;) {
        cbuf[0] = 0;
        _fin.getline(cbuf,sizeof cbuf);
        if (_fin.eof()) {
            // cerr << getCurrentFileName() << ": eof" << endl;
            _eofState = true;
            break;
        }
        // cerr << getCurrentFileName() << ": line=" << cbuf << endl;
        if (_fin.bad()) 
            throw n_u::IOException(getCurrentFileName(),"read",errno);
        _nline++;

        for (_curpos = 0; cbuf[_curpos] && std::isspace(cbuf[_curpos]);
            _curpos++);
        if (!cbuf[_curpos]) continue;		// all whitespace

        if (cbuf[_curpos] != '#') break;	// actual data line, break

        regmatch_t pmatch[2];
        int nmatch = sizeof pmatch/ sizeof(regmatch_t);

        {
            n_u::Synchronized autoLock(_reMutex);
            if (!_reCompiled) compileREs();
            int regstatus;
            if ((regstatus = ::regexec(&_dateFormatPreg,cbuf+_curpos,nmatch,
                pmatch,0)) == 0 && pmatch[1].rm_so >= 0) {
                setDateTimeFormat(string(cbuf+_curpos+pmatch[1].rm_so,
                    pmatch[1].rm_eo - pmatch[1].rm_so));
                continue;
            }
            else if (regstatus != REG_NOMATCH) {
                char regerrbuf[64];
                ::regerror(regstatus,&_dateFormatPreg,regerrbuf,sizeof regerrbuf);
                throw n_u::ParseException("regexec dateFormat RE",string(regerrbuf));
            }
            // cerr << "dateTime regstatus=" << regstatus << endl;

            if ((regstatus = ::regexec(&_timeZonePreg,cbuf+_curpos,nmatch,
                pmatch,0)) == 0 && pmatch[1].rm_so >= 0) {
                setTimeZone(string(cbuf+_curpos+pmatch[1].rm_so,
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
    _curline = cbuf;
}

/*
 * Read numeric data from a CalFile into a float [];
 */
int CalFile::readData(float* data, int ndata)
    throw(n_u::IOException,n_u::ParseException)
{
    if (eof()) throw n_u::EOFException(getCurrentFileName(),"read");
    if (_include) return _include->readData(data,ndata);
    if (_curline.substr(_curpos).length() == 0) return 0;

    istringstream sin(_curline.substr(_curpos));

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
                        _curline.substr(_curpos).c_str(),nmatch,pmatch,0)) == 0 &&
                            pmatch[1].rm_so >= 0) {
                        includeName = _curline.substr(_curpos+pmatch[1].rm_so,
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
                    // if include file contains no data (rare situation),
                    // return a value 0, so that the user can read again.
                    // Otherwise, if we just call readData() here recursively,
                    // then the next data read will be associated with
                    // the results of the previous readTime.
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
                    ::toupper(possibleNaN[1]) == 'A') data[id] = floatNAN;
                else if (possibleNaN[0] == '#')
                    break;
            }
            else {
                ostringstream ost;
                ost << ": field number " << (id+1);
                throw n_u::ParseException(getCurrentFileName(),
                    _curline.substr(_curpos) + ost.str(),getLineNumber());
            }
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

    n_u::UTime tsearch = _curTime;

    // read next time in this file before opening include file
    // We will read records from the include file until
    // they are equal or later than this time.
    _timeAfterInclude = readTime();
    // cerr << "_timeAfterInclude=" << _timeAfterInclude.toUsecs() << endl;

    _include = new CalFile(*this);
    _include->setFile(name);
    _include->open();

    // cerr << "searching " << _include->getCurrentFileName() << " t=" << prevTime << endl;
    _include->search(tsearch);
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

