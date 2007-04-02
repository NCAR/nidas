/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/core/DerivedDataClient.h $
 ********************************************************************
*/

#ifndef _nidas_core_DerivedDataClient_h_
#define _nidas_core_DerivedDataClient_h_

#include <nidas/core/DerivedDataReader.h>

namespace nidas { namespace core {

/**
 * Interface of a DerivedDataClient.
 */
class DerivedDataClient {
public:

  virtual ~DerivedDataClient() {}

  virtual void derivedDataNotify(const DerivedDataReader * s) throw() = 0;

};

}}	// namespace nidas namespace core

#endif
