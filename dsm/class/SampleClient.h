/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_RAWSAMPLECLIENT_H
#define DSM_RAWSAMPLECLIENT_H

#include <Sample.h>
#include <SampleParseException.h>
#include <atdUtil/IOException.h>

namespace dsm {

/**
 * Interface of a SampleClient.
 */
class SampleClient {
public:

  /**
   * Method called to pass a sample to this client.
   * This method is typically called by a SampleSource
   * for each of its SampleClients when it has a sample ready.
   * Returns
   *   true: success
   *   false: sample rejected. This is meant to signal a
   *     warning-type situation - like a socket not
   *     being available temporarily.  True errors
   *     will be thrown as an IOException.
   */
  virtual bool receive(const Sample *s)
  	throw(SampleParseException, atdUtil::IOException) = 0;

  /**
   * Called by external object to reset this SampleClient. 
   * For example, re-open a socket.
   */
  virtual void reset(Sample *s) throw(atdUtil::IOException) {}
};
}

#endif
