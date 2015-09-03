/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
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

/* static */
void SamplePools::deleteInstance()
{
    if (_instance) {
        n_u::Synchronized pooler(_instanceLock);
        delete _instance;
        _instance = 0;
    }
}

SamplePools::~SamplePools()
{
    _poolsLock.lock();
    std::list<SamplePoolInterface*> tmplist = _pools;
    _poolsLock.unlock();

    std::list<SamplePoolInterface*>::iterator pi = tmplist.begin();
    for ( ; pi != tmplist.end(); ++pi) {
        delete *pi;     // will remove itself from the _pools list.
    }
}

list<SamplePoolInterface*> SamplePools::getPools() const
{
    n_u::Synchronized pooler(_poolsLock);
    return _pools;
}

void SamplePools::addPool(SamplePoolInterface* pool)
{
    n_u::Synchronized pooler(_poolsLock);
    _pools.push_back(pool);
}

void SamplePools::removePool(SamplePoolInterface* pool)
{
    n_u::Synchronized pooler(_poolsLock);
    list<SamplePoolInterface*>::iterator li =
	find(_pools.begin(),_pools.end(),pool);
    if (li != _pools.end()) _pools.erase(li);
}

