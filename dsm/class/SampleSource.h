
/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $


*/

#ifndef DSM_RAWSAMPLESOURCE_H
#define DSM_RAWSAMPLESOURCE_H

#include <list>

#include <atdUtil/ThreadSupport.h>
#include <SampleClient.h>

namespace dsm {

/**
 * Interface of a source of samples.
 */
class SampleSource {
public:

  virtual ~SampleSource() {}

  /**
   * Add a SampleClient to this SampleSource.  The pointer
   * to the SampleClient must remain valid, until after
   * it is removed.
   */
  virtual void addSampleClient(SampleClient*);

  /**
   * Remove a SampleClient from this SampleSource
   */
  virtual void removeSampleClient(SampleClient*);

  /**
   * Big cleanup.
   */
  virtual void removeAllSampleClients();

  /**
   * How many samples have been distributed by this SampleSource.
   */
  unsigned long getNumSamplesSent() const { return numSamplesSent; }

  void setNumSamplesSent(unsigned long val) { numSamplesSent = val; }

  /**
   * Distribute this sample to my clients.
   */
  virtual void distribute(const Sample*)
  	throw(SampleParseException,atdUtil::IOException);

protected:

  /**
   * mutex to prevent simultaneous access to clients list
   */
  atdUtil::Mutex clistLock;

  /**
   * My current clients.
   */
  std::list<SampleClient*> clients;

  /**
   * Number of samples distributed.
   */
  int numSamplesSent;

};
}

#endif
