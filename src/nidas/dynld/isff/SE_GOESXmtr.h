/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2005-10-28 14:50:09 -0600 (Fri, 28 Oct 2005) $

    $LastChangedRevision: 3093 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/trunk/dsm/class/Socket.h $
 ********************************************************************

*/

#ifndef NIDAS_DYNLD_ISFF_SE_GOESXMTR_H
#define NIDAS_DYNLD_ISFF_SE_GOESXMTR_H

#include <nidas/core/IOChannel.h>
#include <nidas/core/SampleTag.h>
#include <nidas/dynld/isff/GOESXmtr.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Logger.h>

#include <string>
#include <iostream>
#include <vector>

namespace nidas { namespace dynld { namespace isff {

using namespace nidas::core;

/**
 * An IOChannel implementation for a Signal Engineering GOES transmitter.
 */

class SE_GOESXmtr: public GOESXmtr {

public:

    /**
     * Constructor.
     */
    SE_GOESXmtr();

    /**
     * Copy constructor.
     */
    SE_GOESXmtr(const SE_GOESXmtr&);

    /**
     * Destructor.
     */
    ~SE_GOESXmtr();

    /**
     * Clone invokes default copy constructor.
     */
    SE_GOESXmtr* clone() const { return new SE_GOESXmtr(*this); }

    void init() throw (nidas::util::IOException);

    int getModel() const { return model; }

    void setModel(int val) { model = val; }

    virtual bool isNewFile() const { return false; }

    /**
     * Queue a sample for writing to a GOES transmitter.
    */
    void transmitData(const nidas::util::UTime& sendTime,
    	int configid,const Sample*)
    	throw (nidas::util::IOException);

    /**
     * Queue a sample for writing to a GOES transmitter.
    */
    void transmitDataSE110(const nidas::util::UTime& sendTime,
    	int configid,const Sample*)
    	throw (nidas::util::IOException);

    void transmitDataSE120(const nidas::util::UTime& sendTime,
    	int configid,const Sample*)
    	throw (nidas::util::IOException);

    void close() throw (nidas::util::IOException)
    {
        port.close();
    }

    /**
     * Sent a message to the GOES transmitter.
     * @param msg The basic message, without the leading SOH,
     *        or length, or the trailing CRC or EOT.
     */
    size_t send(const std::string& msg) throw(nidas::util::IOException);

    /**
     * A one-byte message, containing only the type.
     */
    size_t send(char c) throw(nidas::util::IOException);

    /**
     * Receive a message from the transmitter.
     * IOException is thrown if the message does not match specs:
     * 1. Is not enclosed in SOH,EOT.
     * 2. the length does not match what was read.
     * 3. The CRC check fails.
     * @return The unfix'd contents of the message, without the SOH,
     *     length, CRC or the EOT.
     */
    std::string recv() throw(nidas::util::IOException);

    void flush() throw(nidas::util::IOException)
    {
        port.flushBoth();
    }

    /**
     * Fix a string for transmission to a SE GOES transmitter.
     * SOH character is changed to  "#" followed by ~SOH (one's complement)
     * EOT character is changed to  "#" followed by ~EOT (one's complement)
     *   # character is changed to  "#" followed by ~#  (one's complement)
     */
    static std::string fix(const std::string& msg);

    /**
     * Unfix a string that was read from a SE GOES transmitter,
     * looking for '#c' where c is any character, and replacing
     * the two characters with the one's complement of c.
     */
    static std::string unfix(const std::string& msg);

    /**
     * Compute a CRC for the message.
     */
    static char crc(const std::string& msg);

    void encodeClock(const nidas::util::UTime& ut,char* out,bool fractsecs);

    nidas::util::UTime decodeClock(const char* in);

    /**
     * Raise RTS to wake up transmitter.
     */
    void wakeup() throw(nidas::util::IOException);

    /**
     * Lower RTS to put transmitter to sleep.
     */
    void tosleep() throw(nidas::util::IOException);

    /**
     * Query the GOES Transmitter.
     * @return false: not responding or not ready.
     *         true: A-OK.
     */
    void query() throw(nidas::util::IOException);

    int detectModel() throw(nidas::util::IOException);

    bool testTransmitSE120() throw(nidas::util::IOException);

    void cancelTransmit(const nidas::util::UTime& at)
    	throw(nidas::util::IOException);

    /**
     * Queries the transmitter for status information.
     * Returns a preliminary guess at the model number, which may be wrong.
     * Use detectModel() for a more complete check of the model number.
     */
    int checkStatus() throw(nidas::util::IOException);

    void printStatus() throw();

    void printStatus(std::ostream&) throw();

    void setXmtrId() throw(nidas::util::IOException);

    unsigned long getXmtrId() throw(nidas::util::IOException);

    void setXmtrClock() throw(nidas::util::IOException);

    nidas::util::UTime getXmtrClock() throw(nidas::util::IOException);

    /**
     * Get the transmission delay of the transmitter clock.
     * @param nchar Number of characters sent to set the
     *     clock, or number of characters received when getting
     *     the clock.
     * @return The number of microseconds that the receipt
     *     of a transmitter clock value was delayed due
     *	   to serial transmission. This value is
     *	   typically  14 * 10 * 10^6 / 9600 = 14583.
     * The clock packet is 14 bytes long,
     * assume 10 transmitted bits per byte, at 9600 baud.
     */
    int getXmtrClockDelay(int nchar) const;

    unsigned long checkId() throw(nidas::util::IOException);

    int checkClock() throw(nidas::util::IOException);

    void transmitDataSE110() throw(nidas::util::IOException);

    void reset() throw(nidas::util::IOException);

    void doSelfTest() throw(nidas::util::IOException);

    static std::string codeString(char pktType);

    /**
     * Check response string.
     * If status is not OK, put the transmitter toSleep(),
     * and throw IOException.  toSleep() is not done
     * if an exception is not thrown.
     */
    void checkResponse(char ptype,const std::string& resp)
    	throw(nidas::util::IOException);
    
    /**
     * Check response to SE110 Transmit Data packet.
     * If status is not OK, put the transmitter toSleep(),
     * and throw IOException.  toSleep() is not done
     * if an exception is not thrown.
     */
    void checkACKResponse(char ptype,const std::string& resp,char seqnum)
    	throw(nidas::util::IOException);
    
    /**
     * Byte at beginning of each message sent to or received from 
     * transmitter.
     */
    static const char SOH = '\x1';

    /**
     * Byte at end of each message sent to or received from 
     * transmitter.
     */
    static const char EOT = '\x4';

    static const std::string SOH_STR;

    struct SE_Codes {
        int value;
	const char* msg;
    };

    std::string getSelfTestStatusString();

    /** Get the transmitter's id */
    static const char PKT_GET_ID = '\x10';

    /** Sets Transmitter id in either ram or eeprom */
    static const char PKT_SET_ID = '\x11';

    /** Load Transmitter time-of-day */
    static const char PKT_SET_TIME = '\x13';

    /** Get Transmitter time-of-day */
    static const char PKT_GET_TIME = '\x14';

    /** Transmit data, SE110 only */
    static const char PKT_XMT_DATA = '\x15';

    /** Cancel transmission */
    static const char PKT_CANCEL_XMT = '\x16';

    static const char PKT_GET_XMT_QUE = '\x17';

    static const char PKT_QUERY = '\x18';

    /** SE110 */
    static const char PKT_SET_GLOBALS = '\x1b';

    /** SE110 */
    static const char PKT_GET_GLOBALS = '\x1c';

    /** Display version information, SE120, SE1200 */
    static const char PKT_DISPLAY_VERSION = '\x1d';

    /** Transmit, SE120, SE1200 */
    static const char PKT_XMT_DATA_SE120 = '\x1e';

    static const char PKT_POLL_ENABLE = 0x38;
    static const char PKT_POLL_DATA_GET = 0x39;
    static const char PKT_POLL_DATA_SET = 0x3a;

    /** SE1200 Only, grabs status info. */
    static const char PKT_GET_SE1200_STATUS = '\x74';

    static const char PKT_RESET_XMTR = '\x76';

    static const char PKT_SELFTEST_DISPL = '\x77';

    static const char PKT_SELFTEST_START = '\x78';

    static const char PKT_SEND_FIXED_CHAN = '\x7a';

    /** SE120, SE1200 */
    static const char PKT_SOFTWARE_LOAD = '\x7c';

    /* Acknowledge from xmtr to a xmt data request,
     * load SDI script or polled xmt data req.
     * Indicates prev. pkt was OK.
     */
    static const char PKT_ACKNOWLEDGE = '\x80';

    /** Xmtr detected an error in msg it rcvd */
    static const char PKT_ERR_RESPONSE = '\xf0';

    static struct SE_Codes cmdCodes[];

    /* Signal Engineering Error Response Codes from the xmtr
     * for Type Code F0 (PKT_ERR_RESPONSE).
     */
    static const char PKT_STATUS_OK = 0;

    static const char PKT_STATUS_ILLEGAL_REQUEST = 0x01;

    static const char PKT_STATUS_ITEM_NOT_FOUND = 0x03;

    static const char PKT_STATUS_INVALID_DATE_TIME = 0x04;

    static const char PKT_STATUS_TRANSMIT_OVERLAP = 0x05;

    static const char PKT_STATUS_INVALID_CHANNEL = 0x06;

    static const char PKT_STATUS_INVALID_TRANSMIT_INTERVAL = 0x07;

    static const char PKT_STATUS_EEPROM_NOT_UPDATED = 0x08;

    static const char PKT_STATUS_INVALID_REPEAT_COUNT = 0x09;

    static const char PKT_STATUS_CLOCK_NOT_LOADED = 0x0a;

    static const char PKT_STATUS_CRC_ERROR_ON_LOAD = 0x0b;

    static const char* statusCodeStrings[];

    /* Received Packet Too Long */
    static const char ERR_TOOLONG = 0x01;

    /* Received Packet Too Short */
    static const char ERR_TOOSHORT = 0x02;

    /* Received Packet Checksum Error */
    static const char ERR_CRC = 0x03;

    /* Invalid Type Code */
    static const char ERR_BADTYPE = 0x04;

    /* SE110 only: Length Field mismatch */
    static const char ERR_BADLEN = 0x05;

    /* Unable to Allocate Memory */
    static const char ERR_MEMORY = 0x06;

    /* SE110 only: Invalid Sequence Number */
    static const char ERR_SEQUENCE = 0x07;

    /* Command Timeout for multi-packet Command */
    static const char ERR_TIMEOUT = 0x08;

    static const char* errorCodeStrings[];

    static struct selfTest {
        short mask;
	const char* text;
    } selfTestCodes[2][10];

private:
    
    /**
     * Read a message, up to the EOT, from the serial port.
     */
    size_t read(void* buf, size_t len) throw (nidas::util::IOException);

    /**
     * Write a message to the serial port.  The message must have
     * already been formatted for a SE GOES tranmitter:
     * leading SOH, length, message, CRC, EOT, and the
     * length,message and CRC portion must have been fixed with the fix()
     * method.
     */
    size_t write(const void* buf, size_t len) throw (nidas::util::IOException);

    nidas::util::Logger* logger;

    int model;

    /**
     * Most recent value for GOES clock difference:  GOES clock - system clock.
     */
    int clockDiffMsecs;

    nidas::util::UTime transmitQueueTime;

    nidas::util::UTime transmitAtTime;

    nidas::util::UTime transmitSampleTime;

    std::string lastXmitStatus;

    std::string softwareBuildDate;

    short selfTestStatus;

    int maxRFRate;

    bool gpsNotInstalled;

    size_t xmitNbytes;

    /**
     * Actual id read from transmitter.
     */
    unsigned long activeId;
};


}}}	// namespace nidas namespace dynld namespace isff

#endif
