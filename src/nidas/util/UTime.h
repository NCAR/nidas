/*              Copyright (C) 1989,90,91,92,93,94 by UCAR
 *
 * File       : $RCSfile: UTime.h,v $
 * Revision   : $Revision: 1.2 $
 * Directory  : $Source: /code/cvs/isa/src/lib/atdUtil/UTime.h,v $
 * System     : ASTER
 * Author     : Gordon Maclean
 * Date       : $Date: 2006/03/28 05:51:41 $
 *
 * Description:
 *
 */

#ifndef NIDAS_UTIL_UTIME_H
#define NIDAS_UTIL_UTIME_H

#include <sys/types.h>
#include <ctime>
#include <iostream>
#include <cctype>
#include <cmath>
#include <climits>

#include <locale>
#include <string>

#include <nidas/util/ThreadSupport.h>
#include <nidas/util/ParseException.h>

#ifndef SECS_PER_HOUR
#define SECS_PER_HOUR 3600
#endif

#ifndef SECS_PER_DAY
#define SECS_PER_DAY 86400
#endif

#ifndef MSECS_PER_SEC
#define MSECS_PER_SEC 1000
#endif

#ifndef MSECS_PER_DAY
#define MSECS_PER_DAY 86400000
#define MSECS_PER_HALF_DAY 43200000
#endif

#ifndef USECS_PER_SEC
#define USECS_PER_SEC 1000000
#endif

#ifndef USECS_PER_MSEC
#define USECS_PER_MSEC 1000
#endif

#ifndef USECS_PER_HOUR
#define USECS_PER_HOUR 3600000000LL
#endif


#ifndef USECS_PER_DAY
#define USECS_PER_DAY 86400000000LL
#define USECS_PER_HALF_DAY 43200000000LL
#endif

namespace nidas { namespace util {

class UTime {
public:

    /**
     * A very early time.
     */
    // static const long long BIGBANG = LLONG_MIN;

    /**
     * No-arg constructor initializes to current time.
     */
    UTime( );

    /**
     * Constructor.
     * @param t Microseconds since Jan 1, 1970 00:00 UTC
     */
    UTime(long long t) { _utime = t; }

    /**
     * Constructor.
     * @param t Seconds since Jan 1, 1970 00:00 UTC
     */
    UTime(time_t t) { _utime = fromSecs(t); }

    /**
     * Constructor.
     * @param t Seconds since Jan 1, 1970 00:00 UTC
     */
    UTime(double t) { _utime = fromSecs(t); }

    UTime(bool utc,const struct tm* tmp,int usecs = 0);

    UTime(bool utc, int year,int mon, int day,int hour, int min, double sec);

    UTime(bool utc, int year,int mon, int day,int hour, int min, int sec, int usecs);

    UTime(bool utc, int year,int yday,int hour, int min, double sec);

    UTime(bool utc, int year,int yday,int hour, int min, int sec, int usecs = 0);

    void setFromSecs(time_t t) { _utime = fromSecs(t); }

    struct tm* toTm(bool utc,struct tm* tmp, int* usecs = 0) const;

    static long long fromTm(bool utc,const struct tm* tmp, int usecs = 0);

    void set(const std::string& string,bool utc=false) 
    	throw(ParseException);

    static UTime parse(bool utc,const std::string& string,int* nparsed=0)
    	throw(ParseException);

    static UTime parse(bool utc,const std::string& string,
    	const std::string& format,int* nparsed=0) throw(ParseException);

    std::string format(bool utc,const std::string& fmt) const;

    std::string format(bool utc) const;

    UTime& operator=(const UTime& u)
    {
        _utime = u._utime;
        _fmt = u._fmt;
	return *this;
    }

    UTime& operator=(long long u) { _utime = u; return *this; }

    UTime operator+ (long long u) const { return UTime(_utime + u); }

    UTime operator- (long long u) const { return UTime(_utime - u); }

    long long operator- (const UTime& u) const { return _utime - u._utime; }

    UTime& operator+=(long long u) { _utime += u; return *this; }

    UTime& operator-=(long long u) { _utime -= u; return *this; }

    bool operator<(const UTime& u) const { return _utime < u._utime; }

    bool operator<=(const UTime& u) const { return _utime <= u._utime; }

    bool operator>(const UTime& u) const { return _utime > u._utime; }

    bool operator>=(const UTime& u) const { return _utime >= u._utime; }

    bool operator==(const UTime& u) const { return _utime == u._utime; }

    UTime earlier(long long y) const;

    static int month(std::string monstr);

    // conversion operator
    // operator long long() const { return _utime; }

    long long toUsecs() const
    {
	return _utime;
    } 

    double toDoubleSecs() const
    {
	// should work for positive and negative.
	return (long)(_utime/USECS_PER_SEC) +
		(double)(_utime % USECS_PER_SEC) / USECS_PER_SEC;
    } 

    unsigned long toSecs() const
    {
	return (_utime + USECS_PER_SEC / 2) / USECS_PER_SEC;
    } 

    UTime& setFormat(std::string& val)
    {
        _fmt = val;
	return *this;
    }

    const std::string& getFormat() const
    {
        if (_fmt.length() > 0) return _fmt;
        return getDefaultFormat();
    }

    static void setDefaultFormat(const std::string& f);

    static const std::string& getDefaultFormat();

    static void setTZ(const char *TZ);

    static std::string getTZ();

    struct tm tm(bool local) const;

    friend std::ostream& operator<<(std::ostream&, const UTime &);

    static std::ostream& setDefaultFormat(std::ostream& os, const std::string& f);

    static std::ostream& setTZ(std::ostream& os, const char *tz);

    /**
     * Positive modulus:  if x > 0, returns x % y
     *                    else  y + (x % y)
     * Useful for time calculation on negative times, for
     * example:
     *	pmod((long long)ut,USECS_PER_DAY)
     * gives microseconds since 00:00 of day whether 
     * date is negative or positive.
     */
    static long long pmod(long long x, long long y);

protected:

    /**
     * Convert a unsigned value in seconds to a value in the units of UTime.
     */
    static long long fromSecs(long x)
    {
        return (long long)x * USECS_PER_SEC;
    } 

    /**
     * Convert a double value in seconds to a value in the units of UTime.
     */
    static long long fromSecs(double x)
    {
	double xf = floor(x);
        return (long long)xf * USECS_PER_SEC +
		(int)rint((x-xf) * USECS_PER_SEC) ;
    } 

    static double toDoubleSecs(long long x)
    {
	// should work for positive and negative.
	return (long)(x/USECS_PER_SEC) + (double)(x % USECS_PER_SEC) / USECS_PER_SEC;
    } 

    static unsigned long toSecs(long long x)
    {
	return x / USECS_PER_SEC;
    } 

private:
    long long _utime;

    std::string _fmt;

    static std::string _defaultFormat;

    static Mutex _fmtMutex;

    static char *_TZ;

    static Mutex _TZMutex;

};

//
// class for changing output format of UTime on ostream, in a way
// like the standard stream manipulator classes.
// This supports doing:
//	cout << UTsetDefaultFormat("%H%M%S") << ut << endl;
//	cout << ut.setFormat("%H%M%S") << endl;
//
class UTime_stream_manip1 {
    std::string _fmt;
    std::ostream& (*_f)(std::ostream&, const std::string&);
public:
    UTime_stream_manip1(std::ostream & (*f)(std::ostream&, const std::string&),
	const std::string& fmt): _fmt(fmt),_f(f) {}

    friend std::ostream& operator<<(std::ostream& os, 
				  const UTime_stream_manip1& m) {
	return m._f(os,m._fmt); }
};

class UTime_stream_manip2 {
    std::string _fmt;
    std::ostream& (*_f)(std::ostream&, const char*);
public:
    UTime_stream_manip2(std::ostream & (*f)(std::ostream&, const char*),
	const char* fmt): _fmt(fmt),_f(f) {}

    friend std::ostream& operator<<(std::ostream& os, 
				  const UTime_stream_manip2& m) {
	return m._f(os,m._fmt.c_str()); }
};

UTime_stream_manip1 UTsetDefaultFormat(const std::string& fmt);
UTime_stream_manip2 UTsetTZ(const char *fmt);

}}	// namespace nidas namespace util

#endif
