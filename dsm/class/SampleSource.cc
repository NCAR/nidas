/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/


#include <SampleSource.h>

using namespace dsm;

void SampleSource::addSampleClient(SampleClient* client) {
  clistLock.lock();
  clients.push_back(client);
  clistLock.unlock();
}

void SampleSource::removeSampleClient(SampleClient* client) {
  clistLock.lock();
  std::list<SampleClient*>::iterator li;
  for (li = clients.begin(); li != clients.end(); ) {
    if (*li == client) li = clients.erase(li);
    else ++li;
  }
  clistLock.unlock();
}
  
void SampleSource::removeAllSampleClients() {
  clistLock.lock();
  clients.clear();
  clistLock.unlock();
}
  
void SampleSource::distribute(const Sample* sample)
	throw(SampleParseException,atdUtil::IOException) {

  clistLock.lock();
  std::list<SampleClient*> tmp = clients;
  clistLock.unlock();

  std::list<SampleClient*>::iterator li;
  for (li = tmp.begin(); li != tmp.end(); ++li)
    (*li)->receive(sample);
  sample->freeReference();
  numSamplesSent++;
}

