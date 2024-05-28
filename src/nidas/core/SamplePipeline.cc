/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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

#include "SamplePipeline.h"
#include "SampleBuffer.h"
#include "SampleSorter.h"
#include "DSMSensor.h"

#include <nidas/util/Logger.h>

using namespace nidas::core;
using namespace std;

namespace n_u = nidas::util;
using nidas::util::endlog;

SamplePipeline::SamplePipeline() :
	_name("SamplePipeline"),
        _rawMutex(),_rawSorter(0),
	_procMutex(),_procSorter(0),
        _sampleTags(),_dsmConfigs(),
        _realTime(false),
        _rawSorterLength(0.0),
        _procSorterLength(0.0),
        _rawHeapMax (100000000),
        _procHeapMax(100000000),
        _heapBlock(false),
        _keepStats(false),
        _rawLateSampleCacheSize(0),
        _procLateSampleCacheSize(0)
{
}

SamplePipeline::~SamplePipeline()
{
    _rawMutex.lock();
    delete _rawSorter;
    _rawMutex.unlock();

    _procMutex.lock();
     delete _procSorter;
    _procMutex.unlock();
}

void SamplePipeline::interrupt()
{
    _rawMutex.lock();
    if (_rawSorter) _rawSorter->interrupt();
    _rawMutex.unlock();

    _procMutex.lock();
    if (_procSorter) _procSorter->interrupt();
    _procMutex.unlock();
}

void SamplePipeline::join() throw()
{
    _rawMutex.lock();
    if (_rawSorter) {
        if (_rawSorter->isRunning()) {
            _rawSorter->interrupt();
            try {
                _rawSorter->join();
            }
            catch(const n_u::Exception& e) {
                WLOG(("SamplePipeline: %s: %s",
                    _rawSorter->getName().c_str(),e.what()));
            }
        }
    }
    _rawMutex.unlock();

    _procMutex.lock();
    if (_procSorter) {
        if (_procSorter->isRunning()) {
            _procSorter->interrupt();
            try {
                _procSorter->join();
            }
            catch(const n_u::Exception& e) {
                WLOG(("SamplePipeline: %s: %s",
                    _procSorter->getName().c_str(),e.what()));
            }
        }
    }
    _procMutex.unlock();
}

void SamplePipeline::rawinit()
{
    n_u::Autolock autolock(_rawMutex);
    if (!_rawSorter) {
        if (getRawSorterLength() > 0) {
            _rawSorter = new SampleSorter(_name + "RawSorter",true);
            _rawSorter->setLengthSecs(getRawSorterLength());
            _rawSorter->setLateSampleCacheSize(getRawLateSampleCacheSize());
        }
        else {
            _rawSorter = new SampleBuffer(_name + "RawBuffer",true);
        }
        _rawSorter->setHeapMax(getRawHeapMax());
        _rawSorter->setHeapBlock(getHeapBlock());
        _rawSorter->setRealTime(getRealTime());
        _rawSorter->setKeepStats(_keepStats);
        VLOG(("RawSorter: length=%.3f secs, heapMax=%d MB, "
              "stats=%d, realtime=",
              _rawSorter->getLengthSecs(),_rawSorter->getHeapMax()/1000000,
              _rawSorter->getKeepStats()) << _rawSorter->getRealTime());
        if (getRealTime())
        {
            _rawSorter->setRealTimeFIFOPriority(40);
        }
        _rawSorter->start();
    }
}

void SamplePipeline::procinit()
{
    n_u::Autolock autolock(_procMutex);
    if (!_procSorter) {
        if (getProcSorterLength() > 0) {
            _procSorter = new SampleSorter(_name + "ProcSorter",false);
            _procSorter->setLengthSecs(getProcSorterLength());
            _procSorter->setLateSampleCacheSize(getProcLateSampleCacheSize());
        }
        else {
            _procSorter = new SampleBuffer(_name + "ProcBuffer",false);
        }
        _procSorter->setHeapMax(getProcHeapMax());
        _procSorter->setHeapBlock(getHeapBlock());
        _procSorter->setRealTime(getRealTime());
        _procSorter->setKeepStats(_keepStats);
        VLOG(("ProcSorter: length=%f secs, heapMax=%d MB, "
              "stats=%d, realtime=",
              _procSorter->getLengthSecs(),_procSorter->getHeapMax()/1000000,
              _procSorter->getKeepStats()) << _procSorter->getRealTime());
        if (getRealTime())
        {
            _procSorter->setRealTimeFIFOPriority(30);
        }
        _procSorter->start();
    }
}

void SamplePipeline::connect(SampleSource* src) throw()
{
    rawinit();
    procinit();

    static n_u::LogContext clog(LOG_VERBOSE, "slice_debug");
    static n_u::LogMessage cmsg(&clog);
    SampleSource* rawsrc = src->getRawSampleSource();

    if (rawsrc) {
        SampleTagIterator si = rawsrc->getSampleTagIterator();
        for ( ; si.hasNext(); ) {
            const SampleTag* stag = si.next();
            if (clog.active())
            {
                dsm_sample_id_t rawid = stag->getId() - stag->getSampleId();
                cmsg << "connect rawid=" << GET_DSM_ID(rawid) << ','
                     << GET_SPS_ID(rawid) << endlog;
                cmsg << "connect id=" << GET_DSM_ID(stag->getId()) << ','
                     << GET_SPS_ID(stag->getId()) << endlog;
            }
            const DSMSensor* sensor = stag->getDSMSensor();
            if (sensor) {
                if (clog.active())
                {
                    cmsg << "sensor=" << sensor->getName() << endlog;
                }
                SampleTagIterator si2 = sensor->getSampleTagIterator();
                for ( ; si2.hasNext(); ) {
                    const SampleTag* stag2 = si2.next();
                    _procSorter->addSampleTag(stag2);
                }
            }
            _rawSorter->addSampleTag(stag);
        }
        rawsrc->addSampleClient(_rawSorter);
    }
    else {
        src = src->getProcessedSampleSource();
        if (!src) return;
        SampleTagIterator si = src->getSampleTagIterator();
        for ( ; si.hasNext(); ) {
            const SampleTag* stag = si.next();
            _procSorter->addSampleTag(stag);
        }
        src->addSampleClient(_procSorter);
    }
}

void SamplePipeline::disconnect(SampleSource* src) throw()
{

    SampleSource* rawsrc = src->getRawSampleSource();

    if (rawsrc) {
        {
            n_u::Autolock autolock(_rawMutex);
            if (!_rawSorter) return;
        }
        rawsrc->removeSampleClient(_rawSorter);
        {
            n_u::Autolock autolock(_procMutex);
            if (!_procSorter) return;
        }
        SampleTagIterator si = rawsrc->getSampleTagIterator();
        for ( ; si.hasNext(); ) {
            const SampleTag* stag = si.next();
            _rawSorter->removeSampleTag(stag);
            // dsm_sample_id_t rawid = stag->getId() - stag->getSampleId();
            const DSMSensor* sensor = stag->getDSMSensor();
            if (sensor) {
                SampleTagIterator si2 = sensor->getSampleTagIterator();
                for ( ; si2.hasNext(); ) {
                    const SampleTag* stag2 = si2.next();
                    _procSorter->removeSampleTag(stag2);
                }
            }
        }
    }
    else {
        {
            n_u::Autolock autolock(_procMutex);
            if (!_procSorter) return;
        }
        src = src->getProcessedSampleSource();
        if (!src) return;
        SampleTagIterator si = src->getSampleTagIterator();
        for ( ; si.hasNext(); ) {
            const SampleTag* stag = si.next();
            _procSorter->removeSampleTag(stag);
        }

        src->removeSampleClient(_procSorter);
    }
}

void SamplePipeline::addSampleClient(SampleClient* client) throw()
{
    rawinit();
    procinit();

    list<const SampleTag*> rtags = _rawSorter->getSampleTags();
    list<const SampleTag*>::const_iterator si = rtags.begin();

    // add a client for all possible processed SampleTags.
    for ( ; si != rtags.end(); ++si) {
        const SampleTag* stag = *si;
        // dsm_sample_id_t rawid = stag->getId();
        DSMSensor* sensor = const_cast<DSMSensor*>(stag->getDSMSensor());
        if (sensor) {
            VLOG(("addSampleClient sensor=") << sensor->getName());
            sensor->addSampleClient(_procSorter);
            stag = sensor->getRawSampleTag();
            _rawSorter->addSampleClientForTag(sensor,stag);
        }
    }
    _procSorter->addSampleClient(client);
}

void SamplePipeline::removeSampleClient(SampleClient* client) throw()
{
    {
        n_u::Autolock autolock(_procMutex);
        if (!_procSorter) return;
    }
    _procSorter->removeSampleClient(client);

    //	If there are no clients of procSorter then clean up.
    if (_procSorter->getClientCount() == 0) {
        {
            n_u::Autolock autolock(_procMutex);
            if (!_rawSorter) return;
        }
        list<const SampleTag*> rtags = _rawSorter->getSampleTags();
        list<const SampleTag*>::const_iterator si = rtags.begin();
        for ( ; si != rtags.end(); ++si) {
            const SampleTag* stag = *si;
            // dsm_sample_id_t rawid = stag->getId();
            DSMSensor* sensor = const_cast<DSMSensor*>(stag->getDSMSensor());
            if (sensor) {
                sensor->removeSampleClient(_procSorter);
                stag = sensor->getRawSampleTag();
                _rawMutex.lock();
                if (_rawSorter) _rawSorter->removeSampleClientForTag(sensor,stag);
                _rawMutex.unlock();
            }
        }
    }
}

void SamplePipeline::addSampleClientForTag(SampleClient* client,
	const SampleTag* stag) throw()
{
    rawinit();
    procinit();

    // dsm_sample_id_t rawid = stag->getId() - stag->getSampleId();
    DSMSensor* sensor = const_cast<DSMSensor*>(stag->getDSMSensor());
    if (stag->getSampleId() != 0 && sensor) {
        _procSorter->addSampleClientForTag(client,stag);

        sensor->addSampleClient(_procSorter);

        stag = sensor->getRawSampleTag();
        _rawSorter->addSampleClientForTag(sensor,stag);
    }
}

void SamplePipeline::removeSampleClientForTag(SampleClient* client,
	const SampleTag* stag) throw()
{
    {
        n_u::Autolock autolock(_procMutex);
        if (!_procSorter) return;
    }

    DSMSensor* sensor = const_cast<DSMSensor*>(stag->getDSMSensor());

    if (stag->getSampleId() != 0 && sensor) {
        _procSorter->removeSampleClientForTag(client,stag);
    }
    else {
        _rawMutex.lock();
        if (_rawSorter) _rawSorter->removeSampleClientForTag(client,stag);
        _rawMutex.unlock();
    }

    //	If there are no clients of procSorter then clean up.
    if (_procSorter->getClientCount() == 0) {
        sensor->removeSampleClient(_procSorter);
        stag = sensor->getRawSampleTag();
        _rawMutex.lock();
        if (_rawSorter) _rawSorter->removeSampleClientForTag(sensor,stag);
        _rawMutex.unlock();
    }
}

