/*
   Copyright by the National Center for Atmospheric Research
*/

#include <RTL_DevIoctl.h>

#include <ioctl_fifo.h>

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
    infifofd(-1),outfifofd(-1),opened(false)
{
}

RTL_DevIoctl::~RTL_DevIoctl()
{
    close();
}

void RTL_DevIoctl::open() throw(atdUtil::IOException)
{
    
    infifofd = ::open(inputFifoName.c_str(),O_RDONLY);
    if (infifofd < 0) throw atdUtil::IOException(inputFifoName,"open",errno);

    outfifofd = ::open(outputFifoName.c_str(),O_WRONLY);
    if (outfifofd < 0) throw atdUtil::IOException(outputFifoName,"open",errno);

    opened = true;

}

void RTL_DevIoctl::close()
{
    if (infifofd >= 0) ::close(infifofd);
    infifofd = -1;
    if (outfifofd >= 0) ::close(outfifofd);
    outfifofd = -1;
}


int RTL_DevIoctl::getNumPorts() throw(atdUtil::IOException)
{

    if (!opened) open();

    if (numPorts > 0) return numPorts;

    ioctl(GET_NUM_PORTS,0,&numPorts,sizeof(numPorts));
    std::cerr << "numPorts=" << numPorts << std::endl;

    return numPorts;
}


void RTL_DevIoctl::ioctl(int cmd, int port, void *buf, size_t len)
    throw(atdUtil::IOException)
{
    static const unsigned char etx = '\003';

#ifdef USE_WRITEV
    // As a hack, try writev, instead of individual writes.
    // The other end of the fifo still reads the fields
    // one at a time though, so it doesn't gain us anything.

    struct iovec ivec[5];
    int iv = 0;
    ivec[iv].iov_base = &cmd;
    ivec[iv++].iov_len = sizeof(cmd);

    ivec[iv].iov_base = &port;
    ivec[iv++].iov_len = sizeof(port);

    ivec[iv].iov_base = &len;
    ivec[iv++].iov_len = sizeof(len);

    if (_IOC_DIR(cmd) & _IOC_WRITE) {
	ivec[iv].iov_base = buf;
	ivec[iv++].iov_len = len;
    }

    ivec[iv].iov_base = (void*)&etx;
    ivec[iv++].iov_len = sizeof(etx);

    if (writev(outfifofd, ivec,iv) < 0)
	throw atdUtil::IOException(outputFifoName,"write",errno);
#else
    unsigned char* wbuf;
    unsigned char* wptr;
    size_t wlen = sizeof(cmd) + sizeof(port) + sizeof(len) + sizeof(etx);

    if (_IOC_DIR(cmd) & _IOC_WRITE) wlen += len;

    wptr = wbuf = new unsigned char[wlen];

    memcpy(wptr,&cmd,sizeof(cmd));
    wptr += sizeof(cmd);

    memcpy(wptr,&port,sizeof(port));
    wptr += sizeof(port);

    memcpy(wptr,&len,sizeof(len));
    wptr += sizeof(len);

    if (_IOC_DIR(cmd) & _IOC_WRITE) {
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
#endif

    /* read back the status errno */
    int errval;
    if (read(infifofd,&errval,sizeof(errval)) < 0)
    throw atdUtil::IOException(inputFifoName,"read",errno);

    if (errval < 0) throw atdUtil::IOException(getDSMSensorName(port),
	"ioctl",errval);

    size_t inlen;
    if (read(infifofd,&inlen,sizeof(inlen)) < 0)
    throw atdUtil::IOException(inputFifoName,"read",errno);

    if (_IOC_DIR(cmd) & _IOC_READ) {
	if (inlen < len) len = inlen;
	int nread;
	if ((nread = read(infifofd,buf,len)) < 0)
	    throw atdUtil::IOException(inputFifoName,"read",errno);

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
