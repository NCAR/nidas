
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

// #define DEBUG

const char FileSet::pathSeparator = '/';	// this is unix, afterall

FileSet::FileSet() :
	timeputter(std::use_facet<std::time_put<char> >(std::locale())),
	fd(-1),startTime(0),endTime(0),
	fileiter(fileset.begin()),
	initialized(false),fileLength(UINT_MAX),nextFileTime(0),newFile(false)
{
}

/* Copy constructor. */
FileSet::FileSet(const FileSet& x):
	timeputter(std::use_facet<std::time_put<char> >(std::locale())),
	dir(x.dir),filename(x.filename),fullpath(x.fullpath),
	fd(-1),startTime(x.startTime),endTime(x.endTime),
	fileset(x.fileset),fileiter(fileset.begin()),
	initialized(x.initialized),
	fileLength(x.fileLength),nextFileTime(0),newFile(false)
{
}

FileSet::~FileSet()
{
    try {
        closeFile();
    }
    catch(const IOException& e) {}
}

void FileSet::closeFile() throw(IOException)
{
    if (fd >= 0) {
        if (::close(fd) < 0)
	    IOException(currname,"close",errno);
	fd = -1;
    }
    currname.clear();
}

/* static */
void FileSet::createDirectory(const string& name) throw(IOException)
{
#ifdef DEBUG
    cerr << "FileSet::createDirectory, name=" << name << endl;
#endif
    if (name.length() == 0) throw IOException(name,"mkdir",ENOENT);

    struct stat64 statbuf;
    if (::stat64(name.c_str(),&statbuf) < 0) {
        if (errno != ENOENT) throw IOException(name,"open",errno);

        // create parent directory if it doesn't exist
	string tmpname = getDirPortion(name);
	if (tmpname != ".") createDirectory(tmpname);  // recursive

        if (::mkdir(name.c_str(),0777) < 0)
            throw IOException(name,"mkdir",errno);
    }
}

/**
 * Create a file using a time to create the name.
 * Return the time of the next file.
 */
time_t FileSet::createFile(time_t ftime,bool exact) throw(IOException)
{
#ifdef DEBUG
    cerr << "nidas::util::FileSet::createFile, ftime=" << ftime << endl;
#endif

    closeFile();

    if (!exact && fileLength < INT_MAX && fileLength > 0)
	ftime -= ftime % fileLength;

    // break input time into date/time fields using GMT timezone
    struct tm gmt;
    gmtime_r(&ftime,&gmt);

    string path = makePath(getDir(),getFileName());

#ifdef DEBUG
    cerr << "nidas::util::FileSet:: path=" << path << endl;
#endif

    // use std::time_put to format path into a file name
    // this converts the strftime %Y,%m type format descriptors
    // into date/time fields.
    ostringstream ostr;
    ostr.str("");
    timeputter.put(ostr.rdbuf(),ostr,' ',&gmt,
        path.data(),path.data()+path.length());

    currname = ostr.str();
#ifdef DEBUG
    cerr << "nidas::util::FileSet:: currname=" << currname << endl;
#endif

    // create the directory, and parent directories, if they don't exist
    string tmpname = getDirPortion(currname);
    if (tmpname != ".") createDirectory(tmpname);

    Logger::getInstance()->log(LOG_INFO,"creating: %s",
    	currname.c_str());

    if ((fd = ::open64(currname.c_str(),O_CREAT | O_EXCL | O_WRONLY,0444)) < 0)
        throw IOException(currname,"open",errno);

    if (fileLength < INT_MAX && fileLength > 0)
        nextFileTime = (((ftime + 1) / fileLength) + 1) * fileLength;
    else nextFileTime = INT_MAX;

#ifdef DEBUG
    cerr << "nidas::util::FileSet:: nextFileTime=" << nextFileTime << endl;
#endif
    newFile = true;

    return nextFileTime;
}


ssize_t FileSet::read(void* buf, size_t count) throw(IOException)
{
    newFile = false;
    if (fd < 0) openNextFile();		// throws EOFException
    ssize_t res = ::read(fd,buf,count);
    if (res <= 0) {
        if (!res) fd = -1;	// next read will open next file
	else throw IOException(currname,"read",errno);
    }
    return res;
}

ssize_t FileSet::write(const void* buf, size_t count) throw(IOException)
{
    newFile = false;
    ssize_t res = ::write(fd,buf,count);
    if (res < 0) throw IOException(currname,"write",errno);
    return res;
}


void FileSet::openNextFile() throw(IOException)
{
    if (!initialized) {

	fullpath = makePath(getDir(),getFileName());
	if (fullpath.length() > 0) {

	    fileset = matchFiles(startTime,endTime);

	    // If the first matched file is later than the
	    // start time, then we'll look to find an earlier
	    // file.
	    if (fileset.size() > 0) {
	        string firstFile = fileset.front();
		string t1File = formatName(startTime);
		if (firstFile.compare(t1File) > 0) {
		    time_t t1;
		    // roll back a day
		    if (fileLength == INT_MAX || fileLength == 0)
		    	t1 = startTime - 86400;
		    else {
		        t1 = (startTime / fileLength) * fileLength;
			if (t1 == startTime) t1 -= fileLength;
		    }
		    time_t t2 = startTime;
		    list<string> files = matchFiles(t1,t2);
		    if (files.size() > 0)  {
			list<string>::const_reverse_iterator ptr = files.rbegin();
			string f1 = *ptr;
			if (f1.compare(firstFile) < 0) fileset.push_front(f1);
		    }
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
    Logger::getInstance()->log(LOG_INFO,"opening: %s",
    	currname.c_str());

    if (currname == "-") fd = 0;	// read from stdin
    else if ((fd = ::open64(currname.c_str(),O_RDONLY)) < 0)
    	throw IOException(currname,"open",errno);
    newFile = true;
}

/* static */
string FileSet::getDirPortion(const string& path)
{
    size_t lslash = path.rfind(pathSeparator);
    if (lslash == string::npos) return ".";
    else if (lslash == 0) return string(1,pathSeparator);
    else return path.substr(0,lslash);
}

/* static */
string FileSet::getFilePortion(const string& path)
{
    size_t lslash = path.rfind(pathSeparator);
    if (lslash == string::npos) return path;
    else return path.substr(lslash+1);
}

/* static */
string FileSet::makePath(const string& dir,const string& file)
{
    if (dir.length() == 0 || dir == ".") return file;
    return dir + pathSeparator + file;
}

string FileSet::formatName(time_t t1)
{
    struct tm gmt;
    gmtime_r(&t1,&gmt);
    ostringstream ostr;
    timeputter.put(ostr.rdbuf(),ostr,' ',&gmt,
	fullpath.data(),fullpath.data()+fullpath.length());
    return ostr.str();
}

#if !defined(NIDAS_EMBEDDED)
void FileSet::checkPathFormat(time_t t1, time_t t2) throw(IOException)
{
    if (fullpath.find("%b") != string::npos) {
        UTime ut(t1);
	string m1 = ut.format(true,"%b");
	ut.setFromSecs(t2);
	string m2 = ut.format(true,"%b");
	if (::llabs(t1-t2) > 31 * 86400 || m1 != m2) 
	    throw IOException(
		string("FileSet: ") + fullpath,"search",
		"%b (alpha month) does not sort to time order");
    }

    vector<size_t> dseq;
    size_t di;
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

    for (size_t i = 1; i < dseq.size(); i++)
        if (dseq[i] < dseq[i-1]) throw IOException(
		string("FileSet: ") + fullpath,"search",
		"file names do not sort to time order");
}
#endif

list<string> FileSet::matchFiles(time_t t1, time_t t2) throw(IOException)
{

    set<string> matchedFiles;
    string openedDir;
    DIR *dirp = 0;
    struct tm gmt1;
    struct tm gmt2;
    ostringstream ostr;
    time_t requestDeltat = t2 - t1;

    // Check that format sorts correctly
    checkPathFormat(t1,t2);


#ifdef DEBUG
    cerr << "fullpath=" << fullpath << endl;
#endif

    gmtime_r(&t1,&gmt1);
    ostr.str("");
    timeputter.put(ostr.rdbuf(),ostr,' ',&gmt1,
	fullpath.data(),fullpath.data()+fullpath.length());

    // matched filenames must compare to be greater than or equal to t1path
    string t1path = ostr.str();
#ifdef DEBUG
    cerr << "t1path=" << t1path << endl;
#endif

    gmtime_r(&t2,&gmt2);
    ostr.str("");
    timeputter.put(ostr.rdbuf(),ostr,' ',&gmt2,
	fullpath.data(),fullpath.data()+fullpath.length());
    // matched filenames must compare to be less than to t2path
    string t2path = ostr.str();
#ifdef DEBUG
    cerr << "t2path=" << t2path << endl;
#endif

    bool t1path_eq_t2path = t1path == t2path;

#ifdef DEBUG
    cerr << "ostr.str()=" << ostr.str() << endl;
#endif

    // Check if there are time fields in the directory portion.
    // If so, it complicates things.

    openedDir = getDirPortion(fullpath);

    // use std::time_put to format path into a file name
    // this converts the strftime %Y,%m type format descriptors
    // into date/time fields.
#ifdef DEBUG
    cerr << "openedDir=" << openedDir << endl;
#endif
    gmtime_r(&t1,&gmt1);
    ostr.str("");
    timeputter.put(ostr.rdbuf(),ostr,' ',&gmt1,
	openedDir.data(),openedDir.data()+openedDir.length());

    time_t dirDeltat = 365 * 86400;
    // Check if file name has changed via the above time formatting,
    // therefore there must be % fields in it.
    if (ostr.str() != openedDir) {
	if (openedDir.find("%H") != string::npos) dirDeltat = 3600;
	else if (openedDir.find("%d") != string::npos) dirDeltat = 86400;
	else if (openedDir.find("%m") != string::npos) dirDeltat = 28 * 86400;
	else if (openedDir.find("%y") != string::npos) dirDeltat = 365 * 86400;
	else if (openedDir.find("%Y") != string::npos) dirDeltat = 365 * 86400;
	else dirDeltat = 3600;	// wierd
    }
    openedDir.clear();

#ifdef DEBUG
    cerr << "dirDeltat=" << dirDeltat << endl;
    cerr << "fullpath=" << fullpath << endl;
#endif

    // must execute this loop at least once with time of t1.
    // If t2 > t1 then increment time by dirDeltat each iteration,
    // but in the last iteration, time should be == t2.
    for ( ; t1 <= t2; ) {

	gmtime_r(&t1,&gmt1);
	ostr.str("");
	timeputter.put(ostr.rdbuf(),ostr,' ',&gmt1,
	    fullpath.data(),fullpath.data()+fullpath.length());

	// currpath is the full path name of a file with a name
	// corresponding to time t1
	string currpath = ostr.str();
#ifdef DEBUG
	cerr << "currpath=" << currpath << endl;
#endif

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

#ifdef DEBUG
	cerr << "fullpath=" << fullpath << endl;
#endif
	// now take file portion of input path, and create regular expression
	string fileregex = 
	    string("^") + getFilePortion(fullpath) + string("$");
	    
#ifdef DEBUG
	cerr << "fileregex=" << fileregex << endl;
#endif

	// replace time fields with corresponding regular expressions

	// This ensures that if we jump across a month when incrementing
	// by dirDeltat that we match files for the intermediate month.
	// dirDeltat is never more than a year, so we don't have to
	// substitute for %y or %Y.
	if (requestDeltat > 28 * 86400) {
	    replaceChars(fileregex,"%m","[0-1][0-9]");
	    replaceChars(fileregex,"%b","[a-zA-Z][a-zA-Z][a-zA-Z]");
	}
	else if (requestDeltat > 86400)
	    replaceChars(fileregex,"%d","[0-3][0-9]");
	replaceChars(fileregex,"%H","[0-2][0-9]");
	replaceChars(fileregex,"%M","[0-5][0-9]");
	replaceChars(fileregex,"%S","[0-5][0-9]");
#ifdef DEBUG
	cerr << "fileregex=" << fileregex << endl;
#endif

	// use std::time_put to format path into a file name
	ostr.str("");
	timeputter.put(ostr.rdbuf(),ostr,' ',&gmt1,
	    fileregex.data(),fileregex.data()+fileregex.length());
	fileregex = ostr.str();
#ifdef DEBUG
	cerr << "fileregex=" << fileregex << endl;
#endif

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
#ifdef DEBUGX
	    cerr << "dp->d_name=" << dp->d_name << endl;
#endif
	    if ((regstatus = regexec(&preg,dp->d_name,0,0,0)) == 0) {
		string matchfile = makePath(currdir,dp->d_name);
#ifdef DEBUG
		cerr << "regexec matchfile=" << matchfile << endl;
#endif
	        if (t1path.compare(matchfile) <= 0) {
		    if (t2path.compare(matchfile) > 0 ||
		    	(t1path_eq_t2path && t2path.compare(matchfile) >= 0)) {
#ifdef DEBUG
			cerr << "regexec & time matchfile=" << matchfile << endl;
#endif
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

	if (t1 == t2) break;
	t1 += dirDeltat;
	if (t1 > t2) t1 = t2;
    }
    if (dirp) closedir(dirp);
#ifdef DEBUG
    cerr << "matchedFiles.size()=" << matchedFiles.size() << endl;
#endif
    return list<string>(matchedFiles.begin(),matchedFiles.end());
}

/* static */
void FileSet::replaceChars(string& in,const string& pat, const string& rep) 
{
    unsigned int patpos;
    while ((patpos = in.find(pat,0)) != string::npos)
    	in.replace(patpos,pat.length(),rep);
}


