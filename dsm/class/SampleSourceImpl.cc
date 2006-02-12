/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/


#include <SampleSourceImpl.h>

using namespace dsm;
using namespace std;

void SampleSourceImpl::addSampleClientImpl(SampleClient* client) throw() {
    clients.add(client);
}

void SampleSourceImpl::removeSampleClientImpl(SampleClient* client) throw() {
    clients.remove(client);
}
  
void SampleSourceImpl::removeAllSampleClientsImpl() throw() {
    clients.removeAll();
}

int SampleSourceImpl::getClientCountImpl() const throw() {
    return clients.size();
}
  
void SampleSourceImpl::distributeImpl(const Sample* sample) throw()
{
    // copy constructor does a lock
    SampleClientList tmp(clients);

    /* There is a multithreading issue at this point.
     * If a SampleClient removes themselves from the list
     * AND immediately delete's themselves, when the SampleSource
     * is executing this method, right here between the above
     * copy and the clients receive(), then things will crash.
     *
     * (I should keep this comment short to avoid this situation :)
     *
     * We don't want to keep the lock however, because
     * perhaps the SampleClient may want to remove themselves
     * within the receive() (if they get an IOException for example).
     * That situation would cause a deadlock.
     * To avoid this issue, SampleClients should either make
     * sure the SampleSource isn't executing at the time of
     * removeSampleClient or that there is a "long" time between
     * removeSampleClient and their destruction.  Hmmm, needs 
     * more thought.
     */

    list<SampleClient*>::const_iterator li;
    for (li = tmp.begin(); li != tmp.end(); ++li)
	(*li)->receive(sample);
    numSamplesSent++;
}

void SampleSourceImpl::distributeImpl(const list<const Sample*>& samples)
	throw()
{
    list<const Sample*>::const_iterator si;
    for (si = samples.begin(); si != samples.end(); ++si) {
	const Sample *s = *si;
	distributeImpl(s);
	s->freeReference();
    }
}

void SampleSourceImpl::flushImpl() throw()
{
    // copy constructor does a lock
    SampleClientList tmp(clients);
    list<SampleClient*>::const_iterator li;
    for (li = tmp.begin(); li != tmp.end(); ++li)
	(*li)->finish();
}
