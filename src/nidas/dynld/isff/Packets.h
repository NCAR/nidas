/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
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

    float getSignalStrength() const { return (float) sigdbm; }
    float getFreqOffset() const { return (float) freqError * 50; }
    int getChannel() const {
    	return EW == 'E' ? channel : EW == 'W' ? -channel : -99999; }
    int getStatusInt() const;
    int getLength() const { return len; }

private:
    char messageStatus;
    int sigdbm;
    int freqError;
    char modIndex;
    char dataQuality;
    int channel;
    char EW;
    int len;

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

    float getSignalStrength() const { return sigdbm; }
    float getFreqOffset() const { return (float) freqError; }
    int getChannel() const { return -99999; }
    int getStatusInt() const { return 0; }
    int getLength() const { return len; }

private:
    int modNumber;
    float sigdbm;
    int freqError;
    int modPhase;
    float SNratio;
    int len;

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
        return packetTime;
    }

    const PacketInfo* getPacketInfo() const
    {
        return packetInfo;
    }

    int getStationId() const
    {
        return stationId;
    }

    int getConfigId() const
    {
        return configId;
    }

    int getSampleId() const
    {
        return sampleId;
    }

    void parseData(float*,int nvars);

private:

    static ::regex_t** packetPreg;

    static ::regex_t** infoPreg;

    int nPacketTypes;

    int nInfoTypes;

    /**
     * Max number of parenthesized expressions in any regular expression.
     */
    const size_t nmatch;

    /**
     * Array for returning pointer to beginning of the match to
     * each parenthesized expression.
     */
    ::regmatch_t *pmatch;

    static nidas::util::Mutex pregMutex;

    int packetType;

    int infoType;

    PacketInfo* packetInfo;

    nidas::util::UTime packetTime;

    int stationId;

    /**
     * Pointer into the current packet.
     */
    const char* packetPtr;

    /**
     * One past the end of the current packet.
     */
    const char* endOfPacket;

    int configId;

    int sampleId;

};

}}}	// namespace nidas namespace dynld namespace isff

#endif
