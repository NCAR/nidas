/*
   Copyright by the National Center for Atmospheric Research
*/

#include <sys/stat.h>	/* for stat() */

#include <RTL_DevIoctlStore.h>

#include <iostream>

using namespace std;

/* static */
RTL_DevIoctlStore* RTL_DevIoctlStore::instance = 0;
                                                                                
/* static */
atdUtil::Mutex* RTL_DevIoctlStore::instanceMutex = new atdUtil::Mutex;
                                                                                
/* static */
RTL_DevIoctlStore* RTL_DevIoctlStore::getInstance() {
                                                                                
    instanceMutex->lock();
    if (!instance) instance = new RTL_DevIoctlStore();
    instanceMutex->unlock();
                                                                                
    return instance;
}
                                                                                
/* static */
void RTL_DevIoctlStore::removeInstance() {
    instanceMutex->lock();
    delete instance;
    instance = 0;
    instanceMutex->unlock();
}
                                                                                
RTL_DevIoctlStore::RTL_DevIoctlStore()
{
}
                                                                                
/**
 * Delete the accumulated RTL_DevIoctl s
 */
RTL_DevIoctlStore::~RTL_DevIoctlStore()
{
  fifosMutex.lock();
  for (vector<RTL_DevIoctl*>::iterator fi = fifos.begin();  fi != fifos.end();
        ++fi) delete *fi;
  fifosMutex.unlock();
}

RTL_DevIoctl*  RTL_DevIoctlStore::getDevIoctl(const string& prefix, int portNum)
{
    // first loop over vector of found Ioctls
    RTL_DevIoctl* fifo = 0;

    fifosMutex.lock();
    for (vector<RTL_DevIoctl*>::const_iterator fi = fifos.begin();
	fi != fifos.end(); ++fi) {
	if ((*fi)->getPrefix() == prefix) {
	    int firstPort = (*fi)->getFirstPortNum();
	    int nports = (*fi)->getNumPorts();
	    if (firstPort + nports > portNum) {
	      fifo = *fi;
	      break;
	    }
	}
    }
    fifosMutex.unlock();
    if (fifo) return fifo;

    int firstPort = 0;

    for (int boardNum = 0; ; boardNum++ ) {
        fifo = RTL_DevIoctlStore::getDevIoctl(prefix,boardNum,firstPort);
	if (!fifo) break;
	int nports = fifo->getNumPorts();
	if (firstPort + nports > portNum) break;
	firstPort += nports;
    }
    return fifo;
}

RTL_DevIoctl*  RTL_DevIoctlStore::getDevIoctl(const string& prefix, int boardNum,
	int firstPort)
{
    string infifo = RTL_DevIoctl::makeInputFifoName(prefix,boardNum);
    struct stat statbuf;

    if (::stat(infifo.c_str(),&statbuf) == 0) {
	cerr << infifo << " exists" << endl;
	RTL_DevIoctl* fifo = new RTL_DevIoctl(prefix,boardNum,firstPort);

	fifosMutex.lock();
	fifos.push_back(fifo);
	fifosMutex.unlock();

	return fifo;
    }
    return 0;
}
