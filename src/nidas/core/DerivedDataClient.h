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

#ifndef _nidas_core_DerivedDataClient_h_
#define _nidas_core_DerivedDataClient_h_

#include <nidas/core/DerivedDataReader.h>

namespace nidas { namespace core {

/**
 * Interface of a DerivedDataClient of the DerivedDataReader.
 */
class DerivedDataClient {
public:

  virtual ~DerivedDataClient() {}

  /**
   * Method called on a DerivedDataClient by the DerivedDataReader
   * thread when a new packet of derived data has been received.
   * This method is called on the DerivedDataClients
   * in the sequence that they registered with the DerivedDataReader.
   * The implementation of this method in a client should
   * run quickly, to reduce the delay in calling successive clients
   * in the list.
   */
  virtual void derivedDataNotify(const DerivedDataReader * s) throw() = 0;

};

}}	// namespace nidas namespace core

#endif
