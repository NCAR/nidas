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

/**
 * If UTIME_BASIC_STREAM_IO is defined, then the UTime class
 * supports output to the std::basic_ostream<charT> template class.
 * If it is not defined, then UTime only supports output to
 * std::ostream, which is std::basic_ostream<char>.
 * The general support for basic_ostream<charT> would only be useful
 * if we want to support wide character output, which is not likely,
 * but hey, we'll leave the code in for now.
 */
#define UTIME_BASIC_STREAM_IO

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

    UTime& setFormat(const std::string& val)
    {
        _fmt = val;
	return *this;
    }

    const std::string& getFormat() const
    {
        if (_fmt.length() > 0) return _fmt;
        return getDefaultFormat();
    }

    /**
     * Static method to set the default output format.
     */
    static void setDefaultFormat(const std::string& val);

    static const std::string& getDefaultFormat();

    static void setTZ(const std::string& val);

    static std::string getTZ();

    struct tm tm(bool utc) const;

#ifdef UTIME_BASIC_STREAM_IO
    template<typename charT> friend
    std::basic_ostream<charT, std::char_traits<charT> >& operator << 
        (std::basic_ostream<charT, std::char_traits<charT>  >& os,
            const UTime& x);
#else
    friend std::ostream& operator<<(std::ostream& os, const UTime &x);
#endif

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
//	cout << nidas::util::setDefaultFormat("%H%M%S") << ut << endl;
//
#ifdef UTIME_BASIC_STREAM_IO
template<typename charT>
#endif
class UTime_stream_manip {

    std::string _fmt;
#ifdef UTIME_BASIC_STREAM_IO
    /** 
     * Pointer to function that does a manipulation on a ostream
     * with a string argument.
     */
    std::basic_ostream<charT,std::char_traits<charT> >& (*_f)(std::basic_ostream<charT,std::char_traits<charT> >&, const std::string&);
#else
    std::ostream& (*_f)(std::ostream&, const std::string&);
#endif

public:
    /**
     * Constructor of manipulator.  Pass it a function that
     * @param f A function that returns a reference to an ostream,
     *          with arguments of the ostream reference and a string.
     */
#ifdef UTIME_BASIC_STREAM_IO
    UTime_stream_manip(std::basic_ostream<charT,std::char_traits<charT> >&  (*f)(
        std::basic_ostream<charT,std::char_traits<charT> >&, const std::string&), const std::string& fmt): _fmt(fmt),_f(f)
    {
    }
#else
    UTime_stream_manip(std::ostream&  (*f)(
        std::ostream&, const std::string&), const std::string& fmt): _fmt(fmt),_f(f)
    {
    }
#endif

    /**
     * << operator of this manipulator on an ostream.
     * Invokes the function that was passed to the constructor.
     */
#ifdef UTIME_BASIC_STREAM_IO
    template<typename charTx>
    friend std::basic_ostream<charTx, std::char_traits<charTx> >& operator << 
        (std::basic_ostream<charTx, std::char_traits<charTx> >& os,const UTime_stream_manip<charTx>& m);
#else
    friend std::ostream& nidas::util::operator<<(std::ostream& os,
        const UTime_stream_manip& m);
#endif
};

#ifdef UTIME_BASIC_STREAM_IO

template<typename charT>
std::basic_ostream<charT, std::char_traits<charT> >& operator << 
    (std::basic_ostream<charT, std::char_traits<charT>  >& os, const nidas::util::UTime& x)
{
    return os << x.format(false);
}
template<typename charT>
std::basic_ostream<charT, std::char_traits<charT> >& operator <<
    (std::basic_ostream<charT, std::char_traits<charT> >& os,const nidas::util::UTime_stream_manip<charT>& m)
{
    return m._f(os,m._fmt);
}

/**
 * Internal function to set the default UTime output format on an ostream.
 * Typically this is invoked by an UTime_stream_manip << operator.
 */
template<typename charT>
std::basic_ostream<charT, std::char_traits<charT> >& 
    setOstreamDefaultFormat(std::basic_ostream<charT, std::char_traits<charT> >& os,
        const std::string& val)
{
    UTime::setDefaultFormat(val);
    return os;
}

/**
 * Internal function to set the UTime timezone on an ostream.
 * Passed to the manipulator constructor.
 */
template<typename charT>
std::basic_ostream<charT, std::char_traits<charT> >& 
    setOstreamTZ(std::basic_ostream<charT, std::char_traits<charT> >& os,
        const std::string& val)
{
    UTime::setTZ(val);
    return os;
}

/**
 * Function to set the default UTime output format on an ostream.
 */
template<typename charT>
UTime_stream_manip<charT> setDefaultFormat(const std::string& val)
{
    return UTime_stream_manip<charT>(&nidas::util::setOstreamDefaultFormat,val);
}

/**
 * Function to set the UTime timezone on an ostream.
 */
template<typename charT>
UTime_stream_manip<charT> setTZ(const std::string& val)
{
    return UTime_stream_manip<charT>(&nidas::util::setOstreamTZ,val);
}

#else

inline std::ostream& operator<<(std::ostream& os, const UTime &x)
{
    return os << x.format(false);
}

inline std::ostream& operator<<(std::ostream& os,
    const UTime_stream_manip& m)
{
    return m._f(os,m._fmt);
}

inline std::ostream& setOstreamDefaultFormat(std::ostream& os, const std::string& val)
{
    UTime::setDefaultFormat(val);
    return os;
}

inline std::ostream& setOstreamTZ(std::ostream& os, const std::string& val)
{
    UTime::setTZ(val);
    return os;
}

inline UTime_stream_manip setDefaultFormat(const std::string& val)
{
    return UTime_stream_manip(&nidas::util::setOstreamDefaultFormat,val);
}

inline UTime_stream_manip setTZ(const std::string& val)
{
    return UTime_stream_manip(&nidas::util::setOstreamTZ,val);
}


#endif

}}	// namespace nidas namespace util



#endif
