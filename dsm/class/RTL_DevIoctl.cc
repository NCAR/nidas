/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision: 671 $

    $LastChangedBy: maclean $

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#include <RTL_DevIoctl.h>

#include <ioctl_fifo.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/uio.h>

#include <iostream>

using namespace std;

RTL_DevIoctl::RTL_DevIoctl(const string& prefixArg, int boardNumArg,
	int firstPortArg):
    prefix(prefixArg),
    boardNum(boardNumArg),firstPortNum(firstPortArg),numPorts(-1),
    inputFifoName(makeInputFifoName(prefix,boardNum)),
    outputFifoName(makeOutputFifoName(prefix,boardNum)),
    infifofd(-1),outfifofd(-1),usageCount(0)
{
}

RTL_DevIoctl::~RTL_DevIoctl()
{
    close();
}

void RTL_DevIoctl::open() throw(atdUtil::IOException)
{
    
    if (!usageCount++) {
	if (infifofd < 0)
	    infifofd = ::open(inputFifoName.c_str(),O_RDONLY);
	if (infifofd < 0) throw atdUtil::IOException(inputFifoName,"open",errno);

	if (outfifofd < 0)
	    outfifofd = ::open(outputFifoName.c_str(),O_WRONLY);
	if (outfifofd < 0) throw atdUtil::IOException(outputFifoName,"open",errno);
    }
}

void RTL_DevIoctl::close()
{
    if (!(--usageCount)) {
	if (infifofd >= 0) ::close(infifofd);
	infifofd = -1;
	if (outfifofd >= 0) ::close(outfifofd);
	outfifofd = -1;
    }
}


int RTL_DevIoctl::getNumPorts() throw(atdUtil::IOException)
{

    if (!usageCount) open();

    if (numPorts > 0) return numPorts;

    ioctl(GET_NUM_PORTS,0,&numPorts,sizeof(numPorts));

#ifdef DEBUG
    std::cerr << dec << "numPorts=" << numPorts << std::endl;
#endif

    return numPorts;
}


void RTL_DevIoctl::ioctl(int cmd, int port, void *buf, size_t len)
    throw(atdUtil::IOException)
{
    // allow only one ioctl at a time
    atdUtil::Synchronized autosync(ioctlMutex);

    static const unsigned char etx = '\003';

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
	throw atdUtil::IOException(outputFifoName,"write",errno);
    }
    delete [] wbuf;

    /* read back the status errno */
    int errval;
    ssize_t lread;
    if ((lread = read(infifofd,&errval,sizeof(errval))) < 0)
	throw atdUtil::IOException(inputFifoName,"read",errno);
#ifdef DEBUG
    cerr << "errval lread=" << lread << endl;
#endif

    if (lread != sizeof(errval))
    	throw atdUtil::IOException(inputFifoName,
	"ioctl read errval","wrong len read");

    if (errval != 0) throw atdUtil::IOException(getDSMSensorName(port),
	"ioctl",errval);

    size_t inlen;
    if ((lread = read(infifofd,&inlen,sizeof(inlen))) < 0)
	throw atdUtil::IOException(inputFifoName,"read",errno);
    if (lread != sizeof(inlen))
    	throw atdUtil::IOException(inputFifoName,
	"ioctl read inlen","wrong len read");

    if (_IOC_DIR(cmd) & _IOC_READ) {
	if (inlen < len) len = inlen;
	if ((lread = read(infifofd,buf,len)) < 0)
	    throw atdUtil::IOException(inputFifoName,"read",errno);
	if (lread != len)
	    throw atdUtil::IOException(inputFifoName,
	    "ioctl read buf","wrong len read");

	if (inlen > len) {
	    unsigned char junk[inlen - len];
	    if (read(infifofd,junk,inlen-len) < 0)
	      throw atdUtil::IOException(inputFifoName,"read",errno);
	}
    }
    else {
	if (inlen != 0)
	    throw atdUtil::IOException(inputFifoName,"read","inlen != 0");
    }

    unsigned char in_etx;
    if (read(infifofd,&in_etx,1) < 0)
    throw atdUtil::IOException(inputFifoName,"read",errno);

    if (in_etx != etx)
    throw atdUtil::IOException(inputFifoName,"read","failed to read ETX");
}

void RTL_DevIoctl::ioctl(int cmd, int port, const void *buf, size_t len)
    throw(atdUtil::IOException)
{
    if (_IOC_DIR(cmd) & _IOC_READ)
	throw atdUtil::IOException(getDSMSensorName(port),
	"ioctl","cannot do ioctl get into const buf");

     ioctl(cmd,port,(void*)buf,len);
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

string RTL_DevIoctl::getDSMSensorName(int portNum) const
{
    char num[16];
    sprintf(num,"%d",portNum);
    return prefix +  num;
}
