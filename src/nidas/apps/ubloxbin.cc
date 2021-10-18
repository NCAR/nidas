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

/*
 * Program to set options on a u-blox GPS.
 * Written using docs for a u-blox model M8Q, 
 * but likely will work for most.
 */

#include <nidas/util/time_constants.h>
#include <nidas/util/Logger.h>
#include <nidas/util/InvalidParameterException.h>
#include <nidas/core/NidasApp.h>

using namespace nidas::core;
using namespace nidas::util;

#include <iostream>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/array.hpp>
#include <boost/thread.hpp>

#include "ublox/Message.h"
#include "ublox/frame/UbloxFrame.h"
#include "comms/units.h"
#include "comms/process.h"

using namespace boost::asio;

#if BOOST_VERSION >= 107000
#define GET_IO_SERVICE(s) ((boost::asio::io_context&)(s).get_executor().context())
#else
#define GET_IO_SERVICE(s) ((s).get_io_service())
#endif


enum NMEA_MsgId : std::uint16_t
{
    MsgId_GGA = 0xF000,        // GPS fix data
    MsgId_GLL = 0xF001,        // Lat/long w/time of fix and status
    MsgId_GSA = 0xF002,        // GNSS DOP and active sats
    MsgId_GSV = 0xF003,        // GNSS sats in view
    MsgId_RMC = 0xF004,        // Recommended minimum data
    MsgId_VTG = 0xF005,        // Course over ground and ground speed
    MsgId_GRS = 0xF006,        // GNSS range residuals
    MsgId_GST = 0xF007,        // GNSS pseudo range error stats
    MsgId_ZDA = 0xF008,        // Time and date
    MsgId_GBS = 0xF009,        // GNSS satellite fault detection
    MsgId_DTM = 0xF00A,        // Datum message
    MsgId_GNS = 0xF00D,        // GNSS fix data
    MsgId_VLW = 0xF00F,        // Dual ground/water distance
    // MsgId_GPQ = 0xF040,        // Poll a standard message if talker ID is GP
    // MsgId_TXT = 0xF041,        // Text xmission
    // MsgId_GNQ = 0xF042,        // Poll a standard message if talker ID is GN
    // MsgId_GLQ = 0xF043,        // Poll a standard message if talker ID is GL
    // MsgId_GBQ = 0xF044,        // poll a standard message if talker ID is GB
    MsgId_PBX_POS = 0xF100,    // Lat/long position
    MsgId_PBX_SVSTAT = 0xF103, // Satellite status
    MsgId_PBX_TIME = 0xF104,   // Time of day and clock data
    MsgId_PBX_RATE = 0xF140,   // NMEA output rate
    MsgId_PBX_CFG = 0xF141,    // Set protocols and baudrate
    MsgId_Min = MsgId_GGA,
    MsgId_Max = MsgId_PBX_CFG,
};

NMEA_MsgId All_NMEA_IDs[] = 
{
    MsgId_GBS,
    MsgId_GGA,
    MsgId_GLL,
    // MsgId_GNQ,
    MsgId_GNS,
    MsgId_GRS,
    MsgId_GSA,
    MsgId_GST,
    MsgId_GSV,
    MsgId_RMC,
    // MsgId_TXT,
    MsgId_VLW,
    MsgId_VTG,
    MsgId_ZDA,
    MsgId_DTM,
    // MsgId_GPQ,
    // MsgId_GLQ,
    // MsgId_GBQ,
    MsgId_PBX_POS,
    MsgId_PBX_SVSTAT,
    MsgId_PBX_TIME,
};

ublox::MsgId All_UBX_IDs[] = 
{
    ublox::MsgId_NavPosecef,
    ublox::MsgId_NavPosllh,
    ublox::MsgId_NavStatus,
    ublox::MsgId_NavDop,
    // ublox::MsgId_NavAtt,
    ublox::MsgId_NavSol,
    ublox::MsgId_NavPvt,
    ublox::MsgId_NavOdo,
    // ublox::MsgId_NavResetodo,
    ublox::MsgId_NavVelecef,
    ublox::MsgId_NavVelned,
    // ublox::MsgId_NavHpposecef,
    // ublox::MsgId_NavHpposllh,
    ublox::MsgId_NavTimegps,
    ublox::MsgId_NavTimeutc,
    ublox::MsgId_NavClock,
    ublox::MsgId_NavTimeglo,
    ublox::MsgId_NavTimebds,
    ublox::MsgId_NavTimegal,
    ublox::MsgId_NavTimels,
    ublox::MsgId_NavSvinfo,
    ublox::MsgId_NavDgps,
    ublox::MsgId_NavSbas,
    ublox::MsgId_NavOrb,
    ublox::MsgId_NavSat,
    ublox::MsgId_NavGeofence,
    // ublox::MsgId_NavSvin,
    // ublox::MsgId_NavRelposned,
    // ublox::MsgId_NavEkfstatus,
    ublox::MsgId_NavAopstatus,
    ublox::MsgId_NavEoe,
    // ublox::MsgId_RxmRaw,
    // ublox::MsgId_RxmSfrb,
    ublox::MsgId_RxmSfrbx,
    ublox::MsgId_RxmMeasx,
    ublox::MsgId_RxmRawx,
    ublox::MsgId_RxmSvsi,
    // ublox::MsgId_RxmAlm,
    // ublox::MsgId_RxmEph,
    // ublox::MsgId_RxmRtcm,
    // ublox::MsgId_RxmPmreq,
    ublox::MsgId_RxmRlm,
    ublox::MsgId_RxmImes,
    // ublox::MsgId_InfError,
    // ublox::MsgId_InfWarning,
    // ublox::MsgId_InfNotice,
    // ublox::MsgId_InfTest,
    // ublox::MsgId_InfDebug,
    // ublox::MsgId_AckNak,
    // ublox::MsgId_AckAck,
    // ublox::MsgId_CfgPrt,
    // ublox::MsgId_CfgMsg,
    // ublox::MsgId_CfgInf,
    // ublox::MsgId_CfgRst,
    // ublox::MsgId_CfgDat,
    // ublox::MsgId_CfgTp,
    // ublox::MsgId_CfgRate,
    // ublox::MsgId_CfgCfg,
    // ublox::MsgId_CfgFxn,
    // ublox::MsgId_CfgRxm,
    // ublox::MsgId_CfgEkf,
    // ublox::MsgId_CfgAnt,
    // ublox::MsgId_CfgSbas,
    // ublox::MsgId_CfgNmea,
    // ublox::MsgId_CfgUsb,
    // ublox::MsgId_CfgTmode,
    // ublox::MsgId_CfgOdo,
    // ublox::MsgId_CfgNvs,
    // ublox::MsgId_CfgNavx5,
    // ublox::MsgId_CfgNav5,
    // ublox::MsgId_CfgEsfgwt,
    // ublox::MsgId_CfgTp5,
    // ublox::MsgId_CfgPm,
    // ublox::MsgId_CfgRinv,
    // ublox::MsgId_CfgItfm,
    // ublox::MsgId_CfgPm2,
    // ublox::MsgId_CfgTmode2,
    // ublox::MsgId_CfgGnss,
    // ublox::MsgId_CfgLogfilter,
    // ublox::MsgId_CfgTxslot,
    // ublox::MsgId_CfgPwr,
    // ublox::MsgId_CfgHnr,
    // ublox::MsgId_CfgEsrc,
    // ublox::MsgId_CfgDosc,
    // ublox::MsgId_CfgSmgr,
    // ublox::MsgId_CfgGeofence,
    // ublox::MsgId_CfgDgnss,
    // ublox::MsgId_CfgTmode3,
    // ublox::MsgId_CfgFixseed,
    // ublox::MsgId_CfgDynseed,
    // ublox::MsgId_CfgPms,
    // ublox::MsgId_UpdSos,
    // ublox::MsgId_MonGnss,
    ublox::MsgId_MonIo,
    // ublox::MsgId_MonVer,
    ublox::MsgId_MonHw,
    ublox::MsgId_MonHw2,
    ublox::MsgId_MonIo,
    ublox::MsgId_MonMsgpp,
    // ublox::MsgId_MonPatch,
    ublox::MsgId_MonRxbuf,
    // ublox::MsgId_MonSmgr,
    ublox::MsgId_MonTxbuf,
    ublox::MsgId_MonRxr,
    ublox::MsgId_AidAlm,
    // ublox::MsgId_AidAlp,
    // ublox::MsgId_AidAlpsrv,
    ublox::MsgId_AidAop,
    // ublox::MsgId_AidData,
    // ublox::MsgId_AidEph,
    // ublox::MsgId_AidHui,
    // ublox::MsgId_AidIni,
    // ublox::MsgId_AidReq,
    // ublox::MsgId_TimDosc,
    // ublox::MsgId_TimFchg,
    // ublox::MsgId_TimHoc,
    // ublox::MsgId_TimSmeas,
    ublox::MsgId_TimSvin,
    ublox::MsgId_TimTm2,
    // ublox::MsgId_TimTos,
    ublox::MsgId_TimTp,
    // ublox::MsgId_TimVcocal,
    // ublox::MsgId_TimVrfy,
    // ublox::MsgId_EsfIns,
    // ublox::MsgId_EsfMeas,
    // ublox::MsgId_EsfRaw,
    // ublox::MsgId_EsfStatus,
    // ublox::MsgId_MgaGps,
    // ublox::MsgId_MgaGal,
    // ublox::MsgId_MgaBds,
    // ublox::MsgId_MgaQzss,
    // ublox::MsgId_MgaGlo,
    // ublox::MsgId_MgaAno,
    // ublox::MsgId_MgaFlash,
    // ublox::MsgId_MgaIni,
    // ublox::MsgId_MgaAck,
    // ublox::MsgId_MgaDbd,
    // ublox::MsgId_LogErase,
    // ublox::MsgId_LogString,
    // ublox::MsgId_LogCreate,
    // ublox::MsgId_LogInfo,
    // ublox::MsgId_LogRetrieve,
    // ublox::MsgId_LogRetrievepos,
    // ublox::MsgId_LogRetrievestring,
    // ublox::MsgId_LogFindtime,
    // ublox::MsgId_LogRetrieveposextra,
    // ublox::MsgId_SecSign,
    // ublox::MsgId_SecUniqid,
    // ublox::MsgId_HnrPvt,
};

static bool spyOnCapturedInput = false;

class Session 
{
public:
    using InMessage =
        ublox::Message<
            comms::option::ReadIterator<const std::uint8_t*>,
            comms::option::Handler<Session> // Dispatch to this object
        >;

    using OutBuffer = std::vector<std::uint8_t>;
    using OutMessage =
        ublox::Message<
            comms::option::IdInfoInterface,
            comms::option::WriteIterator<std::back_insert_iterator<OutBuffer> >,
            comms::option::LengthInfoInterface
        >;

    using InNavPvt = ublox::message::NavPvt<InMessage>;
    using InNavTimegps = ublox::message::NavTimegps<InMessage>;
    using InNavSol = ublox::message::NavSol<InMessage>;
    using InNavDop = ublox::message::NavDop<InMessage>;
    using InAckAck = ublox::message::AckAck<InMessage>;
    using InAckNak = ublox::message::AckNak<InMessage>;
    using InTimTp = ublox::message::TimTp<InMessage>;
    using MsgId = ublox::MsgId;

    // MsgId a = MsgId::MsgId_TimTp;

    using AllHandledInMessages =
        std::tuple<
            InNavDop,
            InNavSol,
            InNavPvt,
            InNavTimegps,
            InAckNak,
            InAckAck,
            InTimTp
        >;

    using Frame = ublox::frame::UbloxFrame<InMessage, AllHandledInMessages>;

    using SerialPort = boost::asio::serial_port;
    using IoService = boost::asio::io_service;

    Session(boost::asio::io_service& io, const std::string& dev)
      : m_serial(io), m_device(dev), m_inputBuf(), m_inData(), 
        m_frame(), m_waitingAckMsg(false), m_sentMsgId((MsgId)0), m_msgAcked(false),
        m_enabledMsgs(), m_checkEnabledMsgs(false), m_ackCheck(false), 
        m_detectedBaudRate(0), m_timeIsResolved(false), m_coldBootDetected(false)
    {}

    ~Session()
    {
        if (!GET_IO_SERVICE(m_serial).stopped()) {
            GET_IO_SERVICE(m_serial).stop();
        }

        if (m_serial.is_open()) {
            m_serial.close();
        }
    }

    bool getCheckEnabledMsgs() {return m_checkEnabledMsgs;}
    void setCheckEnabledMsgs(const bool check = true) {m_checkEnabledMsgs = check;}
    bool getAckCheck() {return m_ackCheck;}
    void setAckCheck(const bool check = true) {m_ackCheck = check;}
    bool timeIsResolved() {return m_timeIsResolved;}
    void setTimeIsResolved(const bool resolved=true) {m_timeIsResolved = resolved;}
    bool coldBootDetected() {return m_coldBootDetected;}
    void setColdBootDetected(const bool coldBooting=true) {m_coldBootDetected = coldBooting;}

    bool start()
    {
        boost::system::error_code ec;
        m_serial.open(m_device, ec);
        if (ec) {
            std::cerr << "ERROR: Failed to open " << m_device << std::endl;
            return false;
        }

        m_serial.set_option(SerialPort::baud_rate(9600));
        m_serial.set_option(SerialPort::character_size(8));
        m_serial.set_option(SerialPort::parity(SerialPort::parity::none));
        m_serial.set_option(SerialPort::stop_bits(SerialPort::stop_bits::one));
        m_serial.set_option(SerialPort::flow_control(SerialPort::flow_control::none));

 #if BOOST_ASIO_VERSION < 101200 && defined(__linux__)
        // Workaround to set some options for the port manually. This is done in
        // Boost.ASIO, but until v1.12.0 (Boost 1.66) there was a bug which doesn't enable relevant
        // code. Fixed by commit: https://github.com/boostorg/asio/commit/619cea4356
        {
                DLOG(("Session::start(): current BOOST version doesn't do low level serial port setup."));
                int fd = m_serial.native_handle();
 
                termios tio;
                tcgetattr(fd, &tio);
 
                // Set serial port to "raw" mode to prevent EOF exit.
                cfmakeraw(&tio);
 
                // Commit settings
                tcsetattr(fd, TCSANOW, &tio);

                // also flush
                tcflush(fd, TCIOFLUSH);
        }
 #endif
        return true;
    }

    void handle(InNavPvt& msg)
    {
        int year = msg.field_year().value();
        int month = msg.field_month().value();
        int day = msg.field_day().value();
        int hours = msg.field_hour().value();
        int minutes = msg.field_min().value();
        int seconds = msg.field_sec().value();
        char dateBuf[50];
        memset(dateBuf, 0, 50);
        sprintf(dateBuf, "%d%02d%02d - %d:%02d:%02d", year, month, day, hours, minutes, seconds);
        std::string date(dateBuf, strlen(dateBuf));

        std::string fix(ublox::field::GpsFixCommon::valueName(msg.field_fixType().value()));
        auto& validField = msg.field_valid(); //asBitmask();
        using OutValidFieldMask = typename std::decay<decltype(validField)>::type;
        bool validDate = validField.getBitValue(OutValidFieldMask::BitIdx_validDate);
        bool validTime = validField.getBitValue(OutValidFieldMask::BitIdx_validTime);
        setTimeIsResolved(validField.getBitValue(OutValidFieldMask::BitIdx_fullyResolved));

        DLOG(("NAV-PVT: fix=") << fix 
            << "; lat=" << comms::units::getDegrees<double>(msg.field_lat()) 
            << "; lon=" << comms::units::getDegrees<double>(msg.field_lon()) 
            << "; alt=" << comms::units::getMeters<double>(msg.field_height()) 
            << "; UTC date: " << date 
            << (validDate ? " valid" : " invalid") << " date" 
            << (validTime ? " valid" : " invalid") << " time" 
            << (timeIsResolved() ? " fully resolved" : " not fully resolved") << " time");

        std::string msgName(msg.doName());
        if (getCheckEnabledMsgs() && msgEnabled(msgName)) {
            if (!timeIsResolved() && m_enabledMsgs[msgName] == 0) {
                setColdBootDetected();
                DLOG(("Cold boot detected..."));
            }
            m_enabledMsgs[msgName]++;
        }
    }

    void handle(InNavSol& msg)
    {
        DLOG(("Session::handle(InNavSol&): Caught NavSol message..."));

        std::string fix(ublox::field::GpsFixCommon::valueName(msg.field_gpsFix().value()));
        if (msg.field_flags().getBitValue_GPSfixOK()) {
            DLOG(("NAV-SOL: fix=") << fix 
                << "; towValid=" << (msg.field_flags().getBitValue_TOWSET() ? "Y" : "N")
                << "; tow=" << (comms::units::getMilliseconds<uint32_t>(msg.field_itow()) 
                               + comms::units::getNanoseconds<int32_t>(msg.field_ftow()))
                << "; wkValid=" << (msg.field_flags().getBitValue_WKNSET() ? "Y" : "N")
                << "; wk=" << comms::units::getWeeks<int16_t>(msg.field_week())
                << "; x=" << comms::units::getCentimeters<int32_t>(msg.field_ecefX()) 
                << "; y=" << comms::units::getCentimeters<int32_t>(msg.field_ecefY()) 
                << "; z=" << comms::units::getCentimeters<int32_t>(msg.field_ecefZ()) 
                << "; pAcc=" << comms::units::getCentimeters<uint32_t>(msg.field_pAcc()) 
                << "; pDop=" << msg.field_pDOP().value()/100.0
                << "; xVel=" << comms::units::getCentimetersPerSecond<int32_t>(msg.field_ecefVX()) 
                << "; yVel=" << comms::units::getCentimetersPerSecond<int32_t>(msg.field_ecefVY()) 
                << "; zVel=" << comms::units::getCentimetersPerSecond<int32_t>(msg.field_ecefVZ()) 
                << "; sAcc=" << comms::units::getCentimetersPerSecond<uint32_t>(msg.field_sAcc())
                << "; nSats=" << msg.field_numSV().value()); 
        }
        else {
            DLOG(("NAV-SOL: No GPS fix"));
        }

        std::string msgName(msg.doName());
        if (getCheckEnabledMsgs() && msgEnabled(msgName)) {
            m_enabledMsgs[msgName]++;
        }

        DLOG(("Session::handle(InNavSol&): NavSol message handled..."));
    }

    void handle(InNavDop& msg)
    {
        DLOG(("Session::handle(InNavDop&): Caught NavDop message..."));

        // auto& refInfo = msg.field_refInfo().value();


        std::string msgName(msg.doName());
        if (getCheckEnabledMsgs() && msgEnabled(msgName)) {
            m_enabledMsgs[msgName]++;
        }
    }

    void handle(InNavTimegps& msg)
    {
        DLOG(("Session::handle(InNavTimegps&): Caught NavTimegps message..."));

        // auto& refInfo = msg.field_refInfo().value();


        std::string msgName(msg.doName());
        if (getCheckEnabledMsgs() && msgEnabled(msgName)) {
            m_enabledMsgs[msgName]++;
        }
    }

    void handle(InTimTp& msg)
    {
        DLOG(("Session::handle(InTimTp&): Caught TimTp message..."));
        
        using RaimVal = ublox::message::TimTpFieldsCommon::FlagsMembersCommon::RaimVal;
        RaimVal raim = msg.field_flags().field_raim().value();

        DLOG(("TIM-TP: ")
            << "wk: " << comms::units::getWeeks<uint16_t>(msg.field_week())
            << "; towMS: " << comms::units::getMilliseconds<uint32_t>(msg.field_towMS())
            << "; towNS: " << comms::units::getNanoseconds<uint32_t>(msg.field_towSubMS())
            << "; qErr: " << msg.field_qErr().value() << " pS"
            << "; isUTC: " << (msg.field_flags().field_bitsLow().getBitValue_utc() ? "Y" : "N")
            << "; timebase: " << (msg.field_flags().field_bitsLow().getBitValue_timeBase() ? "UTC" : "GNSS")
            << "; raim: " << (raim == RaimVal::NotAvailable ? "no info" 
                              : (raim < RaimVal::Active ? "not active" : "active")));

        std::string msgName(msg.doName());
        if (getCheckEnabledMsgs() && msgEnabled(msgName)) {
            m_enabledMsgs[msgName]++;
        }
    }

    void handle(InAckAck& msg)
    {
        int ackMsgId = msg.field_msgId().value();
        char buf[32];
        memset(buf, 0, 32);
        sprintf(buf, "0x%04X", ackMsgId);
        std::string ackMsgIdStr(buf, strlen(buf));
        if (getAckCheck()) {
            if (ackMsgId == m_sentMsgId) {
                m_waitingAckMsg = false;
                m_msgAcked = true;
                m_sentMsgId = ublox::MsgId_AckAck;
            }
        }
        VLOG(("AckAck caught for ") << ackMsgIdStr);
    }

    void handle(InAckNak& msg)
    {
        int nakMsgId = msg.field_msgId().value();
        char buf[32];
        memset(buf, 0, 32);
        sprintf(buf, "0x%04X", nakMsgId);
        std::string nakMsgIdStr(buf, strlen(buf));
        VLOG(("AckNak caught for ") << nakMsgIdStr);
        if (getAckCheck()) {
            if (nakMsgId == m_sentMsgId) {
                m_waitingAckMsg = false;
                m_sentMsgId = ublox::MsgId_AckNak;
            }
        }
    }

    void handle(InMessage& /*msg*/)
    {
        // ignore all other incoming
        DLOG(("Session::handle(\"InMessage&\"): caught unhandled message"));
    }

    void performRead()
    {
        m_serial.async_read_some(
            boost::asio::buffer(m_inputBuf),
            [this](const boost::system::error_code& ec, std::size_t bytesCount)
            {
                switch (ec.value()) {
                    case boost::asio::error::operation_aborted:
                        VLOG(("Session::performRead(): aborted"));
                        return;
                        break;
                    case boost::asio::error::eof:
                        VLOG(("Session::performRead(): eof - should never happen..."));
                        return;
                        break;
                }

                if (bytesCount) {
                    VLOG(("Session::performRead(): caught ") << bytesCount << " bytes...");

                    if (spyOnCapturedInput) {
                        VLOG(("Session::performRead(): spying on captured data..."));
                        std::string hexStr;
                        auto inputIter = m_inputBuf.begin();
                        auto inputEndIter = inputIter + bytesCount;
                        while (inputIter++ != inputEndIter) {
                            char hexchar[10] = {0,0,0,0,0,0,0,0,0,0};
                            snprintf(hexchar, 9, "0x%02X ", *inputIter);
                            hexStr.append(hexchar, 5);
                        }
                        VLOG(("Session::performRead(): input buf: ") << hexStr);
                    }

                    auto dataBegIter = m_inputBuf.begin();
                    auto dataEndIter = dataBegIter + bytesCount;
                    m_inData.insert(m_inData.end(), dataBegIter, dataEndIter);

                    processInputData();
                    performRead();
                }
            });
    }   

    bool findBaudRate()
    {
        ILOG(("Session::findBaudRate(): find baud rate by trying to disable a message, and check for ACK"));
        using OutCfgMsgCurrent = ublox::message::CfgMsgCurrent<OutMessage>;
        ILOG(("Session::findBaudRate(): checking for response @ 9600 baud"));
        OutCfgMsgCurrent msg;
        msg.field_msgId().value() = ublox::MsgId_NavPvt;
        msg.field_rate().value() = 0;
        sendMessage(msg);
        if (m_waitingAckMsg || !m_msgAcked) {
            ILOG(("Session::findBaudRate(): No response @ 9600 baud, checking for response @ 115200 baud"));
            m_serial.set_option(SerialPort::baud_rate(115200));
            sendMessage(msg);
            if (m_waitingAckMsg || !m_msgAcked) {
                ILOG(("Session::findBaudRate(): No response @ 115200 baud - failed"));
                return false;
            }
            else {
                ILOG(("Session::findBaudRate(): Response @ 115200 baud - success!"));
                m_detectedBaudRate = 115200;
            }
        }
        else {
            ILOG(("Session::findBaudRate(): Response @ 9600 baud - success!"));
            m_detectedBaudRate = 9600;
        }
        return true;
    }

    bool configureUbx()
    {
        bool success = false;
        
        if (!configureUbxProtocol(true)) {// use the detected baud rate
            ILOG(("Session::configureUbx() - Could not detect baud rate " 
                  "and/or configure UBX Protocol"));
        }
        // spyOnCapturedInput = true;

        else if (!disableAllMessages())  {
            ILOG(("Session::configureUbx() - Could not disable all messages"));
        }
        else if(!configGnss()) {
            ILOG(("Session::configureUbx() - Could not configure UBX satellites used"));
        }
        else if (!configureUbxPowerMode())  {
            ILOG(("Session::configureUbx() - Could not configure UBX power mode"));
        }
        else if (!configureUbxRTCUpdate())  {
            ILOG(("Session::configureUbx() - Could not configure UBX RTC update"));
        }

        // spyOnCapturedInput = false;

        else if (!configureUbxNavMode())  {
            ILOG(("Session::configureUbx() - Could not configure UBX NAV mode"));
        }
        else if (!configurePPS())  {
            ILOG(("Session::configureUbx() - Could not configure PPS"));
        }
        else if (!enableDefaultMessages())  {
            ILOG(("Session::configureUbx() - Could not enable default messages"));
        }
        else if (!configureUbxProtocol(false))  {
            ILOG(("Session::configureUbx() - Could not force switch to 115200 baud"));
        }
        else {
            success = true;
        }

        return success;
    }

    bool msgEnabled(const std::string& name) const
    {
        return (m_enabledMsgs.find(name) != m_enabledMsgs.end());
    }

    int msgCount(const std::string& name) {
        int count = 0;
        // ILOG(("Testing message count for: ") << name);
        if (msgEnabled(name)) {
            count = m_enabledMsgs.find(name)->second;
        }

        return count;
    }

    bool testEnabledMsgs(const int numRequiredMsgs=1)
    {
        if (getCheckEnabledMsgs()) {
            DLOG(("Session::testEnabledMsgs(): enabled"));
            bool done = true;
            using MsgItem = std::pair<std::string, int>;
            for (MsgItem msg : m_enabledMsgs) {
                ILOG(("Session::testEnabledMsgs(): msg: ") << msg.first << " - received: " << msg.second);
                done &= (msg.second >= numRequiredMsgs);
            }

            ILOG(("Session::testEnabledMsgs(): ") << (done ? "done" : "not done." ));
            return done;
        }
        else {
            DLOG(("Session::testEnabledMsgs(): disabled"));
            return false;
        }
    }

    void printEnabledMsgs()
    {
        std::cout << std::endl 
                  << "Enabled Messages" << std::endl 
                  << "================" << std::endl;
        using MsgItem = std::pair<std::string, int>;
        for (MsgItem enabledMsg : m_enabledMsgs) {
            std::cout << enabledMsg.first << " - received: " << enabledMsg.second << std::endl;
        }
        std::cout << std::endl;
    }

    bool resetUBloxConfig()
    {
        using OutCfgCfgMsg = ublox::message::CfgCfg<OutMessage>;
        OutCfgCfgMsg msg;

        // clear all settings, and set to defaults
        auto& fieldClrMask = msg.field_clearMask().value();
        fieldClrMask = 0x1F1F;

        // in battery backed RAM, because that's all there is...
        auto& fieldDeviceMask = msg.field_deviceMask().value();
        fieldDeviceMask.setBitValue_devBBR(1);

        bool retval = sendMessage(msg);

        if (!retval) {
            ILOG(("resetUBloxConfig(): failed to reset config in battery backed ram"));
        }

        return retval;
    }

private:

    void processInputData()
    {
        if (!m_inData.empty()) {
            auto consumed = comms::processAllWithDispatch(&m_inData[0], m_inData.size(), m_frame, *this);
            m_inData.erase(m_inData.begin(), m_inData.begin() + consumed);
        }    
    }

    void waitForResponse()
    {
        boost::asio::io_service readSvc;
        boost::asio::steady_timer readClock(readSvc);
        std::chrono::microseconds expireTime(USECS_PER_SEC/100);

        for(int i=0; getAckCheck() && i<100 && m_waitingAckMsg; ++i) {
            readClock.expires_from_now(expireTime);
            readClock.wait();
            performRead();
        }
    }

    bool sendMessage(const OutMessage& msg)
    {
        ublox::MsgId msgId = msg.getId();
        OutBuffer buf;
        buf.reserve(m_frame.length(msg)); // Reserve enough space
        auto iter = std::back_inserter(buf);
        for (int i=0; i<3; ++i) {
            auto es = m_frame.write(msg, iter, buf.max_size());
            if (es == comms::ErrorStatus::UpdateRequired) {
                auto* updateIter = &buf[0];
                es = m_frame.update(updateIter, buf.size());
            }
            static_cast<void>(es);
            assert(es == comms::ErrorStatus::Success); // do not expect any error

            if (getAckCheck()) {
                ackMsgInit(msg.getId());
            }

            // set the ID of the message to be ack/nak'd
            while (!buf.empty()) {
                boost::system::error_code ec;
                auto count = m_serial.write_some(boost::asio::buffer(buf), ec);

                if (ec) {
                    std::cerr << "ERROR: write failed with message: " << ec.message() << std::endl;
                    GET_IO_SERVICE(m_serial).stop();
                    return false;
                }

                buf.erase(buf.begin(), buf.begin() + count);
            }

            // always read even if just to empty the buffer...
            waitForResponse();

            if (getAckCheck()) {
                char buf[32];
                memset(buf, 0, 32);
                sprintf(buf, "0x%04X", msgId);
                std::string msgIdStr(buf, strlen(buf));
                if (m_waitingAckMsg) {
                    ackMsgInit(msg.getId());
                    ILOG(("Failed to receive any ACK/NAK message for try ") << i << " for msgId " << msgIdStr);
                    continue;
                }
                else if (!m_msgAcked) {
                    ILOG(("Received a NAK for try ") << i << " of " << msgIdStr);
                    continue;
                }
                else {
                    ILOG(("Received an ACK for try ") << i << " of " << msgIdStr);
                    break;
                }
            }
            else {
                break;
            }
        }

        if (getAckCheck() && (m_waitingAckMsg || !m_msgAcked)) {
            ILOG(("Error: Failed to receive ACK message after three tries..."));
            return false;
        }

        return true;
    }

    void ackMsgInit(MsgId msgId)
    {
        m_waitingAckMsg = true;
        m_sentMsgId = msgId;
        m_msgAcked = false;
    }

    bool disableAllMessages()
    {
        DLOG(("disableAllMessages:"));

        using OutCfgMsgCurrent = ublox::message::CfgMsgCurrent<OutMessage>;
        OutCfgMsgCurrent msg;
        msg.field_rate().value() = 0;

        // Ignore the NMEA messages since they get taken care of in
        // the protocol specification
        //
        // // iterate over a list of all NMEA message IDs
        // DLOG((" disabling NMEA Messages..."));
        // for (NMEA_MsgId msgId : All_NMEA_IDs) {
        //     char buf[32];
        //     memset(buf, 0, 32);
        //     sprintf(buf, "0x%04X", msgId);
        //     std::string msgIdStr(buf, strlen(buf));
        //     DLOG(("Disabling NMEA message ID: ") << msgIdStr);
        //     msg.field_msgId().value() = static_cast<ublox::MsgId>(msgId);
        //     sendMessage(msg);
        // }

        // iterate over a list of all UBX message IDs
        DLOG((" disabling UBX Messages..."));
        bool allDisabled = false;
        for (ublox::MsgId msgId : All_UBX_IDs) {
            char buf[32];
            memset(buf, 0, 32);
            sprintf(buf, "0x%04X", msgId);
            std::string msgIdStr(buf, strlen(buf));
            std::string msgNameStr;
            ILOG(("Session::disableAllMessages(): Disabling UBX message ID: ") << msgIdStr);
            msg.field_msgId().value() = msgId;
            allDisabled = sendMessage(msg);
            if (!allDisabled) {
                break;
            }
        }

        return allDisabled;
    }

    bool configureUbxProtocol(bool useDetected)
    {
        DLOG(("Session::configureUbxProtocol(): Configuring In/Out Protocol to UBX only for UART..."));
        using OutCfgPrtUart = ublox::message::CfgPrtUart<OutMessage>;

        OutCfgPrtUart msg;
        auto& outProtoMaskField = msg.field_outProtoMask();
        DLOG(("Session::configureUbxProtocol(): default outProtoMaskField: ") << outProtoMaskField.value());

        using OutProtoMaskField = typename std::decay<decltype(outProtoMaskField)>::type;
        outProtoMaskField.setBitValue(OutProtoMaskField::BitIdx_outUbx, 1);
        outProtoMaskField.setBitValue(OutProtoMaskField::BitIdx_outNmea, 0);
        outProtoMaskField.setBitValue(OutProtoMaskField::BitIdx_outRtcm3, 0);

        DLOG(("Session::configureUbxProtocol(): commanded outProtoMaskField: ") << outProtoMaskField.value());

        auto& inProtoMaskField = msg.field_inProtoMask();
        DLOG(("Session::configureUbxProtocol(): default inProtoMaskField: ") << inProtoMaskField.value());

        using InProtoMaskField = typename std::decay<decltype(inProtoMaskField)>::type;
        inProtoMaskField.setBitValue(InProtoMaskField::BitIdx_inUbx, 1);
        inProtoMaskField.setBitValue(InProtoMaskField::BitIdx_inNmea, 0);
        inProtoMaskField.setBitValue(InProtoMaskField::BitIdx_inRtcm, 0);
        inProtoMaskField.setBitValue(InProtoMaskField::BitIdx_inRtcm3, 0);

        DLOG(("Session::configureUbxProtocol(): commanded inProtoMaskField: ") << inProtoMaskField.value());

        auto& baud = msg.field_baudRate().value();
        ILOG(("Session::configureUbxProtocol(): default baud: ") << baud);
        if (useDetected) {
            baud = m_detectedBaudRate;
        }
        else {
            baud = 115200;
        }
        ILOG(("Session::configureUbxProtocol(): commanded baud: ") << baud);

        auto& txReadyField = msg.field_txReady().value();
        auto& txReadyEnabled = std::get<0>(txReadyField);
        DLOG(("Session::configureUbxProtocol(): default txReady enabled: ") 
              << std::string(txReadyEnabled.getBitValue_en() ? "ENABLED" : "DISABLED"));
        txReadyEnabled.setBitValue_en(0);
        DLOG(("Session::configureUbxProtocol(): commanded txReady enabled: ") 
              << std::string(txReadyEnabled.getBitValue_en() ? "ENABLED" : "DISABLED"));

        return sendMessage(msg);
    }

    bool configureUbxRTCUpdate()
    {
        ILOG(("Session::configureUbxRTCUpdate(): Configuring UBX to update RTC and ephemeris data occasionally..."));
        ILOG(("Session::configureUbxRTCUpdate(): Configuring UBX tracking state machine to cyclic, w/no off period on fail..."));
        using OutCfgPm2 = ublox::message::CfgPm2<OutMessage>;
        OutCfgPm2 msg;

        DLOG(("Session::configureUbxRTCUpdate(): default message version: ") << (int)msg.field_version().value());
        msg.field_version().value() = 1;
        DLOG(("Session::configureUbxRTCUpdate(): commanded message version: ") << (int)msg.field_version().value());

        DLOG(("Session::configureUbxRTCUpdate(): default mode: ") << (int)msg.field_flags().field_mode().value());
        msg.field_flags().field_mode().value() = ublox::field::CfgPm2FlagsMembersCommon::ModeVal::Cyclic;
        DLOG(("Session::configureUbxRTCUpdate(): commanded mode: ") << (int)msg.field_flags().field_mode().value());

        DLOG(("Session::configureUbxRTCUpdate(): default max startup duration: ") << (int)msg.field_maxStartupStateDur().value());
        msg.field_maxStartupStateDur().value() = 0; // ublox figures it out
        DLOG(("Session::configureUbxRTCUpdate(): commanded max startup duration: ") << (int)msg.field_maxStartupStateDur().value());
        

        DLOG(("Session::configureUbxRTCUpdate(): default update period: ") << msg.field_updatePeriod().value());
        msg.field_updatePeriod().value() = 1000; // update nav 1000 mS = 1/sec
        DLOG(("Session::configureUbxRTCUpdate(): commanded update period: ") << msg.field_updatePeriod().value());

        DLOG(("Session::configureUbxRTCUpdate(): default search period: ") << msg.field_searchPeriod().value());
        msg.field_searchPeriod().value() = 1000; // mSecs on = 1 seconds
        DLOG(("Session::configureUbxRTCUpdate(): commanded search period: ") << msg.field_searchPeriod().value());

        DLOG(("Session::configureUbxRTCUpdate(): default grid offset: ") << msg.field_gridOffset().value());
        DLOG(("Session::configureUbxRTCUpdate(): default min acq timeout: ") << msg.field_minAcqTime().value());
        DLOG(("Session::configureUbxRTCUpdate(): default on time: ") << msg.field_onTime().value());
        
        auto& outPM2LowFlags = msg.field_flags().field_bitsLow();
        DLOG(("Session::configureUbxRTCUpdate(): default low bits: ") << outPM2LowFlags.value());
        outPM2LowFlags.setBitValue_extintBackup(0);
        outPM2LowFlags.setBitValue_extintSel(0);
        outPM2LowFlags.setBitValue_extintWake(0);
        DLOG(("Session::configureUbxRTCUpdate(): commanded low bits: ") << outPM2LowFlags.value());

        auto& outPM2MidFlags = msg.field_flags().field_bitsMid();
        DLOG(("Session::configureUbxRTCUpdate(): default mid bits: ") << outPM2MidFlags.value());
        outPM2MidFlags.setBitValue_updateRTC(1);
        outPM2MidFlags.setBitValue_updateEPH(1);
        outPM2MidFlags.setBitValue_waitTimeFix(0);
        outPM2MidFlags.setBitValue_doNotEnterOff(1);
        DLOG(("Session::configureUbxRTCUpdate(): commanded mid bits: ") << outPM2MidFlags.value());

        return sendMessage(msg);
    }

    bool configureUbxPowerMode()
    {
        ILOG(("Session::enableDefaultMessages(): Configuring UBX Power Mode..."));
        using OutCfgRxm = ublox::message::CfgRxm<OutMessage>;

        OutCfgRxm msg;
        auto& outLpMode = msg.field_lpMode().value();

        using OutLpModeValType = typename std::decay<decltype(outLpMode)>::type;
        outLpMode = OutLpModeValType::Continuous;

        return sendMessage(msg);
    }

    bool configureUbxNavMode()
    {
        ILOG(("Session::enableDefaultMessages(): Configuring UBX NAV Mode..."));
        using OutCfgNav5 = ublox::message::CfgNav5<OutMessage>;
        OutCfgNav5 msg;
        auto& outDynamicModel = msg.field_dynModel().value();

        using OutDynamicModelType = typename std::decay<decltype(outDynamicModel)>::type;
        // using OutDynamicModelType = typename ublox::message::CfgNav5FieldsCommon::DynModelVal;
        outDynamicModel = OutDynamicModelType::Portable;

        auto& utcStd = msg.field_utcStandard().value();
        using UtcStdType = typename std::decay<decltype(utcStd)>::type;
        utcStd = UtcStdType::GPS;

        return sendMessage(msg);
    }

    bool configGnss()
    {
        ILOG(("Session::configGnss(): Set up which GNSS sat systems are in use, and reserve channels for them."));
        using OutCfgGnss = ublox::message::CfgGnss<OutMessage>;
        OutCfgGnss cfgGnssMsg;

        cfgGnssMsg.field_msgVer().value() = 0;
        cfgGnssMsg.field_numTrkChUse().value() = 0xFF;
        
        cfgGnssMsg.field_numConfigBlocks().value() = 3;
        auto& gnssCfgBlocks = cfgGnssMsg.field_list().value();
        gnssCfgBlocks.resize(cfgGnssMsg.field_numConfigBlocks().value());

        auto& cfgBlock0 = gnssCfgBlocks[0];
        cfgBlock0.field_gnssId().value() = ublox::field::GnssIdVal::GPS;
        auto& enable = cfgBlock0.field_flags().field_bitsLow();
        enable.setBitValue_enable(1);
        auto& sigCfg = cfgBlock0.field_flags().field_sigCfgMask().value();
        sigCfg = 0x01;
        cfgBlock0.field_maxTrkCh().value() = 16;
        cfgBlock0.field_resTrkCh().value() = 8;

        auto& cfgBlock1 = gnssCfgBlocks[1];
        cfgBlock1.field_gnssId().value() = ublox::field::GnssIdVal::QZSS;
        auto& enable1 = cfgBlock1.field_flags().field_bitsLow();
        enable1.setBitValue_enable(1);
        auto& sigCfg1 = cfgBlock1.field_flags().field_sigCfgMask().value();
        sigCfg1 = 0x01;
        cfgBlock1.field_maxTrkCh().value() = 3;
        cfgBlock1.field_resTrkCh().value() = 0;

        auto& cfgBlock2 = gnssCfgBlocks[2];
        cfgBlock2.field_gnssId().value() = ublox::field::GnssIdVal::SBAS;
        auto& enable2 = cfgBlock2.field_flags().field_bitsLow();
        enable2.setBitValue_enable(0);
        auto& sigCfg2 = cfgBlock2.field_flags().field_sigCfgMask().value();
        sigCfg2 = 0x01;
        cfgBlock2.field_maxTrkCh().value() = 4;
        cfgBlock2.field_resTrkCh().value() = 0;

        return sendMessage(cfgGnssMsg);
    }

    bool configurePPS()
    {
        bool ppsConfigured = false;

        // set up the measurement rate
        ILOG(("Session::configurePPS(): Configuring UBX Measurement Rate..."));
        using OutCfgRate = ublox::message::CfgRate<OutMessage>;
        OutCfgRate cfgRatemsg;
        auto& measRate = cfgRatemsg.field_measRate().value();
        measRate = 1000; // 1000 mS = 1S ==> time between GNSS measurements
        auto& navRate = cfgRatemsg.field_navRate().value();
        navRate = 1; // 1 nav measurement per nav solution
        auto& timeRef = cfgRatemsg.field_timeRef().value();
        timeRef = ublox::message::CfgRateFieldsCommon::TimeRefVal::UTC;

        ppsConfigured = sendMessage(cfgRatemsg);
        if (!ppsConfigured) {
            ILOG(("Session::configurePPS(): Failed to configure UBX Measurement Rate"));
            return ppsConfigured;
        }

        // then set up the PPS signal output
        ILOG(("Session::configurePPS(): Configuring UBX PPS Output..."));
        using OutCfgTp5 = ublox::message::CfgTp5<OutMessage>;
        OutCfgTp5 cfgTp5Msg;

        auto& version = cfgTp5Msg.field_version().value();
        version = 0;

        auto& tpIdx = cfgTp5Msg.field_tpIdx().value();
        tpIdx = ublox::field::CfgTp5TpIdxVal::TIMEPULSE;

        auto& bits = cfgTp5Msg.field_flags().field_bits();
        bits.setBitValue_active(1);
        bits.setBitValue_lockGnssFreq(1);   // really true?
        bits.setBitValue_isFreq(0);
        bits.setBitValue_isLength(1);
        bits.setBitValue_alignToTow(1);
        bits.setBitValue_polarity(1);       // rising edge @ top of second
        bits.setBitValue_lockedOtherSet(0);
    
        auto& timeGrid = cfgTp5Msg.field_flags().field_gridUtcGnss().value();
        timeGrid = ublox::message::CfgTp5FieldsCommon::FlagsMembersCommon::GridUtcGnssVal::UTC;

        cfgTp5Msg.field_period().value().value() = 1000000; // 1 Sec == 1000000 uSec
        // cfgTp5Msg.field_periodLock().value().value() = 1;   // Hz
        cfgTp5Msg.field_ratio().value().value() = 100000;   // 100 mSec = 100000 uSec
        // cfgTp5Msg.field_ratioLock().value().value() = 10;       // 10% duty
        // cfgTp5Msg.field_pulseLen().value().value() = 10;    // 10% duty
        // cfgTp5Msg.field_pulseLenLock().value().value() = 10;// 10% duty
        cfgTp5Msg.field_antCableDelay().value() = 50;       // nsec
        cfgTp5Msg.field_userConfigDelay().value() = 0;
        // cfgTp5Msg.field_freq().value().value() = 0;
        // cfgTp5Msg.field_freqLock().value().value() = 0;

        ppsConfigured &= sendMessage(cfgTp5Msg);
        if (!ppsConfigured) {
            ILOG(("Session::configurePPS(): Failed to configure UBX PPS Output."));
        }
    
        return (ppsConfigured);
    }

    bool enableDefaultMessages()
    {
        ILOG(("Session::enableDefaultMessages(): Enabling NAV PVT Message..."));
        using OutCfgMsg = ublox::message::CfgMsg<OutMessage>;
        OutCfgMsg msg;
        
        msg.field_msgId().value() = ublox::MsgId_NavPvt;

        // only enable it for uart1
        auto& ifaceRateVector = msg.field_rates().value();
        ifaceRateVector.resize((unsigned)ublox::field::CfgPrtPortIdVal::UART + 1);
        ifaceRateVector[(unsigned)ublox::field::CfgPrtPortIdVal::UART].value() = 1; // 1/nav solution delivered
        
        bool defaultMsgsEnabled = sendMessage(msg);

        if (defaultMsgsEnabled) {
            std::pair<std::string, int> NavPvt(InNavPvt().doName(), 0);
            m_enabledMsgs.insert(NavPvt);

            ILOG(("Session::enableDefaultMessages(): Enabling TIM TP Message..."));
            msg.field_msgId().value() = ublox::MsgId_TimTp;
        }

        defaultMsgsEnabled &= sendMessage(msg);

        if (defaultMsgsEnabled) {
            std::pair<std::string, int> TimTp(InTimTp().doName(), 0);
            m_enabledMsgs.insert(TimTp);

            ILOG(("Session::enableDefaultMessages(): Enabling NAV SOL Message..."));
            msg.field_msgId().value() = ublox::MsgId_NavSol;
        }

        defaultMsgsEnabled &= sendMessage(msg);

        if (defaultMsgsEnabled) {
            std::pair<std::string, int> NavSol(InNavSol().doName(), 0);
            m_enabledMsgs.insert(NavSol);

            ILOG(("Session::enableDefaultMessages(): Enabling NAV DOP Message..."));
            msg.field_msgId().value() = ublox::MsgId_NavDop;
        }

        defaultMsgsEnabled &= sendMessage(msg);

        if (defaultMsgsEnabled) {
            std::pair<std::string, int> NavDop(InNavDop().doName(), 0);
            m_enabledMsgs.insert(NavDop);

            ILOG(("Session::enableDefaultMessages(): Enabling NAV Timegps Message..."));
            msg.field_msgId().value() = ublox::MsgId_NavTimegps;
        }

        defaultMsgsEnabled &= sendMessage(msg);

        if (defaultMsgsEnabled) {
            std::pair<std::string, int> NavTimegps(InNavTimegps().doName(), 0);
            m_enabledMsgs.insert(NavTimegps);
        }

        return defaultMsgsEnabled;
    }

    SerialPort m_serial;
    std::string m_device;
    boost::array<std::uint8_t, 512> m_inputBuf;
    std::vector<std::uint8_t> m_inData;
    Frame m_frame;
    bool m_waitingAckMsg;
    MsgId m_sentMsgId;
    bool m_msgAcked;
    std::map<std::string, int> m_enabledMsgs;
    bool m_checkEnabledMsgs;
    bool m_ackCheck;
    int m_detectedBaudRate;
    bool m_timeIsResolved;
    bool m_coldBootDetected;
};

// NidasAppArg Enable("NOT IMPLEMENTED -E,--enable-msg", "-E UBX-NAV-LLV",
//         "Enable a specific u-blox binary message.", "");
// NidasAppArg Disable("NOT IMPLEMENTED -D,--disable-msg", "-D UBX-NAV-LLV",
//         "Disable a specific u-blox binary message.", "");
NidasAppArg NoBreak("-n,--no-break", "", 
        "Disables the feature to collect enough data to ascertain that the u-blox GPS receiver \n"
        "is operational and then exit. When this option is specified, ubloxbin continuously \n"
        "waits for the enabled messages, but never exits until the user commands it. \n"
        "This option is primarily used for testing.", "");
NidasAppArg Device("-d,--device", "/dev/gps[0-9]",
        "Device to which a u-blox GPS receiver is connected, and which this program uses.\n" 
        "May differ from the example given.", "/dev/gps0");
NidasAppArg Timer("-t, --timer", "", 
        "Causes the utility to determine the time from end of configuration, or \n"
        "restart, to the first TIM-TP message.", "");
NidasAppArg ResetConfig("-r, --reset", "", 
        "Causes the utility to reset the device configuration which is \n"
        "held in battery backed RAM", "");
NidasAppArg SyncMsgCount("-s, --sync", "<int>", 
        "Causes the utility to run until the count of the number of each enabled message \n"
        "reaches the value specified.", "1");
NidasApp app("ubloxbin");

int usage(const char* argv0)
{
    std::cerr
<< argv0 << " is a utility to control the configuration of a u-blox NEO-M8Q GPS receiver." << std::endl
<< "By default it configures the NEO-M8Q GPS Receiver in the following manner: " << std::endl
<< "   * Disables all NMEA and UBX messages and enables the UBX-NAV-PVT, UBX-NAV-SOL, " << std::endl
<< "   * UBX-NAV-DOP, UBX-NAV-TIMEGPS, and UBX-TIM-TP messages. " << std::endl
<< "   * Configures all UBX messages to use the onboard UART. " << std::endl
<< "   * Configures the Real Time Clock to be updated when the receiver has a fix. " << std::endl
<< "   * Configures the receiver to always run - i.e. never enter power saving mode. " << std::endl
<< "   * Collects sufficient enabled receiver messages to determine it is configured correctly and exits. " << std::endl
<< std::endl
<< "As described below, there are command line options to " << std::endl
<< "   * specify the gps device, often a serial port, " << std::endl
<< "   * continously read UBX messages w/o exiting." << std::endl
<< "   * reset the GPS configuration prior to configuring it." << std::endl
<< "   * set the number of each message to receive before declaring the time is synced." << std::endl
<< std::endl
<< "Usage: " << argv0 << " [-d <device path> | -n | -r | -h | -t | -s <int> | -l <log level>]" << std::endl
<< "       " << argv0 << " -h" << std::endl
<< "       " << argv0 << " -d <device path> -l <log level>" << std::endl
<< "       " << argv0 << " -n -l <log level>" << std::endl << std::endl
<< app.usage();

    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(Device | SyncMsgCount | NoBreak | Timer | ResetConfig
                        | app.Help | app.loggingArgs());

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        return usage(argv[0]);
    }
    
    return 0;
}

int main(int argc, char** argv)
{
    if (parseRunString(argc, argv) != 0) {
        return -1;
    }

    try {
        bool success = false;
        for (int tries=0; tries<3; ++tries) {
            boost::asio::io_service io;

            boost::asio::signal_set signals(io, SIGINT, SIGTERM);
            signals.async_wait(
                [&io](const boost::system::error_code& ec, int signum)
                {
                    io.stop();
                    if (ec) {
                        std::cerr << "ERROR: " << ec.message() << std::endl;
                        return;
                    }

                    std::cerr << "Termination due to signal " << signum << std::endl;
                });

            Session session(io, Device.getValue());
            if (!session.start()) {
                return usage(argv[0]);
            }

            boost::asio::io_service readSvc;
            boost::asio::steady_timer readClock(readSvc);
            boost::asio::signal_set readSignals(readSvc, SIGINT, SIGTERM);

            // We start up the asio io_service in a thread so that we can check for Ack/Nak 
            // after sending each configuration message.
            boost::thread run_thread([&] { io.run(); });

            // always check UBX response
            session.setAckCheck();

            // spyOnCapturedInput = true;
            if (!session.findBaudRate()) {
                return usage(argv[0]);
            }
            // spyOnCapturedInput = false;

            success = session.configureUbx();
            if (!success) {
                ILOG(("Failed to configure UBlox receiver!! Trying again..."));
                continue;
            }

            if (ResetConfig.specified()) {
                if (!session.resetUBloxConfig()) {
                    ILOG(("main(): Failed to reset the UBlox config in battery backed RAM"));
                    exit(-1);
                }

                else {
                    ILOG(("Succeeded in resetting UBlox config in battery backed RAM"));
                    if (!session.configureUbx()) {
                        ILOG(("main(): Failed to configure UBlox protocol after resetting UBlox config"));
                    }
                }
            }


            time_t start_time;
            if (Timer.specified()) {
                time(&start_time);
            }

            session.printEnabledMsgs();

            session.setCheckEnabledMsgs();

            int numEnabledMsgTests = 0;
            bool skipTry = false;
            time_t TIM_TP_time = 0;

            ILOG(("ubloxbin: waiting for messages to arrive and be handled..."));

            while (NoBreak.specified() || !skipTry) {
                readClock.expires_from_now(std::chrono::seconds(1));
                readClock.wait();
                session.performRead();
                if (Timer.specified() && TIM_TP_time == 0 
                    && session.msgCount(Session::InTimTp::doName()) != 0) {
                    time(&TIM_TP_time);
                    ILOG(("Time to PPS time sync: ") << (TIM_TP_time-start_time));
                }
                if (session.getCheckEnabledMsgs()) {
                    success = session.testEnabledMsgs(SyncMsgCount.asInt());
                    if (success) {
                        DLOG(("ubloxbin: have all the enabled msgs needed. breaking out..."));
                        break;
                    }
                    else if (!session.timeIsResolved()) {
                        DLOG(("ubloxbin: Time is not resolved and not enough messages reporting. Go around the while loop again..."));
                        continue;
                    }
                    else if (numEnabledMsgTests < 10) {
                            ++numEnabledMsgTests;
                        DLOG(("ubloxbin: Time is resolved, but we don't have all messages reporting. Increment test count:") << numEnabledMsgTests);
                    }
                    else if (!session.coldBootDetected()) {
                        DLOG(("ubloxbin: Time is resolved, not cold booting, but we don't have all messages reporting. Time to start over..."));
                        skipTry = true;
                    }
                }
            }

            io.stop();
            run_thread.join();

            if (tries < 3) {
                if (!success && session.getCheckEnabledMsgs() 
                    && !session.coldBootDetected() && numEnabledMsgTests >= 10) {
                    continue;
                }
                else {
                    break;
                }
            }
            else {
                break;
            } 
        }

        if (!success) {
            return -1;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: Unexpected exception: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}

