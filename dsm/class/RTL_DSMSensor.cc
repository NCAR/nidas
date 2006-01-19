/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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

#include <atdUtil/Logger.h>

using namespace std;
using namespace dsm;

CREATOR_FUNCTION(RTL_DSMSensor)

RTL_DSMSensor::RTL_DSMSensor() :
    DSMSensor(),devIoctl(0),infifofd(-1),outfifofd(-1)
{
}


/**
 * Destructor. 
 */
RTL_DSMSensor::~RTL_DSMSensor()
{
}

void RTL_DSMSensor::setDeviceName(const std::string& val)
{
    DSMSensor::setDeviceName(val);

    string::reverse_iterator ri;
    for (ri = devname.rbegin(); ri != devname.rend(); ++ri)
        if (!isdigit(*ri)) break;
    string::iterator pi = ri.base();

    // cerr << "*pi=" << *pi << " diff=" << pi-devname.begin() << endl;

    // string prefix = devname.substr(0,pi-devname.begin());
    prefix = string(devname.begin(),pi);

    string devstr(pi,devname.end());

    devNum = atoi(devstr.c_str());

    inFifoName = prefix + "_in_" + devstr;
    outFifoName = prefix + "_out_" + devstr;
}


void RTL_DSMSensor::open(int flags) throw(atdUtil::IOException,atdUtil::InvalidParameterException)
{
    
    atdUtil::Logger::getInstance()->log(LOG_NOTICE,
    	"opening: %s",getName().c_str());

    int accmode = flags & O_ACCMODE;

    if (accmode == O_RDONLY || accmode == O_RDWR) {
	infifofd = ::open(inFifoName.c_str(),O_RDONLY);
	if (infifofd < 0) throw atdUtil::IOException(inFifoName,"open",errno);
    }

    if (accmode == O_WRONLY || accmode == O_RDWR) {
	outfifofd = ::open(outFifoName.c_str(),O_WRONLY);
	if (outfifofd < 0) throw atdUtil::IOException(outFifoName,"open",errno);
    }
    initBuffer();

}

void RTL_DSMSensor::close() throw(atdUtil::IOException)
{
    atdUtil::Logger::getInstance()->log(LOG_INFO,
    	"closing: %s",getName().c_str());

    if (infifofd >= 0) ::close(infifofd);
    infifofd = -1;

    if (outfifofd >= 0) ::close(outfifofd);
    outfifofd = -1;

    if (devIoctl) devIoctl->close();
    devIoctl = 0;
    destroyBuffer();
}


void RTL_DSMSensor::ioctl(int request, void* buf, size_t len) 
	throw(atdUtil::IOException)
{
    if (!devIoctl) {
        devIoctl =
		RTL_DevIoctlStore::getInstance()->getDevIoctl(prefix,devNum);
	devIoctl->open();
    }
    devIoctl->ioctl(request,devNum,buf,len);
}

void RTL_DSMSensor::ioctl(int request, const void* buf, size_t len) 
	throw(atdUtil::IOException)
{
    if (!devIoctl) {
        devIoctl =
		RTL_DevIoctlStore::getInstance()->getDevIoctl(prefix,devNum);
	devIoctl->open();
    }
    devIoctl->ioctl(request,devNum,buf,len);
}

size_t RTL_DSMSensor::read(void *buf, size_t len) throw(atdUtil::IOException)
{
    ssize_t l = ::read(infifofd,buf,len);
    if (l < 0) throw atdUtil::IOException(inFifoName,"read",errno);
    return l;
}

size_t RTL_DSMSensor::write(const void *buf, size_t len) throw(atdUtil::IOException)
{
    ssize_t l = ::write(outfifofd,buf,len);
    if (l < 0) throw atdUtil::IOException(outFifoName,"write",errno);
    return l;
}

