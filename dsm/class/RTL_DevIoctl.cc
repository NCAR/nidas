/*
   Copyright by the National Center for Atmospheric Research
*/

#include <RTL_DevIoctl.h>

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
    if (infifofd >= 0) ::close(infifofd);
    if (outfifofd >= 0) ::close(outfifofd);
}

void RTL_DevIoctl::open() throw(atdUtil::IOException)
{
    
    infifofd = ::open(inputFifoName.c_str(),O_RDONLY);
    if (infifo < 0) throw atdUtil::IOException(inputFifoName,"open",errno);

    outfifofd = ::open(outputFifoName.c_str(),O_WRONLY);
    if (outfifo < 0) throw atdUtil::IOException(outputFifoName,"open",errno);

    opened = true;

}

int RTL_DevIoctl::getNumPorts() throw(atdUtil::IOException)

    if (!opened) open();

    if (numPorts > 0) return numPorts;

    ioctlRecv(NUM_PORTS,0,sizeof(numPorts),&numPorts);

    return numPorts;
}


int RTL_DevIoctl::ioctlRecv(int cmd, int port, size_t len, void *buf)
    throw(atdUtil::IOException)
{

    static const unsigned char etx = '\003';

    if (write(outfifofd,&cmd,sizeof(cmd)) < 0)
    throw atdUtil::IOException(outputFifoName,"write",errno);

    if (write(outfifofd,&port,sizeof(port)) < 0)
    throw atdUtil::IOException(outputFifoName,"write",errno);

    if (write(outfifofd,&etx,1) < 0)
    throw atdUtil::IOException(outputFifoName,"write",errno);

    /* read back the status errno */
    int errval;
    if (read(infifofd,&errval,sizeof(errval)) < 0)
    throw atdUtil::IOException(inputFifoName,"read",errno);

    if (errval < 0) throw atdUtil::IOException(getDSMSensorName(port),
	"ioctl",errval);

    size_t inlen;
    if (read(infifofd,&inlen,sizeof(inlen)) < 0)
    throw atdUtil::IOException(inputFifoName,"read",errno);

    if (inlen < len) len = inlen;

    int nread;
    if ((nread = read(fdin,buf,len)) < 0)
    throw atdUtil::IOException(inputFifoName,"read",errno);

    if (inlen > len) {
    unsigned char junk[inlen - len];
    if (read(fdin,junk,inlen-len) < 0)
      throw atdUtil::IOException(inputFifoName,"read",errno);
    }

    unsigned char in_etx;
    if (read(fdin,&in_etx,1) < 0)
    throw atdUtil::IOException(inputFifoName,"read",errno);

    if (in_etx != etx)
    throw atdUtil::IOException(inputFifoName,"read","failed to read ETX");
    return nread;
}

int RTL_DevIoctl::ioctlSend(int cmd, int port, size_t len, void *buf)
    throw(atdUtil::IOException)
{

    static const unsigned char etx = '\003';

    if (write(outfifofd,&cmd,sizeof(cmd)) < 0)
    throw atdUtil::IOException(outputFifoName,"write",errno);

    if (write(outfifofd,&port,sizeof(port)) < 0)
    throw atdUtil::IOException(outputFifoName,"write",errno);

    if (write(outfifofd,buf,len) < 0)
    throw atdUtil::IOException(outputFifoName,"write",errno);

    if (write(outfifofd,&etx,1) < 0)
    throw atdUtil::IOException(outputFifoName,"write",errno);

    /* read back the status errno */
    int errval;
    if (read(infifofd,&errval,sizeof(errval)) < 0)
    throw atdUtil::IOException(inputFifoName,"read",errno);

    if (errval < 0) throw atdUtil::IOException(getDSMSensorName(port),
	"ioctl",errval);

    // printf("nread=%d, errval=%d\n",nread,errval);

    size_t inlen;
    if (read(infifofd,&inlen,sizeof(inlen)) < 0)
    throw atdUtil::IOException(inputFifoName,"read",errno);

    if (inlen != 0)
    throw atdUtil::IOException(inputFifoName,"read","inlen != 0");

    unsigned char in_etx;
    if (read(fdin,&in_etx,1) < 0)
    throw atdUtil::IOException(inputFifoName,"read",errno);

    if (in_etx != etx)
    throw atdUtil::IOException(inputFifoName,"read","failed to read ETX");

    return nread;
}

/* static */
string RTL_DevIoctl::makeInputFifoName(const string& prefix, int boardNum)
{
    char num[16];
    sprintf(num,"%d",boardNum);
    return infifo = prefix + "_ictl_" +  num;
}

/* static */
string RTL_DevIoctl::makeOutputFifoName(const string& prefix, int boardNum)
{
    char num[16];
    sprintf(num,"%d",boardNum);
    return infifo = prefix + "_octl_" +  num;
}

string RTL_DevIoctl::getDSMSensorName(int portNum) const {
{
    char num[16];
    sprintf(num,"%d",portNum);
    return infifo = prefix +  num;
}
