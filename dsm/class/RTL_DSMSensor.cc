/*
   Copyright by the National Center for Atmospheric Research
*/

#include <RTL_DSMSensor.h>

using namespace std;

RTL_DSMSensor::RTL_DSMSensor(const string& nameArg) :
    DSMSensor(nameArg),ioctlFifo(0),infifofd(-1),outfifofd(-1),
    ioctlFifo(0)
{

    string::const_iterator pi = getPrefixEnd(name);

    string prefix = name.substr(0,pi-name.begin());
    string portstr = name.substr(pi-name.begin());

    portNum = atoi(portstr.c_name());

    inFifoName = prefix + "_in_" + portstr;
    outFifoName = prefix + "_out_" + portstr;
}

RTL_DSMSensor::~RTL_DSMSensor()
{
  close();
}

void RTL_DSMSensor::open(int flags) throw(atdUtil::IOException)
{
  
    if (flags & O_RDONLY) {
	infifofd = ::open(inFifoName.c_str(),O_RDONLY);
	if (infifofd < 0) throw atdUtil::IOException(inFifoName,"open",errno);
    }

    if (flags & O_WRONLY) {
	outfifofd = ::open(outFifoName.c_str(),O_WRONLY);
	if (outfifofd < 0) throw atdUtil::IOException(outFifoName,"open",errno);
    }

    ioctlFifo = RTLIoctlFifos::getInstance()->getIoctlFifo(prefix,portNum);

    if (ioctlFifo) ioctlFifo->open();

}


void RTL_DSMSensor::close()
{
    if (infifofd >= 0)
        ::close(infifofd);
    infifofd = -1;

    if (outfifofd >= 0)
        ::close(outfifofd);
    outfifofd = -1;

    if (ioctlFifo) ioctlFifo->close();
    ioctlFifo = 0;

}

int RTL_DSMSensor::ioctlSend(int request,size_t len, void* buf) 
	throw(atdUtil::IOException)
{
    if (!ioctlFifo) throw atdUtil::IOException(getName(),"ioctlSend",
    	"no ioctlFifo associated with device");

   return ioctlFifo->ioctlSend(request,portNum,len,buf);
}


int RTL_DSMSensor::ioctlRecv(int request,size_t len, void* buf) 
	throw(atdUtil::IOException)
{
    if (!ioctlFifo) throw atdUtil::IOException(getName(),"ioctlSend",
    	"no ioctlFifo associated with device");

   return ioctlFifo->ioctlRecv(request,portNum,len,buf);
}


