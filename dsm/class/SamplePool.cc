/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-12-02 12:24:18 -0700 (Fri, 02 Dec 2005) $

    $LastChangedRevision: 3171 $

    $LastChangedBy: maclean $

    $HeadURL: http://localhost:8080/svn/nids/trunk/dsm/class/PortSelector.cc $
 ********************************************************************

*/

#include <SamplePool.h>

using namespace dsm;
using namespace std;

/* static */
SamplePools* SamplePools::instance = 0;

/* static */
atdUtil::Mutex SamplePools::instanceLock = atdUtil::Mutex();

/* static */
SamplePools *SamplePools::getInstance()
{
    if (!instance) {
        atdUtil::Synchronized pooler(instanceLock);
        if (!instance) instance = new SamplePools();
    }
    return instance;
}

list<SamplePoolInterface*> SamplePools::getPools() const
{
    atdUtil::Synchronized pooler(instanceLock);
    return pools;
}

void SamplePools::addPool(SamplePoolInterface* pool)
{
    atdUtil::Synchronized pooler(instanceLock);
    pools.push_back(pool);
}

void SamplePools::removePool(SamplePoolInterface* pool)
{
    atdUtil::Synchronized pooler(instanceLock);
    list<SamplePoolInterface*>::iterator li =
	find(pools.begin(),pools.end(),pool);
    if (li != pools.end()) pools.erase(li);
}

