/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-01-19 16:57:44 -0700 (Thu, 19 Jan 2006) $

    $LastChangedRevision: 3235 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/RTL_IODevice.cc $
 ********************************************************************

*/

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <bits/pthreadtypes.h>

#include <iostream>

#include <RTL_IODevice.h>
#include <RTL_DevIoctlStore.h>

using namespace std;
using namespace dsm;


RTL_IODevice::RTL_IODevice() :
    devIoctl(0),infifofd(-1),outfifofd(-1)
{
}

RTL_IODevice::RTL_IODevice(const std::string& name):
    devIoctl(0),infifofd(-1),outfifofd(-1)
{
    setName(name);
}

/**
 * Destructor. 
 */
RTL_IODevice::~RTL_IODevice()
{
}

void RTL_IODevice::setName(const std::string& val)
{
    IODevice::setName(val);

    string::const_reverse_iterator ri;
    for (ri = getName().rbegin(); ri != getName().rend(); ++ri)
        if (!isdigit(*ri)) break;
    string::const_iterator pi = ri.base();

    // cerr << "*pi=" << *pi << " diff=" << pi-getName().begin() << endl;

    // string prefix = getName().substr(0,pi-getName().begin());
    prefix = string(getName().begin(),pi);

    string devstr(pi,getName().end());

    devNum = atoi(devstr.c_str());

    inFifoName = prefix + "_in_" + devstr;
    outFifoName = prefix + "_out_" + devstr;
}


void RTL_IODevice::open(int flags) throw(atdUtil::IOException,atdUtil::InvalidParameterException)
{
    
    int accmode = flags & O_ACCMODE;

    if (accmode == O_RDONLY || accmode == O_RDWR) {
	infifofd = ::open(inFifoName.c_str(),O_RDONLY);
	if (infifofd < 0) throw atdUtil::IOException(inFifoName,"open",errno);
    }
    // cerr << "opened: " <<  inFifoName << endl;

    if (accmode == O_WRONLY || accmode == O_RDWR) {
	outfifofd = ::open(outFifoName.c_str(),O_WRONLY);
	if (outfifofd < 0) throw atdUtil::IOException(outFifoName,"open",errno);
    }
    // cerr << "opened: " <<  outFifoName << endl;

}

void RTL_IODevice::close() throw(atdUtil::IOException)
{
    if (infifofd >= 0) ::close(infifofd);
    infifofd = -1;

    if (outfifofd >= 0) ::close(outfifofd);
    outfifofd = -1;

    if (devIoctl) devIoctl->close();
    devIoctl = 0;
}


void RTL_IODevice::ioctl(int request, void* buf, size_t len) 
	throw(atdUtil::IOException)
{
    if (!devIoctl) {
        devIoctl =
		RTL_DevIoctlStore::getInstance()->getDevIoctl(prefix,devNum);
	devIoctl->open();
    }
    devIoctl->ioctl(request,devNum,buf,len);
}

size_t RTL_IODevice::read(void *buf, size_t len) throw(atdUtil::IOException)
{
    ssize_t l = ::read(infifofd,buf,len);
    if (l < 0) throw atdUtil::IOException(inFifoName,"read",errno);
    return l;
}

size_t RTL_IODevice::write(const void *buf, size_t len) throw(atdUtil::IOException)
{
    ssize_t l = ::write(outfifofd,buf,len);
    if (l < 0) throw atdUtil::IOException(outFifoName,"write",errno);
    return l;
}

