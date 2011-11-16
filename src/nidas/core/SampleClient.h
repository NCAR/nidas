// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_SAMPLECLIENT_H
#define NIDAS_CORE_SAMPLECLIENT_H

#include <nidas/core/Sample.h>
#include <nidas/util/IOException.h>

namespace nidas { namespace core {

/**
 * Interface of a SampleClient.
 */
class SampleClient {
public:

  virtual ~SampleClient() {}
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
  virtual bool receive(const Sample *s) throw() = 0;

  /**
   * Method called to indicate the end of the input samples
   * and that we should finish off processing, e.g.
   * compute the last set of statistics and send them on.
   * The default implementation does nothing, so derived
   * classes should override finish if they need to.
   */
  virtual void finish() throw() {}

};

}}	// namespace nidas namespace core

#endif
