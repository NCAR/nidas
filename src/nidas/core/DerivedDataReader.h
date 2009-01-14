/*
    Copyright 2007 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2007-01-31 11:23:38 -0700 (Wed, 31 Jan 2007) $

    $LastChangedRevision: 3648 $

    $LastChangedBy: cjw $

    $HeadURL: http://svn/svn/nids/trunk/src/nidas/core/DerivedDataReader.h $

*/

#ifndef _nidas_core_DerivedDataReader_h_
#define _nidas_core_DerivedDataReader_h_

#include <nidas/util/Socket.h>
#include <nidas/util/ParseException.h>
#include <nidas/util/Thread.h>

namespace nidas { namespace core {

class DerivedDataClient;

/**
 * This class will read, parse and make available the parameters in the
 * onboard real-time broadcast of data.
 */
class DerivedDataReader : public nidas::util::Thread
{
public:

  /**
   * Constructor.  Generally the user does not call this
   * constructor directly since in ordinary use it is a singleton.
   * Instead, the first instance should be created with the static
   * createInstance() method. A pointer to the singleton can be
   * gotten with the static getInstance() method.
   */
  DerivedDataReader(const nidas::util::Inet4SocketAddress&)
    throw(nidas::util::IOException);

  ~DerivedDataReader();

  float getTrueAirspeed() const		{ return _tas; }
  float getAmbientTemperature() const	{ return _at; }
  float getAltitude() const		{ return _alt; }
  float getRadarAltitude() const	{ return _alt; }
  float getTrueHeading() const		{ return _thdg; }

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
   * Add a client to DerivedDataReader.  The derivedDataNotify method of the
   * client will be called when derived data is received.
   */
  void addClient(DerivedDataClient * ddc);

  void removeClient(DerivedDataClient * ddc);

  /**
   * Create the instance of DerivedDataReader.
   */
  static DerivedDataReader * createInstance(const nidas::util::Inet4SocketAddress&)
    throw(nidas::util::IOException);

  /**
   * Delete the singleton instance of DerivedDataReader, shutting down the
   * thread if is is running.
   */
  static void deleteInstance();

  /**
   * Fetch the pointer to the instance of DerivedDataReader.
   */
  static DerivedDataReader * getInstance();

private:
    void notifyClients();

  static DerivedDataReader * _instance;

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

  bool parseIWGADTS(const char *) throw(nidas::util::ParseException);

  time_t _lastUpdate;	// Store last time we received a broadcast.

  float _tas;		// True Airspeed.  Meters per second
  float _at;		// Ambient Temperature.  deg_C
  float _alt;		// Altitude (probably GPS).  Meters
  float _radarAlt;	// Distance above surface/ground.  Meters
  float _thdg;		// True Heading. degrees_true

  int _parseErrors;

};

}}      // namespace nidas namespace core

#endif

