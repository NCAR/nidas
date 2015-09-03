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

#ifndef NIDAS_CORE_SAMPLECLIENT_H
#define NIDAS_CORE_SAMPLECLIENT_H

#include <nidas/core/Sample.h>
#include <nidas/util/IOException.h>

namespace nidas { namespace core {

/**
 * Pure virtual interface of a client of Samples.
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
   * Ask that this SampleClient send out any buffered Samples that it
   * may be holding.
   */
  virtual void flush() throw() = 0;

};

}}	// namespace nidas namespace core

#endif
