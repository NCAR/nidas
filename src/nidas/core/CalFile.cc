/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-02-21 08:31:42 -0700 (Tue, 21 Feb 2006) $

    $LastChangedRevision: 3297 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/src/data_dump.cc $
 ********************************************************************

*/

#include <nidas/core/CalFile.h>
#include <nidas/core/Sample.h>      // floatNAN
#include <nidas/core/Project.h>      // floatNAN

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

namespace {
void replaceChars(string& in,const string& pat, const string& rep)
{
    unsigned int patpos;
    while ((patpos = in.find(pat,0)) != string::npos)
        in.replace(patpos,pat.length(),rep);
}
}

/* static */
n_u::Mutex CalFile::reMutex;

/* static */
int CalFile::reUsers = 0;

/* static */
bool CalFile::reCompiled = false;

/* static */
regex_t CalFile::dateFormatPreg;

/* static */
regex_t CalFile::timeZonePreg;

/* static */
regex_t CalFile::includePreg;

/* static */
void CalFile::compileREs() throw(n_u::ParseException)
{
    // cerr << "compileREs" << endl;
    int regstatus;
    if ((regstatus = ::regcomp(&dateFormatPreg,
        "^[[:space:]]*#[[:space:]]*dateFormat[[:space:]]*=[[:space:]]*\"([^\"]+)\"",
            REG_EXTENDED)) != 0) {
        char regerrbuf[64];
        regerror(regstatus,&dateFormatPreg,regerrbuf,sizeof regerrbuf);
        throw n_u::ParseException("CalFile dateFormat regular expression",
            string(regerrbuf));
    }
    if ((regstatus = ::regcomp(&timeZonePreg,
            "^[[:space:]]*#[[:space:]]*timeZone[[:space:]]*=[[:space:]]*\"([^\"]+)\"",
            REG_EXTENDED)) != 0) {
        char regerrbuf[64];
        regerror(regstatus,&timeZonePreg,regerrbuf,sizeof regerrbuf);
        throw n_u::ParseException("CalFile timeZone regular expression",
            string(regerrbuf));
    }

    if ((regstatus = ::regcomp(&includePreg,
            "[[:space:]]*include[[:space:]]*=?[[:space:]]*\"([^\"]+)\"",
            REG_EXTENDED)) != 0) {
        char regerrbuf[64];
        regerror(regstatus,&includePreg,regerrbuf,sizeof regerrbuf);
        throw n_u::ParseException("CalFile include regular expression",
            string(regerrbuf));
    }
    reCompiled = true;
}

/* static */
void CalFile::freeREs()
{
    // cerr << "freeREs" << endl;
    ::regfree(&dateFormatPreg);
    ::regfree(&timeZonePreg);
    ::regfree(&includePreg);
    reCompiled = false;
}

CalFile::CalFile():
    timeZone("GMT"),utcZone(true),
    curpos(0),eofState(false),nline(0),
    include(0),dsm(0)
{
    setTimeZone("GMT");

    n_u::Synchronized autoLock(reMutex);
    reUsers++;
}

CalFile::CalFile(const CalFile& x):
    fileName(x.fileName),path(x.path),
    dateTimeFormat(x.dateTimeFormat),
    curpos(0),eofState(false),nline(0),include(0),dsm(0)
{
    setTimeZone(x.getTimeZone());

    n_u::Synchronized autoLock(reMutex);
    reUsers++;
}

CalFile::~CalFile()
{
    close();
    delete include;

    n_u::Synchronized autoLock(reMutex);
    if (--reUsers == 0 && reCompiled) freeREs();
}

const string& CalFile::getFile() const
{
    return fileName;
}

void CalFile::setFile(const string& val)
{
    // fileName = val.replace('\\',File.separatorChar);
    // fileName = fileName.replace('/',File.separatorChar);
    fileName = val;
}

const std::string& CalFile::getPath() const
{
    return path;
}

void CalFile::setPath(const std::string& val)
{
    path = val;
    // path = path.replace('\\',File.separatorChar);
    // path = path.replace('/',File.separatorChar);
    
    // path = path.replace(';',File.pathSeparatorChar);
    // path = path.replace(':',File.pathSeparatorChar);
}

void CalFile::setDateTimeFormat(const std::string& val)
{
    dateTimeFormat = val;
    replaceChars(dateTimeFormat,"yyyy","%Y");
    replaceChars(dateTimeFormat,"DDD","%j");
    replaceChars(dateTimeFormat,"MMM","%b");
    replaceChars(dateTimeFormat,"MM","%m");
    replaceChars(dateTimeFormat,"dd","%d");
    replaceChars(dateTimeFormat,"HH","%H");
    replaceChars(dateTimeFormat,"mm","%M");
    replaceChars(dateTimeFormat,"ss","%S");
    replaceChars(dateTimeFormat,"SSS","%3f");
}

void CalFile::open() throw(n_u::IOException)
{
    if (fin.is_open()) fin.close();

    for (string::size_type ic = 0;;) {

        string::size_type nc = path.find(':',ic);

        if (nc == string::npos) currentFileName = path.substr(ic);
        else currentFileName = path.substr(ic,nc-ic);

        if (currentFileName.length() > 0) currentFileName += '/';
        currentFileName += getFile();
        if (dsm) currentFileName = dsm->expandString(currentFileName);
        else if (Project::getInstance())
            currentFileName = Project::getInstance()->expandString(currentFileName);

        // cerr << "stat currentFileName=" << currentFileName << endl;

        struct stat filestat;
        if (::stat(currentFileName.c_str(),&filestat) == 0 &&
            S_ISREG(filestat.st_mode)) break;

        if (nc == string::npos) throw n_u::IOException(
            currentFileName,"open",ENOENT);
        ic = nc + 1;
    }

    fin.open(currentFileName.c_str());
    if (fin.fail()) throw n_u::IOException(currentFileName,"open",errno);
    cerr << "CalFile: " + currentFileName << endl;
    savedLines.clear();
    eofState = false;
    curline = "";
    curpos = 0;
}

void CalFile::close()
{
    if (include) include->close();
    if (fin.is_open()) fin.close();
}

/*
 * 
 * Position the current file at the beginning of the latest record
 * whose time is less than or equal to tsearch.
 * In other words, search for last record i, such that
 *   time[i] <= tsearch
 * Since records are assumed to be time-ordered, this means either
 * that tsearch < time[i+1], or that i is the last record in the file.
 *
 * Don't open include files during the search.
 *
 * Since we have to read ahead to record i+1, then we need to
 * save two lines.  We don't want to call readData, since that opens
 * include files.
 */
void CalFile::search(const n_u::UTime& tsearch)
    throw(n_u::IOException,n_u::ParseException)
{
    if (!fin.is_open()) open();

    list<string> tmpLines;
    for (;;) {
        readLine();
        if (tmpLines.size() == 2) tmpLines.pop_front();
        if (eof()) break;
        tmpLines.push_back(curline);

        n_u::UTime t = parseTime();
        if (t > tsearch) break;
    }
    nline -= tmpLines.size();
    savedLines = tmpLines;
}

n_u::UTime CalFile::parseTime()
    throw(n_u::ParseException)
{

    n_u::UTime t;

    string saveTZ;
    bool changeTZ = !utcZone &&
        (saveTZ = n_u::UTime::getTZ()) != timeZone; 
    if (changeTZ) n_u::UTime::setTZ(timeZone.c_str());

    int nchars = 0;
    try {
        if (dateTimeFormat.length() > 0) 
            t = n_u::UTime::parse(utcZone,curline.substr(curpos),
                dateTimeFormat,&nchars);
        else
            t = n_u::UTime::parse(utcZone,curline.substr(curpos),&nchars);
    }
    catch(const n_u::ParseException& e) {
        if (changeTZ) n_u::UTime::setTZ(saveTZ.c_str());
        throw n_u::ParseException(getCurrentFileName(),e.what(),getLineNumber());
    }
    curpos += nchars;
    if (changeTZ) n_u::UTime::setTZ(saveTZ.c_str());
    return t;
}

/*
 * Read time from a CalFile.
 * On EOF, it will return a time very far in the future.
 */
n_u::UTime CalFile::readTime() throw(n_u::IOException,n_u::ParseException)
{ 
    if (include) {
        curTime = include->readTime();
        if (curTime >= timeAfterInclude) {
            include->close();
            delete include;
            include = 0;
            curTime = timeAfterInclude;
        }
    }
    else {
        readLine();
        if (eof()) curTime = n_u::UTime(LONG_LONG_MAX);
        else curTime = parseTime();
    }
    return curTime;
}


/*
 * Read forward to next non-comment line in CalFile.
 * Also scans for and parses special comment lines
 * looking like
 *	# dateFormat = "xxxxx"
 *    # timeZone = "xxx"
 */
void CalFile::readLine() throw(n_u::IOException,n_u::ParseException)
{
    if (!fin.is_open()) open();

    curpos = 0;
    if (savedLines.size() > 0) {
        curline = savedLines.front();
        savedLines.pop_front();
        for (curpos = 0; curline[curpos] && std::isspace(curline[curpos]);
            curpos++);
        nline++;
        eofState = false;
        return;
    }
    char cbuf[1024];
    for(;;) {
        cbuf[0] = 0;
        fin.getline(cbuf,sizeof cbuf);
        if (fin.eof()) {
            eofState = true;
            break;
        }
        if (fin.bad()) 
            throw n_u::IOException(getCurrentFileName(),"read",errno);
        nline++;

        for (curpos = 0; cbuf[curpos] && std::isspace(cbuf[curpos]);
            curpos++);
        if (!cbuf[curpos]) continue;		// all whitespace

        if (cbuf[curpos] != '#') break;	// actual data line, break

        regmatch_t pmatch[2];
        int nmatch = sizeof pmatch/ sizeof(regmatch_t);

        {
            n_u::Synchronized autoLock(reMutex);
            if (!reCompiled) compileREs();
            int regstatus;
            if ((regstatus = ::regexec(&dateFormatPreg,cbuf+curpos,nmatch,
                pmatch,0)) == 0 && pmatch[1].rm_so >= 0) {
                setDateTimeFormat(string(cbuf+curpos+pmatch[1].rm_so,
                    pmatch[1].rm_eo - pmatch[1].rm_so));
                continue;
            }
            else if (regstatus != REG_NOMATCH) {
                char regerrbuf[64];
                ::regerror(regstatus,&dateFormatPreg,regerrbuf,sizeof regerrbuf);
                throw n_u::ParseException("regexec dateFormat RE",string(regerrbuf));
            }
            // cerr << "dateTime regstatus=" << regstatus << endl;

            if ((regstatus = ::regexec(&timeZonePreg,cbuf+curpos,nmatch,
                pmatch,0)) == 0 && pmatch[1].rm_so >= 0) {
                setTimeZone(string(cbuf+curpos+pmatch[1].rm_so,
                    pmatch[1].rm_eo - pmatch[1].rm_so));
            }
            else if (regstatus != REG_NOMATCH) {
                char regerrbuf[64];
                ::regerror(regstatus,&timeZonePreg,regerrbuf,sizeof regerrbuf);
                throw n_u::ParseException("regexec TimeZone RE",string(regerrbuf));
                continue;
            }
        }
        // cerr << "timezone regstatus=" << regstatus << endl;
    }
    curline = cbuf;
}

/*
 * Read numeric data from a CalFile into a float [];
 */
int CalFile::readData(float* data, int ndata)
    throw(n_u::IOException,n_u::ParseException)
{
    if (eof()) throw n_u::EOFException(getCurrentFileName(),"read");
    if (include) return include->readData(data,ndata);
    if (curline.substr(curpos).length() == 0) return 0;

    istringstream sin(curline.substr(curpos));

    int id;
    for (id = 0; !sin.eof() && id < ndata; id++) {

        sin >> data[id];
        if (sin.fail()) {
            // conversion failure
            if (id == 0) {
                // first field, check if it is an include line
                int regstatus;
                regmatch_t pmatch[2];
                int nmatch = sizeof pmatch/ sizeof(regmatch_t);
                string includeName;
                {
                    n_u::Synchronized autoLock(reMutex);
                    if (!reCompiled) compileREs();
                    if ((regstatus = ::regexec(&includePreg,
                        curline.substr(curpos).c_str(),nmatch,pmatch,0)) == 0 &&
                            pmatch[1].rm_so >= 0) {
                        includeName = curline.substr(curpos+pmatch[1].rm_so,
                                pmatch[1].rm_eo - pmatch[1].rm_so);
                    }
                    else if (regstatus != REG_NOMATCH) {
                        char regerrbuf[64];
                        ::regerror(regstatus,&includePreg,regerrbuf,sizeof regerrbuf);
                        throw n_u::ParseException("regexec include RE",string(regerrbuf));
                    }
                }
                // include file match
                if (regstatus == 0) {
                    // cerr << "includeName=" << includeName << endl;
                    openInclude(includeName);
                    n_u::UTime includeTime = include->readTime();
                    return include->readData(data,ndata);
                }
            }
            // at this point the read failed and it isn't an "include" line.
            // Check if input field starts with "na", ignoring case.
            sin.clear();
            sin.width(4);
            char possibleNaN[4];
            sin >> possibleNaN;
            if (!sin.fail() && ::strlen(possibleNaN) > 1 &&
                ::toupper(possibleNaN[0]) == 'N' &&
                ::toupper(possibleNaN[1]) == 'A') data[id] = floatNAN;
            else throw n_u::ParseException(getCurrentFileName(),
                curline.substr(curpos),getLineNumber());
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

void CalFile::openInclude(const string& name)
    throw(n_u::IOException,n_u::ParseException)
{
    n_u::UTime prevTime = curTime - 1;
    // read next time in this file before opening include file
    // We will read records from the include file until
    // they are equal or later than this time.
    timeAfterInclude = readTime();

    include = new CalFile();
    include->setPath(getPath());
    include->setFile(name);

    include->open();
    // cerr << "searching " << include->getCurrentFileName() << " t=" << prevTime << endl;
    include->search(prevTime);
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
	    if (!aname.compare("path")) setPath(aval);
	    else if (!aname.compare("file")) setFile(aval);
	    else throw n_u::InvalidParameterException(xnode.getNodeName(),
			"unrecognized attribute", aname);
	}
    }
}

xercesc::DOMElement* CalFile::toDOMParent(
    xercesc::DOMElement* parent)
    throw(xercesc::DOMException)
{
    return toDOMElement(parent);
}

xercesc::DOMElement* CalFile::toDOMElement(xercesc::DOMElement* node)
    throw(xercesc::DOMException)
{
    return node;
}

