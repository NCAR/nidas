// -*- mode: C++; c-basic-offset: 4; -*-

#define _LARGEFILE64_SOURCE

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

// #include <climits>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <regex.h>
#include <cstdlib>  // for llabs()

const char FileSet::pathSeparator = '/';	// this is unix, afterall

FileSet::FileSet() :
	timeputter(std::use_facet<std::time_put<char> >(std::locale())),
	fd(-1),fileiter(fileset.begin()),
	initialized(false),fileLength(400*USECS_PER_DAY),newFile(false),
        _lastErrno(0)
{
}

/* Copy constructor. */
FileSet::FileSet(const FileSet& x):
	timeputter(std::use_facet<std::time_put<char> >(std::locale())),
	dir(x.dir),filename(x.filename),fullpath(x.fullpath),
	fd(-1),startTime(x.startTime),endTime(x.endTime),
	fileset(x.fileset),fileiter(fileset.begin()),
	initialized(x.initialized),
	fileLength(x.fileLength),newFile(false),
        _lastErrno(0)
{
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
    dir = val;
    fullpath = makePath(val,getFileName());
}

void FileSet::setFileName(const std::string& val)
{
    filename = val;
    fullpath = makePath(getDir(),val);
}


void FileSet::closeFile() throw(IOException)
{
    if (fd >= 0) {
        if (::close(fd) < 0)
	    IOException(currname,"close",errno);
	fd = -1;
    }
}

long long FileSet::getFileSize() const throw(IOException)
{
    if (fd >= 0) {
        struct stat64 statbuf;
        if (::fstat64(fd,&statbuf) < 0)
	    IOException(currname,"fstat",errno);
	return statbuf.st_size;
    }
    return 0;
}

/* static */
void FileSet::createDirectory(const string& name) throw(IOException)
{
    DLOG(("FileSet::createDirectory, name=") << name);
    if (name.length() == 0) throw IOException(name,"mkdir",ENOENT);

    struct stat64 statbuf;
    if (::stat64(name.c_str(),&statbuf) < 0) {
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
    if ((fd = ::open64(filename.c_str(),O_CREAT | O_EXCL | O_WRONLY,0444)) < 0) {
        _lastErrno = errno;
        throw IOException(filename,"open",errno);
    }
}

/**
 * Create a file using a time to create the name.
 * Return the time of the next file.
 */
UTime FileSet::createFile(UTime ftime,bool exact) throw(IOException)
{
    DLOG(("nidas::util::FileSet::createFile, ftime=")
	 << ftime.format(true,"%c"));
    closeFile();

    if (!exact && fileLength <= 366 * USECS_PER_DAY)
	ftime -= ftime.toUsecs() % fileLength;

    // break input time into date/time fields using GMT timezone
    currname = ftime.format(true,fullpath);

    DLOG(("nidas::util::FileSet:: fullpath=") << fullpath);
    DLOG(("nidas::util::FileSet:: currname=") << currname);

    // create the directory, and parent directories, if they don't exist
    string tmpname = getDirPortion(currname);
    if (tmpname != ".") try {
        createDirectory(tmpname);
    }
    catch (const IOException& e) {
        _lastErrno = e.getErrno();
        throw e;
    }

    ILOG(("creating: ") << currname);

    openFileForWriting(currname);

    nextFileTime = ftime + USECS_PER_SEC;	// add one sec
    nextFileTime += fileLength - (nextFileTime.toUsecs() % fileLength);

    DLOG(("nidas::util::FileSet:: nextFileTime=")
	 << nextFileTime.format(true,"%c"));
    newFile = true;

    return nextFileTime;
}


size_t FileSet::read(void* buf, size_t count) throw(IOException)
{
    newFile = false;
    if (fd < 0) openNextFile();		// throws EOFException
    ssize_t res = ::read(fd,buf,count);
    if (res <= 0) {
        if (!res) {
            closeFile();	// next read will open next file
            return res;
        }
	throw IOException(currname,"read",errno);
    }
    return res;
}

size_t FileSet::write(const void* buf, size_t count) throw(IOException)
{
    newFile = false;
    ssize_t res = ::write(fd,buf,count);
    if (res < 0) {
        _lastErrno = errno;
        throw IOException(currname,"write",errno);
    }
    return res;
}


void FileSet::openNextFile() throw(IOException)
{
    if (!initialized) {

	DLOG(("openNextFile, fullpath=") << fullpath);
	if (fullpath.length() > 0) {

	    fileset = matchFiles(startTime,endTime);

	    // If the first matched file is later than the
	    // start time, then we'll look to find an earlier
	    // file.
            string firstFile;
            string t1File = formatName(startTime);
	    if (fileset.size() > 0) firstFile = fileset.front();
	    if (fileset.size() == 0 || firstFile.compare(t1File) > 0) {
                UTime t1;
                // roll back a day
                if (fileLength > 366 * USECS_PER_DAY)
                    t1 = startTime - USECS_PER_DAY;
                else {
                    t1 = startTime;
                    t1 -= t1.toUsecs() % fileLength;
                }
                UTime t2 = startTime;
                list<string> files = matchFiles(t1,t2);
                if (files.size() > 0)  {
                    list<string>::const_reverse_iterator ptr = files.rbegin();
                    string fl = *ptr;
                    if (firstFile.length() == 0 || 
                        fl.compare(firstFile) < 0) fileset.push_front(fl);
                }
            }

	    if (fileset.size() == 0) throw IOException(fullpath,"open",ENOENT);
	}
	fileiter = fileset.begin();
	initialized = true;
    }
    closeFile();

    if (fileiter == fileset.end()) throw EOFException(currname,"open");
    currname = *fileiter++;
    ILOG(("opening: ") << currname);

    if (currname == "-") fd = 0;	// read from stdin
    else if ((fd = ::open64(currname.c_str(),O_RDONLY)) < 0)
    	throw IOException(currname,"open",errno);
    newFile = true;
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
    return t1.format(true,fullpath);
}

#if !defined(NIDAS_EMBEDDED)
void FileSet::checkPathFormat(const UTime& t1, const UTime& t2) throw(IOException)
{
    if (fullpath.find("%b") != string::npos) {
	string m1 = t1.format(true,"%b");
	string m2 = t2.format(true,"%b");
	if (::llabs(t1-t2) > 31 * USECS_PER_DAY || m1 != m2) 
	    throw IOException(
		string("FileSet: ") + fullpath,"search",
		"%b (alpha month) does not sort to time order");
    }

    vector<string::size_type> dseq;
    string::size_type di;
    di = fullpath.find("%Y");
    if (di == string::npos) di = fullpath.find("%y");
    if (di == string::npos) di = 0;
    dseq.push_back(di);

    di = fullpath.find("%m");
    if (di == string::npos) di = dseq[0];
    dseq.push_back(di);

    di = fullpath.find("%d");
    if (di == string::npos) di = dseq[1];
    dseq.push_back(di);

    di = fullpath.find("%H");
    if (di == string::npos) di = dseq[2];
    dseq.push_back(di);

    di = fullpath.find("%M");
    if (di == string::npos) di = dseq[3];
    dseq.push_back(di);

    di = fullpath.find("%S");
    if (di == string::npos) di = dseq[4];
    dseq.push_back(di);

    for (unsigned int i = 1; i < dseq.size(); i++)
        if (dseq[i] < dseq[i-1]) throw IOException(
		string("FileSet: ") + fullpath,"search",
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

    DLOG(("fullpath=") << fullpath);

    string t1path = t1.format(true,fullpath);
    string t2path = t2.format(true,fullpath);

    DLOG(("t1path=") << t1path);
    DLOG(("t2path=") << t2path);

    bool t1path_eq_t2path = t1path == t2path;

    // Check if there are time fields in the directory portion.
    // If so, it complicates things.

    string openedDir = getDirPortion(fullpath);

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
    DLOG(("fullpath=") << fullpath);

    // must execute this loop at least once with time of t1.
    // If t2 > t1 then increment time by dirDeltat each iteration,
    // but in the last iteration, time should be == t2.
    DIR *dirp = 0;
    for (UTime ftime = t1; ftime <= t2; ) {
	// currpath is the full path name of a file with a name
	// corresponding to time ftime
	string currpath = ftime.format(true,fullpath);
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

	DLOG(("fullpath=") << fullpath);
	// now take file portion of input path, and create regular expression
	string fileregex = 
	    string("^") + getFilePortion(fullpath) + string("$");
	    
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


