// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
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

#ifndef NIDAS_DYNLD_ISFF_PACKETS_H
#define NIDAS_DYNLD_ISFF_PACKETS_H

#include <nidas/util/UTime.h>
#include <nidas/util/ParseException.h>

#include <sys/types.h>
#include <regex.h>
#include <iostream>

namespace nidas { namespace dynld { namespace isff {

class PacketInfo {
public:
  virtual ~PacketInfo() {}
  virtual const char * header() const = 0;

  virtual bool scan(const char*) = 0;

  virtual std::ostream& print(std::ostream &) const = 0;
  virtual float getSignalStrength() const = 0;
  virtual float getFreqOffset() const = 0;
  virtual int getChannel() const = 0;
  virtual int getStatusInt() const = 0;
  virtual int getLength() const = 0;
};

inline std::ostream& operator<<(std::ostream& s, const PacketInfo& q)
{
    return q.print(s);
} 

class NESDISPacketInfo : public PacketInfo {
public:
    NESDISPacketInfo();

    /**
     * Scan a string for info fields. 
     * @return true: all fields scanned
     *         false: not all fields scanned.
     */
    bool scan(const char *);

    std::ostream& print(std::ostream &) const;

    /**
     * Header for a printed listing of NESDISPacketInfo's
     */
    const char* header() const;

    float getSignalStrength() const { return (float) _sigdbm; }
    float getFreqOffset() const { return (float) _freqError * 50; }
    int getChannel() const {
    	return _EW == 'E' ? _channel : _EW == 'W' ? -_channel : -99999; }
    int getStatusInt() const;
    int getLength() const { return _len; }

private:
    char _messageStatus;
    int _sigdbm;
    int _freqError;
    char _modIndex;
    char _dataQuality;
    int _channel;
    char _EW;
    int _len;

};

class SutronPacketInfo : public PacketInfo {
public:

    SutronPacketInfo();

    bool scan(const char *);

    std::ostream& print(std::ostream &) const;

    /**
     * Header for a printed listing of SutronPacketInfo's
     */
    const char* header() const;

    float getSignalStrength() const { return _sigdbm; }
    float getFreqOffset() const { return (float) _freqError; }
    int getChannel() const { return -99999; }
    int getStatusInt() const { return 0; }
    int getLength() const { return _len; }

private:
    int _modNumber;
    float _sigdbm;
    int _freqError;
    int _modPhase;
    float _SNratio;
    int _len;

};
  
class PacketParser {
public:

    PacketParser() throw(nidas::util::ParseException);

    ~PacketParser();

    enum packet_type {NESDIS_PT,UNKNOWN_PT};

    enum info_type {NESDIS_IT,SUTRON_IT,UNKNOWN_IT};

    enum packet_type parse(const char*) throw(nidas::util::ParseException);

    const nidas::util::UTime& getPacketTime() const
    {
        return _packetTime;
    }

    const PacketInfo* getPacketInfo() const
    {
        return _packetInfo;
    }

    int getStationId() const
    {
        return _stationId;
    }

    int getConfigId() const
    {
        return _configId;
    }

    int getSampleId() const
    {
        return _sampleId;
    }

    void parseData(float*,int nvars);

private:

    static ::regex_t** _packetPreg;

    static ::regex_t** _infoPreg;

    int _nPacketTypes;

    int _nInfoTypes;

    /**
     * Max number of parenthesized expressions in any regular expression.
     */
    const size_t _nmatch;

    /**
     * Array for returning pointer to beginning of the match to
     * each parenthesized expression.
     */
    ::regmatch_t *_pmatch;

    static nidas::util::Mutex _pregMutex;

    PacketInfo* _packetInfo;

    int _infoType;

    nidas::util::UTime _packetTime;

    int _stationId;

    /**
     * Pointer into the current packet.
     */
    const char* _packetPtr;

    /**
     * One past the end of the current packet.
     */
    const char* _endOfPacket;

    int _configId;

    int _sampleId;

    /** No copying. */
    PacketParser(const PacketParser&);

    /** No assignment. */
    PacketParser& operator=(const PacketParser&);

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
