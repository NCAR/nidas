// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
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

#ifndef NIDAS_CORE_SAMPLECLIENTLIST_H
#define NIDAS_CORE_SAMPLECLIENTLIST_H

#include <list>

#include <nidas/util/ThreadSupport.h>
#include <nidas/core/SampleClient.h>

namespace nidas { namespace core {

/**
 * A list of SampleClients.  This class is typically used by
 * SampleSources to maintain their client lists.
 */
class SampleClientList {
public:

  SampleClientList(): _clistLock(), _clients() {}

  /**
   * Public copy constructor. To support multithreading,
   * this first locks the SampleClientList argument.
   */
  SampleClientList(const SampleClientList& cl);

  virtual ~SampleClientList() {}

  /**
   * Public assignment operator.  To support multithreading,
   * this first locks the SampleClientList argument.
   */
  SampleClientList& operator=(const SampleClientList& cl);

  /**
   * Add a SampleClient to this list.  The pointer
   * to the SampleClient must remain valid, until after
   * it is removed. A nidas::util::Mutex exclusion lock
   * is used to avoid simultaneous access.
   */
  virtual void add(SampleClient*);

  /**
   * Remove a SampleClient from this SampleSource.
   * A nidas::util::Mutex exclusion lock
   * is used to avoid simultaneous access.
   */
  virtual void remove(SampleClient*);

  /**
   * Are there any clients?
   */
  virtual bool empty() const;

  /**
   * Big cleanup.
   * A nidas::util::Mutex exclusion lock
   * is used to avoid simultaneous access.
   */
  virtual void removeAll();

  /**
   * Lock this list.
   */
  void lock() const throw() { _clistLock.lock(); }

  /**
   * Unlock this list.
   */
  void unlock() const throw() { _clistLock.unlock(); }

  /** get a const_iterator pointing to first element. Does not lock! */
  std::list<SampleClient*>::const_iterator begin() throw() { return _clients.begin(); }

  /** get a const_iterator pointing to one-past-last element. Does not lock! */
  std::list<SampleClient*>::const_iterator end() throw() { return _clients.end(); }

private:

  /**
   * mutex to prevent simultaneous access to clients list
   */
  mutable nidas::util::Mutex _clistLock;

  /**
   * My current clients.
   */
  std::list<SampleClient*> _clients;

};

}}	// namespace nidas namespace core

#endif
