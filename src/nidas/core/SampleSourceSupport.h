
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2009-05-13 11:20:32 -0600 (Wed, 13 May 2009) $

    $LastChangedRevision: 4597 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn.eol.ucar.edu/svn/nidas/trunk/src/nidas/core/SampleSource.h $


*/

#ifndef NIDAS_CORE_SAMPLESOURCESUPPORT_H
#define NIDAS_CORE_SAMPLESOURCESUPPORT_H

#include <nidas/core/SampleSource.h>
#include <nidas/core/SampleClientList.h>

#include <set>
#include <map>

namespace nidas { namespace core {

class SampleTag;

/**
 * A source of samples. A SampleSource maintains a list
 * of SampleClients.  When a SampleSource has a Sample ready,
 * it will call the receive method of all its SampleClients.
 * SampleClients register/unregister with a SampleSource via
 * the addSampleClient/removeSampleClient methods.
 */
class SampleSourceSupport: public SampleSource {
public:

    SampleSourceSupport(bool raw);

    /**
     * Copy OK. A shallow copy, the SampleTags are copied to the
     * new instance, but the SampleClients are not.
     */
    SampleSourceSupport(const SampleSourceSupport& x);

    virtual ~SampleSourceSupport() {}

    SampleSource* getRawSampleSource()
    {
        if (!_raw) return 0;
        return this;
    }

    SampleSource* getProcessedSampleSource()
    {
        if (_raw) return 0;
        return this;
    }

    void addSampleTag(const SampleTag* tag) throw();

    void removeSampleTag(const SampleTag* tag) throw();

    std::list<const SampleTag*> getSampleTags() const;

    SampleTagIterator getSampleTagIterator() const;

    /**
     * Add a SampleClient to this SampleSource.  The pointer
     * to the SampleClient must remain valid, until after
     * it is removed.
     */
    void addSampleClient(SampleClient* c) throw();

    /**
     * Remove a SampleClient from this SampleSource
     * This will also remove a SampleClient if it has been
     * added with addSampleClientForTag().
     */
    void removeSampleClient(SampleClient* c) throw();

    /**
     * Add a SampleClient to this SampleSource.  The pointer
     * to the SampleClient must remain valid, until after
     * it is removed.
     */
    void addSampleClientForTag(SampleClient* c,const SampleTag*) throw();

    /**
     * Add a SampleClient to this SampleSource.  The pointer
     * to the SampleClient must remain valid, until after
     * it is removed.
     */
    void removeSampleClientForTag(SampleClient* c,const SampleTag*) throw();

    /**
     * How many SampleClients are currently in my list.
     */
    int getClientCount() const throw();

    /**
     * Big cleanup.
     */
    void removeAllSampleClients() throw()
    {
        _clients.removeAll();
        _clientMapLock.lock();
        _clientSet.clear();
        _clientMapLock.unlock();
    }

    /**
     * Distribute a sample to my clients, calling the receive() method
     * of each client, passing the const pointer to the Sample.
     * Does a s->freeReference() on the sample when done,
     * which means you should assume that the pointer to the Sample
     * is invalid after the call to distribute(),
     * unless the owner has done an additional holdReference().
     * If so, it is very bad form to make any changes
     * to Sample after the distribute() - the clients may get
     * a half-changed Sample.
     */
    void distribute(const Sample* s) throw();

    /**
     * Distribute a list of samples to my clients. Calls receive() method
     * of each client, passing the pointer to the Sample.
     * Does a s->freeReference() on each sample in the list.
     */
    void distribute(const std::list<const Sample*>& samps) throw();

    void flush() throw();

    const SampleStats& getSampleStats() const
    {
        return _stats;
    }

    void setKeepStats(bool val)
    {
        _keepStats = val;
    }

    bool getKeepStats() const
    {
        return _keepStats;
    }

private:

    mutable nidas::util::Mutex _tagsMutex;

    std::list<const SampleTag*> _sampleTags;

    /**
     * Current clients of all samples.
     */
    SampleClientList _clients;

    /**
     * Current clients of specific samples.
     */
    std::map<dsm_sample_id_t,SampleClientList> _clientsBySampleId;

    std::set<SampleClient*> _clientSet;

    nidas::util::Mutex _clientMapLock;

    SampleStats _stats;

    bool _raw;

    bool _keepStats;

    /**
     * No assignment.
     */
    SampleSourceSupport& operator = (const SampleSourceSupport& x);

};

}}	// namespace nidas namespace core


#endif
