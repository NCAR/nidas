/*
   Copyright by the National Center for Atmospheric Research
*/

#include <fcntl.h>
#include <errno.h>
#include <bits/pthreadtypes.h>

#include <iostream>

#include <RTL_DSMSensor.h>
#include <RTL_DevIoctlStore.h>


using namespace std;

RTL_DSMSensor::RTL_DSMSensor(const string& nameArg) :
    DSMSensor(nameArg),devIoctl(0),infifofd(-1),outfifofd(-1)
{

    string::reverse_iterator ri;
    for (ri = name.rbegin(); ri != name.rend(); ++ri)
        if (!isdigit(*ri)) break;
    string::iterator pi = ri.base();

    // cerr << "*pi=" << *pi << " diff=" << pi-name.begin() << endl;

    // string prefix = name.substr(0,pi-name.begin());
    prefix = string(name.begin(),pi);

    string portstr(pi,name.end());

    portNum = atoi(portstr.c_str());

    inFifoName = prefix + "_in_" + portstr;
    outFifoName = prefix + "_out_" + portstr;
}

RTL_DSMSensor::~RTL_DSMSensor()
{
  close();
}

void RTL_DSMSensor::open(int flags) throw(atdUtil::IOException)
{
  
    int accmode = flags & O_ACCMODE;

    if (accmode == O_RDONLY || accmode == O_RDWR) {
	infifofd = ::open(inFifoName.c_str(),O_RDONLY);
	if (infifofd < 0) throw atdUtil::IOException(inFifoName,"open",errno);
    }

    if (accmode == O_WRONLY || accmode == O_RDWR) {
	outfifofd = ::open(outFifoName.c_str(),O_WRONLY);
	if (outfifofd < 0) throw atdUtil::IOException(outFifoName,"open",errno);
    }

    devIoctl = RTL_DevIoctlStore::getInstance()->getDevIoctl(prefix,portNum);

    if (devIoctl) devIoctl->open();
}


void RTL_DSMSensor::close()
{
    if (infifofd >= 0)
        ::close(infifofd);
    infifofd = -1;

    if (outfifofd >= 0)
        ::close(outfifofd);
    outfifofd = -1;

    if (devIoctl) devIoctl->close();
    devIoctl = 0;

}

void RTL_DSMSensor::ioctl(int request, void* buf, size_t len) 
	throw(atdUtil::IOException)
{
    if (!devIoctl) throw atdUtil::IOException(getName(),"ioctl",
    	"no ioctl associated with device");

    devIoctl->ioctl(request,portNum,buf,len);
}

void RTL_DSMSensor::ioctl(int request, const void* buf, size_t len) 
	throw(atdUtil::IOException)
{
    if (!devIoctl) throw atdUtil::IOException(getName(),"ioctl",
    	"no ioctl associated with device");

    devIoctl->ioctl(request,portNum,buf,len);
}

ssize_t RTL_DSMSensor::read(void *buf, size_t len) throw(atdUtil::IOException)
{
    size_t n = ::read(infifofd,buf,len);
    if (n < 0) throw atdUtil::IOException(inFifoName,"read",errno);
    return n;
}

ssize_t RTL_DSMSensor::write(void *buf, size_t len) throw(atdUtil::IOException)
{
    size_t n = ::write(outfifofd,buf,len);
    if (n < 0) throw atdUtil::IOException(outFifoName,"write",errno);
    return n;
}
