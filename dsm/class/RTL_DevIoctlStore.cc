/*
   Copyright by the National Center for Atmospheric Research
*/

#include <sys/stat.h>	/* for stat() */

#include <RTL_DevIoctlStore.h>

#include <iostream>

using namespace dsm;
using namespace std;

/* static */
RTL_DevIoctlStore* RTL_DevIoctlStore::instance = 0;
                                                                                
/* static */
atdUtil::Mutex RTL_DevIoctlStore::instanceMutex;
                                                                                
/* static */
RTL_DevIoctlStore* RTL_DevIoctlStore::getInstance() {
                                                                                
    if (!instance) {
	atdUtil::Synchronized autosync(instanceMutex);
	if (!instance) instance = new RTL_DevIoctlStore();
    }
                                                                                
    return instance;
}
                                                                                
/* static */
void RTL_DevIoctlStore::removeInstance() {
    instanceMutex.lock();
    delete instance;
    instance = 0;
    instanceMutex.unlock();
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

RTL_DevIoctl*  RTL_DevIoctlStore::getDevIoctl(const string& prefix,
	int portNum) throw(atdUtil::IOException)
{
    /*
     * First loop over vector of found RTL_DevIoctls.
     * For each one found, get the number of ports it supports.
     * If the requested portNum is within the range of
     * ports for that board, then you have the correct RTL_DevIoctl.
     */
    RTL_DevIoctl* fifo = 0;
    int boardNum = -1;
    int firstPort = 0;
    int nports = 0;

    fifosMutex.lock();
    for (vector<RTL_DevIoctl*>::const_iterator fi = fifos.begin();
	fi != fifos.end(); ++fi) {
	if ((*fi)->getPrefix() == prefix) {
	    boardNum = (*fi)->getBoardNum();
	    firstPort = (*fi)->getFirstPortNum();
	    nports = (*fi)->getNumPorts();
	    if (firstPort + nports > portNum) {
	      fifo = *fi;
	      break;
	    }
	}
    }
    fifosMutex.unlock();
    if (fifo) return fifo;

    /*
     * Have to look at subsequent boards
     */
    boardNum++;
    firstPort += nports;

    for ( ; ; boardNum++ ) {
        fifo = RTL_DevIoctlStore::getDevIoctl(prefix,boardNum,firstPort);

	fifosMutex.lock();
	fifos.push_back(fifo);
	fifosMutex.unlock();

	nports = fifo->getNumPorts();
	if (firstPort + nports > portNum) break;
	firstPort += nports;
    }
    return fifo;
}

RTL_DevIoctl*  RTL_DevIoctlStore::getDevIoctl(const string& prefix,
	int boardNum, int firstPort) throw(atdUtil::IOException)
{
    string infifo = RTL_DevIoctl::makeInputFifoName(prefix,boardNum);
    struct stat statbuf;

    if (::stat(infifo.c_str(),&statbuf) == 0) {
	RTL_DevIoctl* fifo = new RTL_DevIoctl(prefix,boardNum,firstPort);
	return fifo;
    }
    throw atdUtil::IOException(infifo,"open","file not found");
}
