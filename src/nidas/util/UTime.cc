//              Copyright (C) by UCAR
//
// File       : $RCSfile: UTime.cpp,v $
// Revision   : $Revision$
// Directory  : $Source: /code/cvs/isa/src/lib/atdUtil/UTime.cpp,v $
// System     : PAM
// Date       : $Date$
//
// Description:
//

#include <nidas/util/UTime.h>

#include <sys/time.h>
#include <cstring>
#include <cstdlib>
#include <iomanip>

using namespace std;
using namespace nidas::util;

/* static */
Mutex UTime::_fmtMutex;
/* static */
string UTime::_defaultFormat("%c");

/* static */
Mutex UTime::_TZMutex;
/* static */
char *UTime::_TZ=0;

UTime::UTime()
{
    struct timeval tv;
    ::gettimeofday(&tv,0);
    _utime = (long long)tv.tv_sec * USECS_PER_SEC + tv.tv_usec;
}

//
// If utc is false, then these fields will be interpreted in the
// local time zone, otherwise UTC
//
UTime::UTime(bool utc, int year,int mon, int day, int hour, int minute,
	int sec, int usec)
{
    if (year > 1900) year -= 1900;	// convert to years since 1900
    else if (year < 50) year += 100;	// 6 means 2006

    struct tm tm;
    tm.tm_sec = sec;
    tm.tm_min = minute;
    tm.tm_hour = hour;
    tm.tm_mday = day;
    tm.tm_mon = mon - 1;	// convert to 0:11
    tm.tm_year = year;
    tm.tm_yday = -1;
    /* from mktime man page:
     * tm_isdst
     * A flag that indicates whether daylight saving time is in  effect
     * at the time described.  The value is positive if daylight saving
     * time is in effect, zero if it is not, and negative if the
     * information is not available.
     */
    tm.tm_isdst = (utc ? 0 : -1);

    _utime = fromTm(utc,&tm,usec);
}

//
// If utc is false, then these fields will be interpreted in the
// local time zone, otherwise UTC
//
UTime::UTime(bool utc, int year,int mon, int day, int hour, int minute,
	double dsec)
{
    if (year > 1900) year -= 1900;	// convert to years since 1900
    else if (year < 50) year += 100;

    struct tm tm;
    tm.tm_sec = (int)dsec;
    dsec -= tm.tm_sec;
    tm.tm_min = minute;
    tm.tm_hour = hour;
    tm.tm_mday = day;
    tm.tm_mon = mon - 1;	// convert to 0:11
    tm.tm_year = year;
    tm.tm_yday = -1;
    /* from mktime man page:
     * tm_isdst
     * A flag that indicates whether daylight saving time is in  effect
     * at the time described.  The value is positive if daylight saving
     * time is in effect, zero if it is not, and negative if the
     * information is not available.
     */
    tm.tm_isdst = (utc ? 0 : -1);

    _utime = fromTm(utc,&tm) + fromSecs(dsec);
}

UTime::UTime(bool utc, int year,int yday, int hour, int minute, double dsec)
{

    if (year > 1900) year -= 1900;	// convert to years since 1900
    else if (year < 50) year += 100;	

    struct tm tm;
    tm.tm_sec = (int)dsec;
    dsec -= tm.tm_sec;

    tm.tm_min = minute;
    tm.tm_hour = hour;
    tm.tm_mday = 1;
    tm.tm_mon = 0;
    tm.tm_year = year;
    tm.tm_yday = yday - 1;
    tm.tm_isdst = (utc ? 0 : -1);
    _utime = fromTm(utc,&tm) + fromSecs(dsec);
}

UTime::UTime(bool utc, int year,int yday, int hour, int minute, int sec, int usec)
{

    if (year > 1900) year -= 1900;	// convert to years since 1900
    else if (year < 50) year += 100;	

    struct tm tm;
    tm.tm_sec = sec;
    tm.tm_min = minute;
    tm.tm_hour = hour;
    tm.tm_mday = 1;
    tm.tm_mon = 0;
    tm.tm_year = year;
    tm.tm_yday = yday - 1;
    tm.tm_isdst = (utc ? 0 : -1);
    _utime = fromTm(utc,&tm,usec);
}

UTime::UTime(bool utc, const struct tm* tmp,int usecs):
	_utime(fromTm(utc,tmp,usecs))
{
}

/* static */
long long UTime::fromTm(bool utc,const struct tm* tmp, int usecs)
{
    time_t ut;
    struct tm tm = *tmp;

    int yday = -1;

    if (tm.tm_yday >= 0 && tm.tm_mon <= 0 && tm.tm_mday <= 1) {
	yday = tm.tm_yday;
        tm.tm_yday = -1;	// ::mktime ignores yday
	tm.tm_mon = 0;
	tm.tm_mday = 1;
    }

    if (utc) tm.tm_isdst = 0;
    ut = ::mktime(&tm);
    // if (ut == (time_t)-1) return ParseException();

#ifdef DEBUG
    cerr << "yr=" << tm.tm_year <<
  	" mn=" << tm.tm_mon <<
  	" dy=" << tm.tm_mday <<
  	" ydy=" << tm.tm_yday <<
  	" hr=" << tm.tm_hour <<
  	" mn=" << tm.tm_min <<
  	" sc=" << tm.tm_sec <<
  	" isdst=" << tm.tm_isdst <<
  	" utc=" << utc <<
  	" timezone=" << timezone <<
  	" ut=" << ut << endl;
#endif

    // utc means input time is to be interpreted as UTC, even
    // though the TZ environment var may not be GMT/UTC.
    if (utc) ut -= timezone;

    if (yday >= 0) {
	// this is close, but off by an hour if there is DST
	// change between Jan 1 and the time of interest.
	ut += yday * 86400;
	if (!utc) {			// correct for DST switch
	    int d1dst = tm.tm_isdst;
	    struct tm tm2;
	    localtime_r(&ut,&tm2);
	    int yddst = tm2.tm_isdst;
	    if (d1dst == 0 && yddst > 0) ut -= 3600;
	    else if (d1dst > 0 && yddst == 0) ut += 3600;
	}
    }
    return (long long) ut * USECS_PER_SEC + usecs;
}

struct tm* UTime::toTm(bool utc,struct tm* tmp,int *usecs) const
{

    long long ute = earlier(USECS_PER_SEC).toUsecs();
    time_t ut = ute  / USECS_PER_SEC;
    if (usecs) *usecs = _utime - ute;
    if (utc) return gmtime_r(&ut,tmp);
    else return localtime_r(&ut,tmp);
}

/* static */
UTime UTime::parse(bool utc,const string& str,int *ncharp) throw(ParseException)
{
    char cmon[32];
    int year,mon,day,hour,min;
    double dsec = 0.;
    int nchar = 0;

    year = 70;
    mon = day = 1;
    hour = min = 0;

    if (str.length() == 0 || str == "now") return UTime();

    // 97 Feb 1 11:22:33.4
    else if (sscanf(str.c_str(),
    	"%d %31[A-Za-z] %d %d:%d:%lf%n",
	&year,cmon,&day,&hour,&min,&dsec,&nchar) >= 6) {
	mon = month(cmon);
    }

    // 97 Feb 1 11:22
    else if (sscanf(str.c_str(),
    	"%d %31[A-Za-z] %d %d:%d%n",&year,cmon,&day,&hour,&min,&nchar) >= 5) {
	mon = month(cmon);
    }

    // 97 Feb 1 112233.4
    else if (sscanf(str.c_str(),
    	"%d %31[A-Za-z] %d %lf%n",&year,cmon,&day,&dsec,&nchar) >= 4) {
	mon = month(cmon);
	hour = (int) dsec / 10000;
	dsec -= hour * 10000;
	min = (int) dsec / 100;
	dsec -= min * 100;
    }

    // 97 Feb 1
    else if (sscanf(str.c_str(),
    	"%d %31[A-Za-z] %d%n",&year,cmon,&day,&nchar) >= 3) {
	mon = month(cmon);
    }

    // 97 2 1 11:22:33.4
    else if (sscanf(str.c_str(),"%d %d %d %d:%d:%lf%n",
    	&year,&mon,&day,&hour,&min,&dsec,&nchar) >= 6);

    // 97 2 1 11:22
    else if (sscanf(str.c_str(),"%d %d %d %d:%d%n",
    	&year,&mon,&day,&hour,&min,&nchar) >= 5);

    // 97 2 1 112233.4
    else if (sscanf(str.c_str(),
    	"%d %d %d %lf%n",&year,&mon,&day,&dsec,&nchar) >= 4) {
	hour = (int) dsec / 10000;
	dsec -= hour * 10000;
	min = (int) dsec / 100;
	dsec -= min * 100;
    }

    // 97 2 1
    else if (sscanf(str.c_str(),
    	"%d %d %d%n",&year,&mon,&day,&nchar) >= 3) {
    }

    // seconds since 1970
    else if (sscanf(str.c_str(),"%lf%n",&dsec,&nchar)) {
	return UTime((long long)trunc(dsec) * USECS_PER_SEC +
	    (long long)rint(fmod(dsec,1.0) * USECS_PER_SEC));
    }
    else throw ParseException(str,"year month day hour:min:sec");

    if (ncharp) *ncharp = nchar;

    return UTime(utc,year,mon,day,hour,min,dsec);
}

void UTime::set(const string& str,bool utc) throw(ParseException)
{
    *this = UTime::parse(utc,str);
}

/* static */
UTime UTime::parse(bool utc,const string& str, const string& fmt,int *ncharp)
	throw(ParseException)
{
    struct tm tm;
    memset(&tm,0,sizeof(tm));
    tm.tm_isdst = (utc ? 0 : -1);

    const char* cstr = str.c_str();
    const char* cptr = cstr;
    const char* cp2;
    //
    // Add support for %f format to parse fractional seconds,
    // Note that %F is a GNU extension, so we'll use %f.
    //
    string::size_type i0,i1;
    string::size_type flen = fmt.length();

    string newfmt;
    long usecs = 0;
    int ncharParsed = 0;

    for (i0 = 0;  (i1 = fmt.find('%',i0)) != string::npos; i0 = i1 ) {

	newfmt.append(fmt.substr(i0,i1-i0));	// append up to %

	// cerr << "i0=" << i0 << " i1=" << i1 << endl;

	i1++;	// points to one past the % sign
	if (flen > i1) {
	    string sfmt;
	    if (fmt[i1] == 'f') {
	        sfmt = "%3ld%n";
		i1++;
	    }
	    else if (flen > i1 + 1 && ::isdigit(fmt[i1]) && fmt[i1+1] == 'f') {
		sfmt = string("%") + fmt[i1] + "ld%n";
		i1 += 2;
	    }
	    else {
		newfmt.push_back('%');
		continue;
	    }
	    cp2 = strptime(cptr,newfmt.c_str(),&tm);
	    if (!cp2) throw ParseException(str,fmt);
            ncharParsed += cp2 - cptr;
	    cptr = cp2;
	    int nchar = 0;
	    if (sscanf(cptr,sfmt.c_str(),&usecs,&nchar) < 1)
		throw ParseException(str,fmt);
	    for (int i = nchar; i < 6; i++) usecs *= 10;
	    cptr += nchar;
            ncharParsed += nchar;
	    newfmt.clear();
	}
    }
    if (i0 < flen) newfmt.append(fmt.substr(i0));
    // cerr << "fmt=" << fmt << " newfmt=" << newfmt << endl;
    if (newfmt.length() > 0) {
    	cp2 = strptime(cptr,newfmt.c_str(),&tm);
	if (!cp2) throw ParseException(str,fmt);
        ncharParsed += cp2 - cptr;
    }

    // cerr << "tm.tm_mday=" << tm.tm_mday << " usecs=" << usecs << endl;

    if (ncharp) *ncharp = ncharParsed;

    return UTime(utc,&tm) + (long long)usecs;

}


string UTime::format(bool utc) const
{
    return format(utc,getFormat());
}

// method for conversion to string.
string UTime::format(bool utc, const string& fmt) const
{
    //
    // Add support for %nf format to print fractional seconds,
    // where n is number of digits after decimal point, 1-6
    // Note that %F is a GNU extension, so we'll use %f.
    //
    string newfmt; 
    string::size_type i0,i1;
    string::size_type flen = fmt.length();

    long long ute = earlier(USECS_PER_SEC).toUsecs();
    time_t ut = ute / USECS_PER_SEC;

    for (i0 = 0;  (i1 = fmt.find('%',i0)) != string::npos; i0 = i1 ) {

	// cerr << "i0=" << i0 << " i1=" << i1 << endl;
	newfmt.append(fmt.substr(i0,i1-i0));	// append up to %

	i1++;	// points to one past the % sign
	int n = -1;
	if (flen > i1) {
	    if (fmt[i1] == 'f') {
		i1++;
	        n = 3;
	    }
	    else if (flen > i1 + 1 && ::isdigit(fmt[i1]) && fmt[i1+1] == 'f') {
		n = fmt[i1] - '0';
		i1 += 2;
	    }
	}
	// cerr << "i0=" << i0 << " i1=" << i1 << " n=" << n << endl;
	if (n < 0) {		// not a %f or %nf, append %
	    newfmt.push_back('%');
	    continue;
	}
	    
	int div = USECS_PER_SEC;
	int mult = 1;
	for (int j=0; j < n; j++)
	    if (div > 1) div /= 10;
	    else mult *= 10;

	/*
	 * Round printed value.
	 */
	int modusecs = (_utime - ute) + div / 2;
	// cerr << "modusecs=" << modusecs <<
	// 	" div=" << div << " mult=" << mult << endl;
	if (modusecs > USECS_PER_SEC) {
	    ut++;		// round up
	    modusecs = 0;
	}
	modusecs = modusecs / div * mult;
	ostringstream ost;
	ost << setw(n) << setfill('0') << modusecs;
	// cerr << "n=" << n << " modusecs=" << modusecs << " ost=" << ost.str() << endl;
	newfmt.append(ost.str());
    }
    if (i0 < flen) newfmt.append(fmt.substr(i0));

    struct tm tm;

    if (utc) gmtime_r(&ut,&tm);
    else localtime_r(&ut,&tm);

#ifdef USE_STRFTIME
    char out[512];
    strftime(out,sizeof(out),newfmt.c_str(),&tm);
    return out;
#else
    // use std::time_put to format path into a file name
    // this converts the strftime %Y,%m type format descriptors
    // into date/time fields.
    ostringstream ostr;
    ostr.str("");

    const time_put<char> &timeputter(use_facet<time_put<char> >(locale()));
    timeputter.put(ostr.rdbuf(),ostr,' ',&tm,
        newfmt.data(),newfmt.data()+newfmt.length());

    return ostr.str();
#endif
}

/* static */
void UTime::setDefaultFormat(const string& val)
{
    Synchronized autolock(_fmtMutex);
    _defaultFormat = val;
}

const string& UTime::getDefaultFormat()
{
    Synchronized autolock(_fmtMutex);
    return _defaultFormat;
}

/* static */
void UTime::setTZ(const string& val)
{
    Synchronized autolock(_TZMutex);
    if (!_TZ) {					// initialize
	const char *envTZ = getenv("TZ");
	if (envTZ) {
	    _TZ = new char[4 + strlen(envTZ)];
	    sprintf(_TZ,"TZ=%s",envTZ);
	}
	else {
	    _TZ = new char[4];
	    strcpy(_TZ,"TZ=");
	}
    }

    char *oldtz = _TZ;
    if (val.length() == 0) {
	if (strlen(_TZ)==3) return;	// no change
	_TZ = new char[4];		// previous _TZ is deleted below
	strcpy(_TZ,"TZ=");
    }
    else {
	if (!strcmp(_TZ+3,val.c_str())) return;	// no change
	_TZ = new char[4 + val.length()];	// previous _TZ is deleted below
	sprintf(_TZ,"TZ=%s",val.c_str());
    }
    putenv(_TZ);			// change environment!
    ::tzset();
    delete [] oldtz;
}

string UTime::getTZ() 
{
    Synchronized autolock(_TZMutex);
    if (!_TZ) return string("");
    else return string(_TZ+3);
}

/*
 * Return month number in the range 1-12, corresponding to a string.
 */
int UTime::month(string monstr)
{
    if (monstr.length() > 0 && std::islower(monstr[0]))
        monstr[0] = std::toupper(monstr[0]);

    istringstream iss(monstr);

    // make this a static member?  Need to mutex it? Probably.
    const time_get<char>& tim_get = use_facet<time_get<char> >(iss.getloc());


    iss.imbue(iss.getloc());
    istreambuf_iterator<char> is_it01(iss);
    istreambuf_iterator<char> end;
    struct tm timestruct;
    ios_base::iostate errorstate = ios_base::goodbit;

    tim_get.get_monthname(is_it01, end, iss, errorstate, &timestruct);
    if (errorstate & ios_base::failbit) {
        cerr << "error in parsing " << monstr << endl;
        return 0;
    }
    return timestruct.tm_mon + 1;
}

UTime UTime::earlier(long long y) const
{
    if (_utime < 0) return UTime(_utime - y - (_utime % y));
    return UTime(_utime - (_utime % y));
}

/* static */
long long UTime::pmod(long long x, long long y)
{
    if (x < 0) return y + (x % y);
    else return x % y;
}

#ifdef USE_LOCALE_TIME
template<class charT, class Traits>
basic_istream<charT, Traits>& operator >> 
    (basic_istream<charT, Traits >& is, UTime& ut)
{
    ios_base::iostate err = 0;
    typedef istreambuf_iterator<charT,Traits> iter_type;


    try {
	typename basic_istream<charT, Traits>::sentry ipfx(is);
	if(ipfx) {
	     struct tm tm;
	     use_facet<time_get<charT,Traits> >(is.getloc())
		.get_date(is, istreambuf_iterator<charT,Traits>()
		,is, err, &tm);
	    ut = UTime(false,&tm);
	    if (err == ios_base::goodbit && *is == '.') {
		double fsecs;
	        is >> fsecs;
		ut += (long long)(fsecs * USECS_PER_SEC);
	    }
	}
    }
    catch(...) {
	bool flag = false;
	try { is.setstate(ios_base::failbit); }
	catch( ios_base::failure ) { flag= true; }
	if ( flag ) throw;
    }
 
    if ( err ) is.setstate(err);
    return is;
}

template<class charT, class Traits>
    basic_ostream<charT, Traits>& operator << 
    (basic_ostream<charT, Traits >& os, const UTime& ut)
{
    ios_base::iostate err = 0;

    try {
	typename basic_ostream<charT, Traits>::sentry opfx(os);
	if(opfx) {
	    string patt = ut.getFormat();
	    size_t pl = patt.length();
	    auto_ptr<charT> fmt(new charT[pl]);
	    use_facet<ctype<charT> >(os.getloc())
	    	.widen(patt.begin(),patt.end(),fmt);
	    struct tm tm = ut.tm(false);
	    if (use_facet<time_put<charT,ostreambuf_iterator<charT,Traits> > >
		 (os.getloc())
		.put(os,os,os.fill(),&tm,fmt,(fmt.get()+pl)).failed())
		err = ios_base::badbit;
		os.width(0);
	}
	// would like to output fractional seconds
    }
    catch(...) {
	bool flag = false;
	try {
	    os.setstate(ios_base::failbit);
	}
	catch( ios_base::failure ) { flag= true; }
	if ( flag ) throw;
    }
    if ( err ) os.setstate(err);
    return os;
}
#endif

