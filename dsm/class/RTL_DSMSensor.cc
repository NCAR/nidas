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

/**
 * Destructor.  This also does a close, which is a bit of an
 * issue, since close can throw IOException, and we want to
 * avoid throwing exceptions in destructors.  For now
 * we'll catch it a write it to cerr, which should be
 * replaced by a actual logging object.
 */
RTL_DSMSensor::~RTL_DSMSensor()
{
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
    char* cbuf = (char*) buf;
    ssize_t n; 
    for (n = 0; n < (ssize_t)len; ) {
	size_t l = ::read(infifofd,cbuf,len - n);
	if (l < 0) throw atdUtil::IOException(inFifoName,"read",errno);
	cbuf += l;
	n += l;
    }
    return n;
}

ssize_t RTL_DSMSensor::write(const void *buf, size_t len) throw(atdUtil::IOException)
{
    size_t n = ::write(outfifofd,buf,len);
    if (n < 0) throw atdUtil::IOException(outFifoName,"write",errno);
    return n;
}
