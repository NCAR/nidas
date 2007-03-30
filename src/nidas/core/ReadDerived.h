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
#include <nidas/util/ThreadSupport.h>

namespace nidas { namespace core {

/**
 * This class will read, parse and make available the parameters in the
 * onboard real-time broadcast of data.
 */
class ReadDerived
{
public:
  float getTrueAirspeed() const		{ return _tas; }
  float getAltitude() const		{ return _alt; }
  float getRadarAltitude() const	{ return _alt; }

  int  getFd() const	{ return _udp->getFd(); }

  /**
   * Read data callback.
   */
  void readData() throw(nidas::util::IOException,nidas::util::ParseException);

  void addClient(DerivedDataClient * ddc);
  void removeClient(DerivedDataClient * ddc);

  /**
   * Fetch the pointer to the instance of Looper
   */
  static ReadDerived * getInstance();

private:
	ReadDerived();
	~ReadDerived();

        void notifyClients();

  static ReadDerived * _instance;

  static nidas::util::Mutex _instanceMutex;

  nidas::util::Mutex _clientMutex;

  std::list<DerivedDataClient*> _clients;

  /**
   * Parse the IWGADTS trivial broadcast.
   */
  bool parseIWGADTS(char s[]) throw(nidas::util::ParseException);

  nidas::util::DatagramSocket * _udp;

  time_t _lastUpdate;	// Store last time we received a broadcast.

  float _tas;		// True Airspeed.  Meters per second
  float _alt;		// Altitude (probably GPS).  Meters
  float _radarAlt;	// Distance above surface/ground.  Meters

};

}}      // namespace nidas namespace core

#endif

