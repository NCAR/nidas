/*
    Copyright 2007 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/core/ReadDerived.h $

*/

#ifndef _nidas_core_ReadDerived_h_
#define _nidas_core_ReadDerived_h_

#include <nidas/core/DerivedDataClient.h>

#include <nidas/util/Socket.h>
#include <nidas/util/ParseException.h>
#include <nidas/util/Thread.h>

namespace nidas { namespace core {

/**
 * This class will read, parse and make available the parameters in the
 * onboard real-time broadcast of data.
 */
class ReadDerived : public nidas::util::Thread
{
public:

  /**
   * Constructor.  Generally the user does not call this
   * constructor directly since in ordinary use it is a singleton.
   * Instead, the first instance should be created with the static
   * createInstance() method. A pointer to the singleton can be
   * gotten with the static getInstance() method.
   */
  ReadDerived(const nidas::util::Inet4SocketAddress&)
    throw(nidas::util::IOException);

  ~ReadDerived();

  float getTrueAirspeed() const		{ return _tas; }
  float getAltitude() const		{ return _alt; }
  float getRadarAltitude() const	{ return _alt; }

  int run() throw(nidas::util::Exception);

  int  getFd() const
  {
      return _usock.getFd();
  }

  /**
   * Read data callback.
   */
  void readData() throw(nidas::util::IOException,nidas::util::ParseException);

  /**
   * Add a client to ReadDerived.  The derivedDataNotify method of the
   * client will be called when derived data is received.
   */
  void addClient(DerivedDataClient * ddc);

  void removeClient(DerivedDataClient * ddc);

  /**
   * Create the instance of ReadDerived.
   */
  static ReadDerived * createInstance(const nidas::util::Inet4SocketAddress&)
    throw(nidas::util::IOException);

  /**
   * Delete the singleton instance of ReadDerived, shutting down the
   * thread if is is running.
   */
  static void deleteInstance();

  /**
   * Fetch the pointer to the instance of ReadDerived.
   */
  static ReadDerived * getInstance();

private:
    void notifyClients();

  static ReadDerived * _instance;

  static nidas::util::Mutex _instanceMutex;

  nidas::util::Mutex _clientMutex;

  std::list<DerivedDataClient*> _clients;

  /** 
   * Socket for reading the derived data.
   */
  nidas::util::DatagramSocket _usock;

  /**
   * Parse the IWGADTS trivial broadcast.
   */
  bool parseIWGADTS(char s[]) throw(nidas::util::ParseException);

  time_t _lastUpdate;	// Store last time we received a broadcast.

  float _tas;		// True Airspeed.  Meters per second
  float _alt;		// Altitude (probably GPS).  Meters
  float _radarAlt;	// Distance above surface/ground.  Meters

};

}}      // namespace nidas namespace core

#endif

