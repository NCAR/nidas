/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <nidas/core/SamplePool.h>

#include <algorithm>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;

/* static */
SamplePools* SamplePools::_instance = 0;

/* static */
n_u::Mutex SamplePools::_instanceLock = n_u::Mutex();

/* static */
SamplePools *SamplePools::getInstance()
{
    if (!_instance) {
        n_u::Synchronized pooler(_instanceLock);
        if (!_instance) _instance = new SamplePools();
    }
    return _instance;
}

list<SamplePoolInterface*> SamplePools::getPools() const
{
    n_u::Synchronized pooler(_instanceLock);
    return _pools;
}

void SamplePools::addPool(SamplePoolInterface* pool)
{
    n_u::Synchronized pooler(_instanceLock);
    _pools.push_back(pool);
}

void SamplePools::removePool(SamplePoolInterface* pool)
{
    n_u::Synchronized pooler(_instanceLock);
    list<SamplePoolInterface*>::iterator li =
	find(_pools.begin(),_pools.end(),pool);
    if (li != _pools.end()) _pools.erase(li);
}

