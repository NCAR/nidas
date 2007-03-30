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


namespace nidas { namespace core {

class ReadDerived;

/**
 * Interface of a DerivedDataClient.
 */
class DerivedDataClient {
public:

  virtual ~DerivedDataClient() {}

  virtual void derivedDataNotify(const ReadDerived * s) throw() = 0;

};

}}	// namespace nidas namespace core

#endif
