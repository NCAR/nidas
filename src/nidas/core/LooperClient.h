/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-02-11 17:55:33 -0700 (Sat, 11 Feb 2006) $

    $LastChangedRevision: 3284 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/ISFF_TREX/dsm/class/SampleClient.h $
 ********************************************************************

*/

#ifndef NIDAS_CORE_LOOPERCLIENT_H
#define NIDAS_CORE_LOOPERCLIENT_H

namespace nidas { namespace core {

/**
 * Interface of a client of Looper. An object that wants
 * a method called periodically should implement
 * LooperClient::looperNotify() and register itself with a
 * Looper via Looper::addClient().
 */
class LooperClient {
public:

  virtual ~LooperClient() {}

  /**
   * Method called by Looper. This method should not be a
   * heavy user of resources, since the notification of
   * other clients is delayed until this method finishes.
   * If much work is to be done, this method should
   * post a semaphore for another worker thread to proceed.
   */
  virtual void looperNotify() throw() = 0;

};

}}	// namespace nidas namespace core

#endif
