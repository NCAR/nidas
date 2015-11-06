// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2004, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_CORE_SAMPLESOURCE_H
#define NIDAS_CORE_SAMPLESOURCE_H

#include <nidas/core/NidsIterators.h>
#include <nidas/core/SampleStats.h>
#include <nidas/util/InvalidParameterException.h>

namespace nidas { namespace core {

class SampleClient;
class SampleTag;
class Sample;

/**
 * Pure virtual interface for a source of Samples.
 * Implementations of SampleSource typically maintain a list
 * of SampleClients.  When a SampleSource has a Sample ready,
 * it will call the receive method of all its SampleClients.
 * SampleClients register/unregister with a SampleSource via
 * the addSampleClient/removeSampleClient methods.
 */
class SampleSource {
public:

    virtual ~SampleSource() {}

    /**
     * Several objects in NIDAS can be both a SampleSource of raw Samples
     * and processed Samples. SampleClients use this method to
     * get a pointer to whatever sample source they are interested
     * in. Derived classes can return NULL if they are not
     * a SampleSource of raw samples.
     */
    virtual SampleSource* getRawSampleSource() = 0;

    /**
     * Several objects in NIDAS can be both a SampleSource of raw Samples
     * and processed Samples. SampleClients use this method to
     * get a pointer to whatever sample source they are interested
     * in. Derived classes can return NULL if they are not
     * a SampleSource of processed samples.
     */
    virtual SampleSource* getProcessedSampleSource() = 0;

    /**
     * Add a SampleTag to this SampleSource. This SampleSource
     * does not own the SampleTag.
     */
    virtual void addSampleTag(const SampleTag*)
        throw (nidas::util::InvalidParameterException) = 0;

    virtual void removeSampleTag(const SampleTag*) throw () = 0;

    /**
     * What SampleTags am I a SampleSource for?
     */
    virtual std::list<const SampleTag*> getSampleTags() const = 0;

    virtual SampleTagIterator getSampleTagIterator() const = 0;

    /**
     * Add a SampleClient of all Samples to this SampleSource.
     * The pointer to the SampleClient must remain valid, until after
     * it is removed.
     */
    virtual void addSampleClient(SampleClient* c) throw() = 0;

    /**
     * Remove a SampleClient from this SampleSource.
     */
    virtual void removeSampleClient(SampleClient* c) throw() = 0;

    /**
     * Add a SampleClient to this SampleSource.  The pointer
     * to the SampleClient must remain valid, until after
     * it is removed.
     */
    virtual void addSampleClientForTag(SampleClient* c,const SampleTag*) throw() = 0;
    /**
     * Remove a SampleClient for a given SampleTag from this SampleSource.
     * The pointer to the SampleClient must remain valid, until after
     * it is removed.
     */
    virtual void removeSampleClientForTag(SampleClient* c,const SampleTag*) throw() = 0;

    /**
     * How many SampleClients are currently in my list.
     */
    virtual int getClientCount() const throw() = 0;

    /**
     * Request that this SampleSource flush it's samples.
     * One must think about whether to call flush() on
     * SampleClients of this SampleSource. A SampleClient may
     * have multiple SampleSources and a flush() of it
     * when one SampleSource() is done may not be what is wanted.
     */
    virtual void flush() throw() = 0;

    virtual const SampleStats& getSampleStats() const = 0;

};

}}	// namespace nidas namespace core


#endif
