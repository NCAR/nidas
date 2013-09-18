// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
    Copyright 2007 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

*/

#ifndef _nidas_core_DerivedDataReader_h_
#define _nidas_core_DerivedDataReader_h_

#include <nidas/util/SocketAddress.h>
#include <nidas/util/ParseException.h>
#include <nidas/util/Thread.h>

#include <vector>

namespace nidas { namespace core {

class DerivedDataClient;

/**
 * This class will read, parse and make available the parameters in the
 * onboard real-time broadcast of data.
 */
class DerivedDataReader : public nidas::util::Thread
{
public:

    float getTrueAirspeed() const		{ return _tas; }
    float getAmbientTemperature() const	{ return _at; }
    float getLatitude() const		{ return _lat; }
    float getLongitude() const		{ return _lon; }
    float getAltitude() const		{ return _alt; }
    float getRadarAltitude() const	{ return _radarAlt; }
    float getTrueHeading() const		{ return _thdg; }
    float getGroundSpeed() const		{ return _grndSpd; }

    int run() throw(nidas::util::Exception);

    /**
     * Add a client to DerivedDataReader.  The derivedDataNotify method of the
     * client will be called when derived data is received.
     */
    void addClient(DerivedDataClient * ddc);

    void removeClient(DerivedDataClient * ddc);

    /**
     * Create the instance of DerivedDataReader.
     */
    static DerivedDataReader * createInstance(const nidas::util::SocketAddress&);

    /**
     * Delete the singleton instance of DerivedDataReader, shutting down the
     * thread if is is running.
     */
    static void deleteInstance();

    /**
     * Fetch the pointer to the instance of DerivedDataReader.
     */
    static DerivedDataReader * getInstance();

    void interrupt();

private:

    /**
     * Constructor.  The user does not call this
     * constructor directly since this class is a singleton.
     * Instead, the first instance should be created with the static
     * createInstance() method. A pointer to the singleton can be
     * gotten with the static getInstance() method.
     */
    DerivedDataReader(const nidas::util::SocketAddress&);

    ~DerivedDataReader();

    void notifyClients();

    static DerivedDataReader * _instance;

    static nidas::util::Mutex _instanceMutex;

    nidas::util::Mutex _clientMutex;

    std::list<DerivedDataClient*> _clients;

    nidas::util::SocketAddress* _saddr;

    /**
     * Parse the IWGADTS trivial broadcast.
     * @return: number of expected comma delimited fields that were found.
     * _nparseErrors is incremented if the expected number of comma delimited fields
     * are not found, and for every field that is not parseable with %f.
     */
    int parseIWGADTS(const char *);

    float _tas;		// True Airspeed.  Meters per second
    float _at;		// Ambient Temperature.  deg_C
    float _lat;	        // Latitude, deg north
    float _lon;         // Longitude, deg east
    float _alt;		// Altitude (probably GPS).  Meters
    float _radarAlt;	// Distance above surface/ground.  Meters
    float _thdg;	// True Heading. degrees_true
    float _grndSpd;	// Ground Speed. meters per second.

    int _parseErrors;
    int _errorLogs;

    struct IWG1_Field {
        IWG1_Field(int n,float* p): nf(n),fp(p) {}
        int nf;         // which field in the IWG1 string, after the "IWG1,timetag,"
        float *fp;      // pointer to the data
    };

    std::vector<IWG1_Field> _fields;

    /** No copying. */
    DerivedDataReader(const DerivedDataReader&);

    /** No assignment. */
    DerivedDataReader& operator=(const DerivedDataReader&);

};

}}      // namespace nidas namespace core

#endif

