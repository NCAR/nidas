/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/RTL_IODevice.h>
#include <nidas/core/RTL_DevIoctlStore.h>
#include <nidas/core/DSMTime.h>
#include <nidas/util/IOTimeoutException.h>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib> // atoi()
#include <bits/pthreadtypes.h>

#include <iostream>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

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


void RTL_IODevice::open(int flags) throw(n_u::IOException,n_u::InvalidParameterException)
{
    
    int accmode = flags & O_ACCMODE;

    if (accmode == O_RDONLY || accmode == O_RDWR) {
	infifofd = ::open(inFifoName.c_str(),O_RDONLY);
	if (infifofd < 0) throw n_u::IOException(inFifoName,"open",errno);
    }
    // cerr << "opened: " <<  inFifoName << endl;

    if (accmode == O_WRONLY || accmode == O_RDWR) {
	outfifofd = ::open(outFifoName.c_str(),O_WRONLY);
	if (outfifofd < 0) throw n_u::IOException(outFifoName,"open",errno);
    }
    // cerr << "opened: " <<  outFifoName << endl;

}

void RTL_IODevice::close() throw(n_u::IOException)
{
    if (infifofd >= 0) ::close(infifofd);
    infifofd = -1;

    if (outfifofd >= 0) ::close(outfifofd);
    outfifofd = -1;

    if (devIoctl) devIoctl->close();
    devIoctl = 0;
}


void RTL_IODevice::ioctl(int request, void* buf, size_t len) 
	throw(n_u::IOException)
{
    if (!devIoctl) {
        devIoctl =
		RTL_DevIoctlStore::getInstance()->getDevIoctl(prefix,devNum);
	devIoctl->open();
    }
    devIoctl->ioctl(request,devNum,buf,len);
}

size_t RTL_IODevice::read(void *buf, size_t len) throw(n_u::IOException)
{
    ssize_t l = ::read(infifofd,buf,len);
    if (l < 0) throw n_u::IOException(inFifoName,"read",errno);
    return l;
}

size_t RTL_IODevice::read(void *buf, size_t len, int msecTimeout) throw(n_u::IOException)
    {
	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(infifofd, &fdset);
	struct timeval tmpto = { 0, msecTimeout * USECS_PER_MSEC };
        int res;
	if ((res = ::select(infifofd+1,&fdset,0,0,&tmpto)) < 0) {
	    throw nidas::util::IOException(inFifoName,"read",errno);
	}
	if (res == 0)
	    throw nidas::util::IOTimeoutException(inFifoName,"read");
        return read(buf,len);
    }

size_t RTL_IODevice::write(const void *buf, size_t len) throw(n_u::IOException)
{
    ssize_t l = ::write(outfifofd,buf,len);
    if (l < 0) throw n_u::IOException(outFifoName,"write",errno);
    return l;
}

