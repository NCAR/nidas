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

#define _FILE_OFFSET_BITS 64

#include <nidas/util/FileSet.h>
#include <nidas/util/EOFException.h>
#include <nidas/util/Logger.h>
#include <nidas/util/UTime.h>


using namespace nidas::util;
using namespace std;

#include <iostream>
#include <sstream>
#include <locale>
#include <vector>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>
#include <cstdlib>  // for llabs()

const char FileSet::pathSeparator = '/';	// this is unix, afterall

FileSet::FileSet() :
	_timeputter(std::use_facet<std::time_put<char> >(std::locale())),
        _newFile(false),_lastErrno(0),
        _dir(),_filename(),_currname(),_fullpath(),_fd(-1),
        _startTime((time_t)0),_endTime((time_t)0),
        _fileset(),_fileiter(_fileset.begin()),
	_initialized(false),_fileLength(LONG_LONG_MAX/2)
{
}

/* Copy constructor. */
FileSet::FileSet(const FileSet& x):
	_timeputter(std::use_facet<std::time_put<char> >(std::locale())),
        _newFile(false),_lastErrno(0),
	_dir(x._dir),_filename(x._filename),_currname(),_fullpath(x._fullpath),
	_fd(-1),_startTime(x._startTime),_endTime(x._endTime),
	_fileset(x._fileset),_fileiter(_fileset.begin()),
	_initialized(x._initialized),
	_fileLength(x._fileLength)
{
}

/* Assignment operator. */
FileSet& FileSet::operator=(const FileSet& rhs)
{
    if (this != &rhs) {
        closeFile();
        _newFile = false;
        _lastErrno = 0;
	_dir = rhs._dir;
        _filename = rhs._filename;
        _currname = "";
        _fullpath = rhs._fullpath;
        _startTime = rhs._startTime;
        _endTime = rhs._endTime;
	_fileset = rhs._fileset;
        _fileiter = _fileset.begin();
	_initialized = rhs._initialized;
	_fileLength = rhs._fileLength;
    }
    return *this;
}

FileSet* FileSet::clone() const
{
    return new FileSet(*this);
}

FileSet::~FileSet()
{
    try {
        closeFile();
    }
    catch(const IOException& e) {}
}

void FileSet::setDir(const std::string& val)
{
    _dir = val;
    _fullpath = makePath(val,getFileName());
}

void FileSet::setFileName(const std::string& val)
{
    _filename = val;
    _fullpath = makePath(getDir(),val);
}


void FileSet::closeFile() throw(IOException)
{
    if (_fd >= 0) {
        /*
         * Note that we don't do an fsync or fdatasync here before closing.
         * FileSet is used for streamed data files, which are not typically
         * created and closed very often, usually less than once an hour.
         * If something causes the last bit of data not to be
         * written, it isn't typically a big issue because the file contents
         * up to that point should still be readable, and no one is going
         * to cry much about losing 5 seconds of data just before the
         * power went out.  Instead, we'll let the OS schedule the sync
         * as a result of the close.  It seems to me that if an fsync is
         * needed here then we should be doing it periodically all the time,
         * which then disables the caching capabilities of the file system.
         * We'll depend on the journalling file system to maintain integrity.
         * If necessary, we could add an fsync method if someone really wants it.
         */
        int fd = _fd;
	_fd = -1;
#ifdef DO_FSYNC
        if (::fsync(fd) < 0) {
            int ierr = errno;
            ::close(fd);
            throw IOException(_currname,"fsync",ierr);
        }
#endif
        if (::close(fd) < 0)
	    throw IOException(_currname,"close",errno);
    }
}

long long FileSet::getFileSize() const throw(IOException)
{
    if (_fd >= 0) {
        struct stat statbuf;
        if (::fstat(_fd,&statbuf) < 0)
	    throw IOException(_currname,"fstat",errno);
	return statbuf.st_size;
    }
    return 0;
}

/* static */
void FileSet::createDirectory(const string& name) throw(IOException)
{
    DLOG(("FileSet::createDirectory, name=") << name);
    if (name.length() == 0) throw IOException(name,"mkdir",ENOENT);

    struct stat statbuf;
    if (::stat(name.c_str(),&statbuf) < 0) {
        if (errno != ENOENT) throw IOException(name,"open",errno);

        // create parent directory if it doesn't exist
	string tmpname = getDirPortion(name);
	if (tmpname != ".") createDirectory(tmpname);  // recursive

        ILOG(("creating: ") << name);
        if (::mkdir(name.c_str(),0777) < 0) throw IOException(name,"mkdir",errno);
    }
}


void
FileSet::
openFileForWriting(const std::string& filename) throw(IOException)
{
    if ((_fd = ::open(filename.c_str(),O_CREAT | O_EXCL | O_WRONLY,0444)) < 0) {
        _lastErrno = errno;
        throw IOException(filename,"create",errno);
    }
}

/**
 * Create a file using a time to create the name.
 * Return the time of the next file.
 */
UTime FileSet::createFile(const UTime ftime,bool exact) throw(IOException)
{
    DLOG(("nidas::util::FileSet::createFile, ftime=")
	 << ftime.format(true,"%c"));
    closeFile();

    UTime ntime = ftime;

    if (!exact && _fileLength < LONG_LONG_MAX / 2)
	ntime -= ntime.toUsecs() % _fileLength;

    // convert input time into date/time format using GMT timezone
    _currname = ntime.format(true,_fullpath);

    DLOG(("nidas::util::FileSet:: fullpath=") << _fullpath);
    DLOG(("nidas::util::FileSet:: currname=") << _currname);

    // create the directory, and parent directories, if they don't exist
    string tmpname = getDirPortion(_currname);
    if (tmpname != ".") try {
        createDirectory(tmpname);
    }
    catch (const IOException& e) {
        _lastErrno = e.getErrno();
        throw e;
    }

    ILOG(("creating: ") << _currname);

    try {
        openFileForWriting(_currname);
    }
    catch(const IOException&e) {
        /*
         * When a data system does not have a clock source on bootup, it
         * may start collecting data with a bogus time. Then if the
         * clock source comes online, a new file will be created here
         * since the time likely jumped ahead by more than fileLength.
         * Typically in this situation, "exact" will be false and the
         * system will try to create a time with (ntime % fileLength) == 0,
         * which is typically a time of day like 00:00:00, or 12:00:00.
         * This may conflict with a file that was created earlier in
         * a previous run of the data system with a good clock.
         * So if exact is false, and we get an EEXIST error, then create a file
         * with the exact time requested.
         */
        if (_lastErrno == EEXIST && _fullpath.find('%') != string::npos) {
            WLOG(("%s: %s",_currname.c_str(),e.what()));
            if (!exact) return createFile(ftime,true);
            else return createFile(ftime+USECS_PER_SEC,true);
        }
        throw e;
    }

    UTime nextFileTime = ntime + USECS_PER_SEC;	// add one sec
    nextFileTime += _fileLength - (nextFileTime.toUsecs() % _fileLength);

    DLOG(("nidas::util::FileSet:: nextFileTime=")
	 << nextFileTime.format(true,"%c"));
    _newFile = true;

    return nextFileTime;
}

size_t FileSet::read(void* buf, size_t count) throw(IOException)
{
    _newFile = false;
    if (_fd < 0) openNextFile();		// throws EOFException
    ssize_t res = ::read(_fd,buf,count);
    if (res <= 0) {
        if (res == 0) {
            closeFile();	// next read will open next file
            return res;
        }
        throw IOException(_currname,"read",errno);
    }
    return res;
}

size_t FileSet::write(const void* buf, size_t count) throw(IOException)
{
    ssize_t res = ::write(_fd,buf,count);
    if (res < 0) {
        _lastErrno = errno;
        throw IOException(_currname,"write",errno);
    }
    return res;
}

size_t FileSet::write(const struct iovec* iov, int iovcnt) throw(IOException)
{
    ssize_t res = ::writev(_fd,iov,iovcnt);
    if (res < 0) {
        _lastErrno = errno;
        throw IOException(_currname,"write",errno);
    }
    return res;
}


void FileSet::openNextFile() throw(IOException)
{
    if (!_initialized) {

	DLOG(("openNextFile, fullpath=") << _fullpath);
	if (_fullpath.length() > 0) {

	    _fileset = matchFiles(_startTime,_endTime);

	    // If the first matched file is later than the
	    // start time, then we'll look to find an earlier
	    // file.
            string firstFile;
            string t1File = formatName(_startTime);
	    if (!_fileset.empty()) firstFile = _fileset.front();
	    if (_fileset.empty() || firstFile.compare(t1File) > 0) {
                UTime t1;
                list<string> files;
                // roll back a day
                if (_fileLength > USECS_PER_DAY)
                    t1 = _startTime - USECS_PER_DAY;
                else {
                    t1 = _startTime;
                    t1 -= t1.toUsecs() % _fileLength;
                }
                UTime t2 = _startTime;

                // Try to handle the situation where the fileLength in the XML
                // is incorrect, which may happen if the archive files were
                // merged, with a longer file length than the original. If the
                // fileLength is smaller than the lengths of the files being read,
                // an earlier file may not be found here. If no matches,
                // back up some more.
                for (int i = 0; i < 4; i++) {
                    files = matchFiles(t1,t2);
                    if (!files.empty()) break;
                    if (_fileLength > USECS_PER_DAY) t1 -= USECS_PER_DAY;
                    else t1 -= _fileLength;
                }
                if (!files.empty())  {
                    list<string>::const_reverse_iterator ptr = files.rbegin();
                    string fl = *ptr;
                    if (firstFile.length() == 0 || 
                        fl.compare(firstFile) < 0) _fileset.push_front(fl);
                }
            }

	    if (_fileset.empty()) throw IOException(_fullpath,"open",ENOENT);
	}
	_fileiter = _fileset.begin();
	_initialized = true;
    }
    closeFile();

    if (_fileiter == _fileset.end()) throw EOFException(_currname,"open");
    _currname = *_fileiter++;
    ILOG(("opening: ") << _currname);

    if (_currname == "-") _fd = 0;	// read from stdin
    else if ((_fd = ::open(_currname.c_str(),O_RDONLY)) < 0)
    	throw IOException(_currname,"open",errno);
    _newFile = true;
}

/* static */
string FileSet::getDirPortion(const string& path)
{
    string::size_type lslash = path.rfind(pathSeparator);
    if (lslash == string::npos) return ".";
    else if (lslash == 0) return string(1,pathSeparator);
    else return path.substr(0,lslash);
}

/* static */
string FileSet::getFilePortion(const string& path)
{
    string::size_type lslash = path.rfind(pathSeparator);
    if (lslash == string::npos) return path;
    else return path.substr(lslash+1);
}

/* static */
string FileSet::makePath(const string& dir,const string& file)
{
    if (dir.length() == 0 || dir == ".") return file;
    return dir + pathSeparator + file;
}

string FileSet::formatName(const UTime& t1)
{
    return t1.format(true,_fullpath);
}

#if !defined(NIDAS_EMBEDDED)
void FileSet::checkPathFormat(const UTime& t1, const UTime& t2) throw(IOException)
{
    if (_fullpath.find("%b") != string::npos) {
	string m1 = t1.format(true,"%b");
	string m2 = t2.format(true,"%b");
	if (::llabs(t1-t2) > 31 * USECS_PER_DAY || m1 != m2) 
	    throw IOException(
		string("FileSet: ") + _fullpath,"search",
		"%b (alpha month) does not sort to time order");
    }

    vector<string::size_type> dseq;
    string::size_type di;
    di = _fullpath.find("%Y");
    if (di == string::npos) di = _fullpath.find("%y");
    if (di == string::npos) di = 0;
    dseq.push_back(di);

    di = _fullpath.find("%m");
    if (di == string::npos) di = dseq[0];
    dseq.push_back(di);

    di = _fullpath.find("%d");
    if (di == string::npos) di = dseq[1];
    dseq.push_back(di);

    di = _fullpath.find("%H");
    if (di == string::npos) di = dseq[2];
    dseq.push_back(di);

    di = _fullpath.find("%M");
    if (di == string::npos) di = dseq[3];
    dseq.push_back(di);

    di = _fullpath.find("%S");
    if (di == string::npos) di = dseq[4];
    dseq.push_back(di);

    for (unsigned int i = 1; i < dseq.size(); i++)
        if (dseq[i] < dseq[i-1]) throw IOException(
		string("FileSet: ") + _fullpath,"search",
		"file names do not sort to time order");
}
#endif

list<string> FileSet::matchFiles(const UTime& t1, const UTime& t2) throw(IOException)
{

    set<string> matchedFiles;
    long long requestDeltat = t2 - t1;

#if !defined(NIDAS_EMBEDDED)
    // Check that format sorts correctly
    checkPathFormat(t1,t2);
#endif

    DLOG(("fullpath=") << _fullpath);

    string t1path = t1.format(true,_fullpath);
    string t2path = t2.format(true,_fullpath);

    DLOG(("t1path=") << t1path);
    DLOG(("t2path=") << t2path);

    bool t1path_eq_t2path = t1path == t2path;

    // Check if there are time fields in the directory portion.
    // If so, it complicates things.

    string openedDir = getDirPortion(_fullpath);

    // use std::time_put to format path into a file name
    // this converts the strftime %Y,%m type format descriptors
    // into date/time fields.
    DLOG(("openedDir=") << openedDir);
    string tmpPath = t1.format(true,openedDir);

    long long dirDeltat = 365 * USECS_PER_DAY;
    // Check if file name has changed via the above time formatting,
    // therefore there must be % fields in it.
    if (tmpPath != openedDir) {
	if (openedDir.find("%H") != string::npos) dirDeltat = USECS_PER_HOUR;
	else if (openedDir.find("%d") != string::npos)
		dirDeltat = USECS_PER_DAY;
	else if (openedDir.find("%m") != string::npos)
		dirDeltat = 28 * USECS_PER_DAY;
	else if (openedDir.find("%y") != string::npos)
		dirDeltat = 365 * USECS_PER_DAY;
	else if (openedDir.find("%Y") != string::npos)
		dirDeltat = 365 * USECS_PER_DAY;
	else dirDeltat = USECS_PER_HOUR;	// wierd
    }
    openedDir.clear();

    DLOG(("dirDeltat=") << dirDeltat);
    DLOG(("fullpath=") << _fullpath);

    // must execute this loop at least once with time of t1.
    // If t2 > t1 then increment time by dirDeltat each iteration,
    // but in the last iteration, time should be == t2.
    DIR *dirp = 0;
    for (UTime ftime = t1; ftime <= t2; ) {
	// currpath is the full path name of a file with a name
	// corresponding to time ftime
	string currpath = ftime.format(true,_fullpath);
	DLOG(("currpath=") << currpath);

	// currdir is the directory portion of currpath
	string currdir = getDirPortion(currpath);

	struct dirent *dp;
	if (currdir != openedDir) {
	    if (dirp) closedir(dirp);
	    if ((dirp = opendir(currdir.c_str())) == NULL)
		throw IOException(currdir,"opendir",errno);
	    openedDir = currdir;
	}
	else rewinddir(dirp);

	DLOG(("fullpath=") << _fullpath);
	// now take file portion of input path, and create regular expression
	string fileregex = 
	    string("^") + getFilePortion(_fullpath) + string("$");
	    
	DLOG(("fileregex=") << fileregex);

	// replace time fields with corresponding regular expressions

	// This ensures that if we jump across a month when incrementing
	// by dirDeltat that we match files for the intermediate month.
	// dirDeltat is never more than a year, so we don't have to
	// substitute for %y or %Y.
	if (requestDeltat > 28 * USECS_PER_DAY) {
	    replaceChars(fileregex,"%m","[0-1][0-9]");
	    replaceChars(fileregex,"%b","[a-zA-Z][a-zA-Z][a-zA-Z]");
	}
	if (requestDeltat > USECS_PER_DAY)
	    replaceChars(fileregex,"%d","[0-3][0-9]");
	replaceChars(fileregex,"%H","[0-2][0-9]");
	replaceChars(fileregex,"%M","[0-5][0-9]");
	replaceChars(fileregex,"%S","[0-5][0-9]");
	DLOG(("fileregex=") << fileregex);

	fileregex = ftime.format(true,fileregex);
	DLOG(("fileregex=") << fileregex);

	// compile into regular expression
	regex_t preg;
	int regstatus;
	if ((regstatus = regcomp(&preg,fileregex.c_str(),0)) != 0) {
	    char regerrbuf[64];
	    regerror(regstatus,&preg,regerrbuf,sizeof regerrbuf);
	    throw IOException(fileregex,"regcomp",string(regerrbuf));
	}

	// search directory
	while ((dp = readdir(dirp))) {
	    DLOG(("dp->d_name=") << dp->d_name);
	    if ((regstatus = regexec(&preg,dp->d_name,0,0,0)) == 0) {
		string matchfile = makePath(currdir,dp->d_name);
		DLOG(("regexec matchfile=") << matchfile);
	        if (t1path.compare(matchfile) <= 0) {
		    if (t2path.compare(matchfile) > 0 ||
		    	(t1path_eq_t2path && t2path.compare(matchfile) >= 0)) {
			DLOG(("regexec & time matchfile=") << matchfile);
			matchedFiles.insert(matchfile);
		    }
		}
	    }
	    else if (regstatus != REG_NOMATCH) {
		char regerrbuf[64];
		regerror(regstatus,&preg,regerrbuf,sizeof regerrbuf);
		throw IOException(fileregex,"regexec",string(regerrbuf));
	    }
        }
	regfree(&preg);

	if (ftime == t2) break;
	ftime += dirDeltat;
	if (ftime > t2) ftime = t2;
    }
    if (dirp) closedir(dirp);
    DLOG(("matchedFiles.size()=") << matchedFiles.size());
    return list<string>(matchedFiles.begin(),matchedFiles.end());
}

/* static */
void FileSet::replaceChars(string& in,const string& pat, const string& rep) 
{
    string::size_type patpos;
    while ((patpos = in.find(pat,0)) != string::npos)
    	in.replace(patpos,pat.length(),rep);
}


