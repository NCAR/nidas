/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/RTL_DevIoctl.h>

#include <nidas/rtlinux/ioctl_fifo.h>

#include <unistd.h>
#include <fcntl.h>
#include <cstring> // memcpy()
#include <cerrno>
#include <sys/uio.h>

#include <iostream>
#include <memory> // auto_ptr<>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

RTL_DevIoctl::RTL_DevIoctl(const string& prefixArg, int boardNumArg,
	int firstDevArg):
    prefix(prefixArg),
    boardNum(boardNumArg),firstDevNum(firstDevArg),numDevs(-1),
    inputFifoName(makeInputFifoName(prefix,boardNum)),
    outputFifoName(makeOutputFifoName(prefix,boardNum)),
    infifofd(-1),outfifofd(-1),usageCount(0)
{
}

RTL_DevIoctl::~RTL_DevIoctl()
{
    close();
}

void RTL_DevIoctl::open() throw(n_u::IOException)
{
    
    n_u::Synchronized autosync(ioctlMutex);
    /*
     * A RTL_DevIoctl can be shared by one or more RTL_Sensors.
     * Therefore we only do the actual FIFO open on the
     * first call to this method.
     */
    if (!usageCount++) {
	if (infifofd < 0)
	    infifofd = ::open(inputFifoName.c_str(),O_RDONLY);
	if (infifofd < 0) throw n_u::IOException(inputFifoName,"open",errno);

	if (outfifofd < 0)
	    outfifofd = ::open(outputFifoName.c_str(),O_WRONLY);
	if (outfifofd < 0) throw n_u::IOException(outputFifoName,"open",errno);
    }
}

void RTL_DevIoctl::close()
{
    n_u::Synchronized autosync(ioctlMutex);
    if (!(--usageCount)) {
	if (infifofd >= 0) ::close(infifofd);
	infifofd = -1;
	if (outfifofd >= 0) ::close(outfifofd);
	outfifofd = -1;
    }
}


int RTL_DevIoctl::getNumDevs() throw(n_u::IOException)
{

    if (numDevs > 0) return numDevs;
    if (!usageCount) open();
    ioctl(GET_NUM_PORTS,0,&numDevs,sizeof(numDevs));

#ifdef DEBUG
    std::cerr << dec << "numDevs=" << numDevs << std::endl;
#endif

    return numDevs;
}


void RTL_DevIoctl::ioctl(int cmd, int dev, void *buf, size_t len)
    throw(n_u::IOException)
{
    // allow only one ioctl at a time
    n_u::Synchronized autosync(ioctlMutex);

    static const unsigned char etx = '\003';

    int port = dev - firstDevNum;

    unsigned char* wbuf;
    unsigned char* wptr;
    size_t wlen = sizeof(cmd) + sizeof(port) + sizeof(len) + sizeof(etx);

    if (_IOC_DIR(cmd) & _IOC_WRITE) wlen += len;
#ifdef DEBUG
    cerr << "wlen=" << wlen << " len=" << len << endl;
#endif

    wptr = wbuf = new unsigned char[wlen];

    memcpy(wptr,&cmd,sizeof(cmd));
    wptr += sizeof(cmd);

    memcpy(wptr,&port,sizeof(port));
    wptr += sizeof(port);

    memcpy(wptr,&len,sizeof(len));
    wptr += sizeof(len);

    if (_IOC_DIR(cmd) & _IOC_WRITE) {
#ifdef DEBUG
	cerr << "_IOC_WRITE, len=" << len << endl;
#endif
	memcpy(wptr,buf,len);
	wptr += len;
    }

    *wptr++ = etx;

    ssize_t wres;
    if ((wres = write(outfifofd, wbuf,wlen)) < 0) {
	delete [] wbuf;
	throw n_u::IOException(outputFifoName,"write",errno);
    }
    delete [] wbuf;

    /* read back the status errno */
    int errval;
    ssize_t lread;
    if ((lread = read(infifofd,&errval,sizeof(errval))) < 0)
	throw n_u::IOException(inputFifoName,"read",errno);
#ifdef DEBUG
    cerr << "errval lread=" << lread << endl;
#endif

    if (lread != sizeof(errval))
    	throw n_u::IOException(inputFifoName,
	"ioctl read errval","wrong len read");

    size_t inlen;
    if ((lread = read(infifofd,&inlen,sizeof(inlen))) < 0)
	throw n_u::IOException(inputFifoName,"read",errno);
    if (lread != sizeof(inlen))
    	throw n_u::IOException(inputFifoName,
	"ioctl read inlen","wrong len read");

    // Device is reporting an ioctl error. Read the message and ETX
    // and throw the IOException.
    if (errval != 0) {
	if (inlen > 256)
	    throw n_u::IOException(inputFifoName,"read",
	    	"error message length out of range");
        auto_ptr<char> msgbuf(new char[inlen]);
	char* bufptr = msgbuf.get();
	while (inlen > 0) {
	    if ((lread = read(infifofd,bufptr,inlen)) < 0)
		throw n_u::IOException(inputFifoName,"read",errno);
	    bufptr += lread;
	    inlen -= lread;
	}
	msgbuf.get()[inlen-1] = '\0';	// make sure it is 0 terminated
	unsigned char in_etx;
	if (read(infifofd,&in_etx,1) < 0)
	throw n_u::IOException(inputFifoName,"read",errno);

	if (in_etx != etx)
	throw n_u::IOException(inputFifoName,"read","failed to read ETX");

	// now finally throw the exception
        throw n_u::IOException(getDSMSensorName(dev),
		string("ioctl: ") + msgbuf.get(),errval);
    }
    if (_IOC_DIR(cmd) & _IOC_READ) {
	if (inlen < len) len = inlen;
	if ((lread = read(infifofd,buf,len)) < 0)
	    throw n_u::IOException(inputFifoName,"read",errno);
	if (lread != (ssize_t)len)
	    throw n_u::IOException(inputFifoName,
	    "ioctl read buf","wrong len read");

	if (inlen > len) {
	    unsigned char junk[inlen - len];
	    if (read(infifofd,junk,inlen-len) < 0)
	      throw n_u::IOException(inputFifoName,"read",errno);
	}
    }
    else {
	if (inlen != 0)
	    throw n_u::IOException(inputFifoName,"read","inlen != 0");
    }

    unsigned char in_etx;
    if (read(infifofd,&in_etx,1) < 0)
    throw n_u::IOException(inputFifoName,"read",errno);

    if (in_etx != etx)
    throw n_u::IOException(inputFifoName,"read","failed to read ETX");
}

void RTL_DevIoctl::ioctl(int cmd, int dev, const void *buf, size_t len)
    throw(n_u::IOException)
{
    if (_IOC_DIR(cmd) & _IOC_READ)
	throw n_u::IOException(getDSMSensorName(dev),
	"ioctl","cannot do ioctl get into const buf");

     ioctl(cmd,dev,(void*)buf,len);
}
   

/* static */
string RTL_DevIoctl::makeInputFifoName(const string& prefix, int boardNum)
{
    char num[16];
    sprintf(num,"%d",boardNum);
    return prefix + "_ictl_" +  num;
}

/* static */
string RTL_DevIoctl::makeOutputFifoName(const string& prefix, int boardNum)
{
    char num[16];
    sprintf(num,"%d",boardNum);
    return prefix + "_octl_" +  num;
}

string RTL_DevIoctl::getDSMSensorName(int devNum) const
{
    char num[16];
    sprintf(num,"%d",devNum);
    return prefix +  num;
}
