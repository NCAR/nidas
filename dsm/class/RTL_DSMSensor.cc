/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision: 671 $

    $LastChangedBy: maclean $

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <bits/pthreadtypes.h>

#include <iostream>

#include <RTL_DSMSensor.h>
#include <RTL_DevIoctlStore.h>
#include <dsm_sample.h>
#include <SamplePool.h>

using namespace std;
using namespace dsm;

CREATOR_ENTRY_POINT(RTL_DSMSensor)

RTL_DSMSensor::RTL_DSMSensor() :
    DSMSensor(),devIoctl(0),infifofd(-1),outfifofd(-1),
    BUFSIZE(8192),bufhead(0),buftail(0),samp(0)
{
}

RTL_DSMSensor::RTL_DSMSensor(const string& devnameArg) :
    DSMSensor(devnameArg),devIoctl(0),infifofd(-1),outfifofd(-1),
    BUFSIZE(8192),bufhead(0),buftail(0),samp(0)
{

    string::reverse_iterator ri;
    for (ri = devname.rbegin(); ri != devname.rend(); ++ri)
        if (!isdigit(*ri)) break;
    string::iterator pi = ri.base();

    // cerr << "*pi=" << *pi << " diff=" << pi-devname.begin() << endl;

    // string prefix = devname.substr(0,pi-devname.begin());
    prefix = string(devname.begin(),pi);

    string portstr(pi,devname.end());

    portNum = atoi(portstr.c_str());

    inFifoName = prefix + "_in_" + portstr;
    outFifoName = prefix + "_out_" + portstr;

    buffer = new char[BUFSIZE];
}

/**
 * Destructor.  This also does a close, which is a bit of an
 * issue, since close can throw IOException, and we want to
 * avoid throwing exceptions in destructors.  For now
 * we'll catch it a write it to cerr, which should be
 * replaced by a actual logging object.
 */
RTL_DSMSensor::~RTL_DSMSensor()
{
    delete [] buffer;

    cerr << "~RTL_DSMSensor()" << endl;
    try {
      close();
    }
    catch(atdUtil::IOException& ioe) {
       cerr << ioe.what() << endl;
    }

}

void RTL_DSMSensor::open(int flags) throw(atdUtil::IOException)
{
  
    devIoctl = RTL_DevIoctlStore::getInstance()->getDevIoctl(prefix,portNum);
    if (devIoctl) devIoctl->open();

    /* may have to defer opening the fifos until sending the ioctl requests */
    int accmode = flags & O_ACCMODE;

    if (accmode == O_RDONLY || accmode == O_RDWR) {
	infifofd = ::open(inFifoName.c_str(),O_RDONLY);
	if (infifofd < 0) throw atdUtil::IOException(inFifoName,"open",errno);
    }

    if (accmode == O_WRONLY || accmode == O_RDWR) {
	outfifofd = ::open(outFifoName.c_str(),O_WRONLY);
	if (outfifofd < 0) throw atdUtil::IOException(outFifoName,"open",errno);
    }

}


void RTL_DSMSensor::close() throw(atdUtil::IOException)
{
    if (infifofd >= 0) {
	cerr << "closing in fifo" << endl;
        ::close(infifofd);
    }
    infifofd = -1;

    if (outfifofd >= 0) {
	cerr << "closing out fifo" << endl;
        ::close(outfifofd);
    }
    outfifofd = -1;

    if (devIoctl) devIoctl->close();
    devIoctl = 0;
}

void RTL_DSMSensor::ioctl(int request, void* buf, size_t len) 
	throw(atdUtil::IOException)
{
    if (!devIoctl) throw atdUtil::IOException(getDeviceName(),"ioctl",
    	"no ioctl associated with device");

    devIoctl->ioctl(request,portNum,buf,len);
}

void RTL_DSMSensor::ioctl(int request, const void* buf, size_t len) 
	throw(atdUtil::IOException)
{
    if (!devIoctl) throw atdUtil::IOException(getDeviceName(),"ioctl",
    	"no ioctl associated with device");

    devIoctl->ioctl(request,portNum,buf,len);
}

ssize_t RTL_DSMSensor::read(void *buf, size_t len) throw(atdUtil::IOException)
{
    ssize_t l = ::read(infifofd,buf,len);
    if (l < 0) throw atdUtil::IOException(inFifoName,"read",errno);
    return l;
}

ssize_t RTL_DSMSensor::write(const void *buf, size_t len) throw(atdUtil::IOException)
{
    size_t n = ::write(outfifofd,buf,len);
    if (n < 0) throw atdUtil::IOException(outFifoName,"write",errno);
    return n;
}

dsm_sample_time_t RTL_DSMSensor::readSamples()
	throw (SampleParseException,atdUtil::IOException)
{
    size_t len = BUFSIZE - bufhead;	// length to read
    size_t rlen;			// read result
    dsm_sample_time_t tt = maxValue(tt);

    rlen = read(buffer+bufhead,len);
    bufhead += rlen;

    // process all data in buffer, pass samples onto clients
    for (;;) {
        if (samp) {
	    rlen = bufhead - buftail;	// bytes available in buffer
	    len = sampDataToRead;	// bytes left to fill sample
	    if (rlen < len) len = rlen;
	    memcpy(sampDataPtr,buffer+buftail,len);
	    buftail += len;
	    sampDataPtr += len;
	    sampDataToRead -= len;
	    if (!sampDataToRead) {		// done with sample
		tt = samp->getTimeTag();	// return last time tag read
	        distribute(samp);
		samp = 0;
	    }
	    else break;				// done with buffer
	}
	// Read the header of the next sample
        if (bufhead - buftail <
		(signed)(len = SIZEOF_DSM_SMALL_SAMPLE_HEADER))
		break;

	struct dsm_small_sample header;	// temporary header to read into
	memcpy(&header,buffer+buftail,len);
	buftail += len;

	len = header.length;
	samp = SamplePool<CharSample>::getInstance()->getSample(len);
	samp->setTimeTag(header.timetag);
	samp->setDataLength(len);
	samp->setId(getId());	// set sample id to id of this sensor
	sampDataPtr = (char*) samp->getVoidDataPtr();
	sampDataToRead = len;
    }

    // shift data down. There shouldn't be much - less than a header's worth.
    register char* bp;
    for (bp = buffer; buftail < bufhead; ) 
    	*bp++ = *(buffer + buftail++);

    bufhead = bp - buffer;
    buftail = 0;
    return tt;
}

