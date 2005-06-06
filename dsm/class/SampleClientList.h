
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$


*/

#ifndef DSM_SAMPLECLIENTLIST_H
#define DSM_SAMPLECLIENTLIST_H

#include <list>

#include <atdUtil/ThreadSupport.h>
#include <SampleClient.h>

namespace dsm {

/**
 * A list of SampleClients.  This class is typically used by
 * SampleSources to maintain their client lists.
 */
class SampleClientList {
public:

  SampleClientList() {}

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
   * it is removed. A atdUtil::Mutex exclusion lock
   * is used to avoid simultaneous access.
   */
  virtual void add(SampleClient*);

  /**
   * Remove a SampleClient from this SampleSource.
   * A atdUtil::Mutex exclusion lock
   * is used to avoid simultaneous access.
   */
  virtual void remove(SampleClient*);

  /**
   * How many clients?
   */
  virtual int size() const;

  /**
   * Big cleanup.
   * A atdUtil::Mutex exclusion lock
   * is used to avoid simultaneous access.
   */
  virtual void removeAll();

  /**
   * Lock this list.
   */
  void lock() const throw() { clistLock.lock(); }

  /**
   * Unlock this list.
   */
  void unlock() const throw() { clistLock.unlock(); }

  /** get a const_iterator pointing to first element. Does not lock! */
  std::list<SampleClient*>::const_iterator begin() throw() { return clients.begin(); }

  /** get a const_iterator pointing to one-past-last element. Does not lock! */
  std::list<SampleClient*>::const_iterator end() throw() { return clients.end(); }

protected:

  /**
   * mutex to prevent simultaneous access to clients list
   */
  mutable atdUtil::Mutex clistLock;

  /**
   * My current clients.
   */
  std::list<SampleClient*> clients;

};
}

#endif
