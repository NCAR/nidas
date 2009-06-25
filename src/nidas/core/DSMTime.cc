
#include <nidas/core/DSMTime.h>

using namespace nidas::core;

bool nidas::core::sleepUntil(unsigned int periodMsec,unsigned int offsetMsec)
    throw(nidas::util::IOException)
{
    struct timespec sleepTime;
    /*
     * sleep until an even number of periodMsec since 
     * creation of the universe (Jan 1, 1970 0Z).
     */
    dsm_time_t tnow = getSystemTime();
    unsigned int mSecVal =
      periodMsec - (unsigned int)((tnow / USECS_PER_MSEC) % periodMsec) + offsetMsec;

    sleepTime.tv_sec = mSecVal / MSECS_PER_SEC;
    sleepTime.tv_nsec = (mSecVal % MSECS_PER_SEC) * NSECS_PER_MSEC;
    if (::nanosleep(&sleepTime,0) < 0) {
	if (errno == EINTR) return true;
	throw nidas::util::IOException("Looper","nanosleep",errno);
    }
    return false;
}

