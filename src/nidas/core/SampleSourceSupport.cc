/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-05-13 11:20:32 -0600 (Wed, 13 May 2009) $

    $LastChangedRevision: 4597 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/SampleSource.cc $
 ********************************************************************

*/

#include <nidas/core/SampleSourceSupport.h>
#include <nidas/core/SampleTag.h>
#include <nidas/util/ThreadSupport.h>

#include <algorithm>

using namespace nidas::core;
using namespace std;
namespace n_u = nidas::util;

SampleSourceSupport::SampleSourceSupport(bool raw): _raw(raw),_keepStats(false)
{
}

SampleSourceSupport::SampleSourceSupport(const SampleSourceSupport& x):
    _sampleTags(x._sampleTags),_raw(x._raw),_keepStats(x._keepStats)
{
}

list<const SampleTag*> SampleSourceSupport::getSampleTags() const
{
    n_u::Autolock autolock(_tagsMutex);
    return list<const SampleTag*>(_sampleTags);
}

void SampleSourceSupport::addSampleTag(const SampleTag* tag) throw()
{
    n_u::Autolock autolock(_tagsMutex);
    if (find(_sampleTags.begin(),_sampleTags.end(),tag) == _sampleTags.end())
        _sampleTags.push_back(tag);
}

void SampleSourceSupport::removeSampleTag(const SampleTag* tag) throw()
{
    n_u::Autolock autolock(_tagsMutex);
    list<const SampleTag*>::iterator si =
        find(_sampleTags.begin(),_sampleTags.end(),tag);
    if (si != _sampleTags.end()) _sampleTags.erase(si);
}

SampleTagIterator SampleSourceSupport::getSampleTagIterator() const
{
    return SampleTagIterator(this);
}

void SampleSourceSupport::addSampleClient(SampleClient* c) throw()
{
    _clients.add(c);
    _clientMapLock.lock();
    _clientSet.insert(c);
    _clientMapLock.unlock();
}

void SampleSourceSupport::removeSampleClient(SampleClient* c) throw()
{
    _clientMapLock.lock();
    map<dsm_sample_id_t,SampleClientList>::iterator ci =
        _clientsBySampleId.begin();
    for ( ; ci != _clientsBySampleId.end(); ++ci)
        ci->second.remove(c);
    _clientSet.erase(c);
    _clientMapLock.unlock();

    _clients.remove(c);
}

void SampleSourceSupport::addSampleClientForTag(SampleClient* client,
    const SampleTag* tag) throw()
{
    _clientMapLock.lock();

    map<dsm_sample_id_t,SampleClientList>::iterator ci =
        _clientsBySampleId.find(tag->getId());

    if (ci == _clientsBySampleId.end()) {
        SampleClientList clients;
        clients.add(client);
        _clientsBySampleId[tag->getId()] = clients;
    }
    else ci->second.add(client);

    _clientSet.insert(client);

    _clientMapLock.unlock();
}

void SampleSourceSupport::removeSampleClientForTag(SampleClient* client,
    const SampleTag* tag) throw()
{
    _clientMapLock.lock();

    map<dsm_sample_id_t,SampleClientList>::iterator ci =
        _clientsBySampleId.find(tag->getId());
    if (ci != _clientsBySampleId.end())
        ci->second.remove(client);
    _clientSet.erase(client);

    _clientMapLock.unlock();

}

int SampleSourceSupport::getClientCount() const throw()
{
    return _clientSet.size();
}

void SampleSourceSupport::distribute(const Sample* sample) throw()
{
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

   _clientMapLock.lock();
    map<dsm_sample_id_t,SampleClientList>::const_iterator ci =
        _clientsBySampleId.find(sample->getId());
    if (ci != _clientsBySampleId.end()) {
        SampleClientList tmp(ci->second);
        _clientMapLock.unlock();
        list<SampleClient*>::const_iterator li = tmp.begin();
        for ( ; li != tmp.end(); ++li) (*li)->receive(sample);
    }
    else _clientMapLock.unlock();

    // copy constructor does a lock
    SampleClientList tmp(_clients);
    list<SampleClient*>::const_iterator li = tmp.begin();
    for ( ; li != tmp.end(); ++li)
	(*li)->receive(sample);

    if (getKeepStats()) {
        _stats.addNumSamples(1);
        _stats.addNumBytes(sample->getHeaderLength() + sample->getDataByteLength());
        _stats.setLastTimeTag(sample->getTimeTag());
    }
    sample->freeReference();
}

void SampleSourceSupport::distribute(const list<const Sample*>& samples)
	throw()
{
    list<const Sample*>::const_iterator si;
    for (si = samples.begin(); si != samples.end(); ++si) {
	const Sample *s = *si;
	distribute(s);
    }
}

void SampleSourceSupport::flush() throw()
{
    _clientMapLock.lock();
    map<dsm_sample_id_t,SampleClientList> tmpmap(_clientsBySampleId);
    _clientMapLock.unlock();

    map<dsm_sample_id_t,SampleClientList>::const_iterator ci =
        tmpmap.begin();
    for ( ; ci != tmpmap.end(); ++ci) {
        SampleClientList tmp(ci->second);
        list<SampleClient*>::const_iterator li = tmp.begin();
        for ( ; li != tmp.end(); ++li) (*li)->finish();
    }

    // copy constructor does a lock
    SampleClientList tmp(_clients);
    list<SampleClient*>::const_iterator li;
    for (li = tmp.begin(); li != tmp.end(); ++li)
	(*li)->finish();
}
