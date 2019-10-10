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
    MsgId_GPQ = 0xF040,        // Poll a standard message if talker ID is GP
    MsgId_TXT = 0xF041,        // Text xmission
    MsgId_GNQ = 0xF042,        // Poll a standard message if talker ID is GN
    MsgId_GLQ = 0xF043,        // Poll a standard message if talker ID is GL
    MsgId_GBQ = 0xF044,        // poll a standard message if talker ID is GB
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
    MsgId_GGA,
    MsgId_GLL,
    MsgId_GSA,
    MsgId_GSV,
    MsgId_RMC,
    MsgId_VTG,
    MsgId_GRS,
    MsgId_GST,
    MsgId_ZDA,
    MsgId_GBS,
    MsgId_DTM,
    MsgId_GNS,
    MsgId_VLW,
    MsgId_GPQ,
    MsgId_TXT,
    MsgId_GNQ,
    MsgId_GLQ,
    MsgId_GBQ,
    MsgId_PBX_POS,
    MsgId_PBX_SVSTAT,
    MsgId_PBX_TIME,
    MsgId_PBX_RATE,
    MsgId_PBX_CFG,
};

ublox::MsgId All_UBX_IDs[] = 
{
    ublox::MsgId_NavPosecef,
    ublox::MsgId_NavPosllh,
    ublox::MsgId_NavStatus,
    ublox::MsgId_NavDop,
    ublox::MsgId_NavAtt,
    ublox::MsgId_NavSol,
    ublox::MsgId_NavPvt,
    ublox::MsgId_NavOdo,
    ublox::MsgId_NavResetodo,
    ublox::MsgId_NavVelecef,
    ublox::MsgId_NavVelned,
    ublox::MsgId_NavHpposecef,
    ublox::MsgId_NavHpposllh,
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
    ublox::MsgId_NavSvin,
    ublox::MsgId_NavRelposned,
    ublox::MsgId_NavEkfstatus,
    ublox::MsgId_NavAopstatus,
    ublox::MsgId_NavEoe,
    ublox::MsgId_RxmRaw,
    ublox::MsgId_RxmSfrb,
    ublox::MsgId_RxmSfrbx,
    ublox::MsgId_RxmMeasx,
    ublox::MsgId_RxmRawx,
    ublox::MsgId_RxmSvsi,
    ublox::MsgId_RxmAlm,
    ublox::MsgId_RxmEph,
    ublox::MsgId_RxmRtcm,
    ublox::MsgId_RxmPmreq,
    ublox::MsgId_RxmRlm,
    ublox::MsgId_RxmImes,
    ublox::MsgId_InfError,
    ublox::MsgId_InfWarning,
    ublox::MsgId_InfNotice,
    ublox::MsgId_InfTest,
    ublox::MsgId_InfDebug,
    ublox::MsgId_AckNak,
    ublox::MsgId_AckAck,
    ublox::MsgId_CfgPrt,
    ublox::MsgId_CfgMsg,
    ublox::MsgId_CfgInf,
    ublox::MsgId_CfgRst,
    ublox::MsgId_CfgDat,
    ublox::MsgId_CfgTp,
    ublox::MsgId_CfgRate,
    ublox::MsgId_CfgCfg,
    ublox::MsgId_CfgFxn,
    ublox::MsgId_CfgRxm,
    ublox::MsgId_CfgEkf,
    ublox::MsgId_CfgAnt,
    ublox::MsgId_CfgSbas,
    ublox::MsgId_CfgNmea,
    ublox::MsgId_CfgUsb,
    ublox::MsgId_CfgTmode,
    ublox::MsgId_CfgOdo,
    ublox::MsgId_CfgNvs,
    ublox::MsgId_CfgNavx5,
    ublox::MsgId_CfgNav5,
    ublox::MsgId_CfgEsfgwt,
    ublox::MsgId_CfgTp5,
    ublox::MsgId_CfgPm,
    ublox::MsgId_CfgRinv,
    ublox::MsgId_CfgItfm,
    ublox::MsgId_CfgPm2,
    ublox::MsgId_CfgTmode2,
    ublox::MsgId_CfgGnss,
    ublox::MsgId_CfgLogfilter,
    ublox::MsgId_CfgTxslot,
    ublox::MsgId_CfgPwr,
    ublox::MsgId_CfgHnr,
    ublox::MsgId_CfgEsrc,
    ublox::MsgId_CfgDosc,
    ublox::MsgId_CfgSmgr,
    ublox::MsgId_CfgGeofence,
    ublox::MsgId_CfgDgnss,
    ublox::MsgId_CfgTmode3,
    ublox::MsgId_CfgFixseed,
    ublox::MsgId_CfgDynseed,
    ublox::MsgId_CfgPms,
    ublox::MsgId_UpdSos,
    ublox::MsgId_MonIo,
    ublox::MsgId_MonVer,
    ublox::MsgId_MonMsgpp,
    ublox::MsgId_MonRxbuf,
    ublox::MsgId_MonTxbuf,
    ublox::MsgId_MonHw,
    ublox::MsgId_MonHw2,
    ublox::MsgId_MonRxr,
    ublox::MsgId_MonPatch,
    ublox::MsgId_MonGnss,
    ublox::MsgId_MonSmgr,
    ublox::MsgId_AidReq,
    ublox::MsgId_AidIni,
    ublox::MsgId_AidHui,
    ublox::MsgId_AidData,
    ublox::MsgId_AidAlm,
    ublox::MsgId_AidEph,
    ublox::MsgId_AidAlpsrv,
    ublox::MsgId_AidAop,
    ublox::MsgId_AidAlp,
    ublox::MsgId_TimTp,
    ublox::MsgId_TimTm2,
    ublox::MsgId_TimVrfy,
    ublox::MsgId_TimSvin,
    ublox::MsgId_TimDosc,
    ublox::MsgId_TimTos,
    ublox::MsgId_TimSmeas,
    ublox::MsgId_TimVcocal,
    ublox::MsgId_TimFchg,
    ublox::MsgId_TimHoc,
    ublox::MsgId_EsfMeas,
    ublox::MsgId_EsfRaw,
    ublox::MsgId_EsfStatus,
    ublox::MsgId_EsfIns,
    ublox::MsgId_MgaGps,
    ublox::MsgId_MgaGal,
    ublox::MsgId_MgaBds,
    ublox::MsgId_MgaQzss,
    ublox::MsgId_MgaGlo,
    ublox::MsgId_MgaAno,
    ublox::MsgId_MgaFlash,
    ublox::MsgId_MgaIni,
    ublox::MsgId_MgaAck,
    ublox::MsgId_MgaDbd,
    ublox::MsgId_LogErase,
    ublox::MsgId_LogString,
    ublox::MsgId_LogCreate,
    ublox::MsgId_LogInfo,
    ublox::MsgId_LogRetrieve,
    ublox::MsgId_LogRetrievepos,
    ublox::MsgId_LogRetrievestring,
    ublox::MsgId_LogFindtime,
    ublox::MsgId_LogRetrieveposextra,
    ublox::MsgId_SecSign,
    ublox::MsgId_SecUniqid,
    ublox::MsgId_HnrPvt,
};

class Session 
{
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
    using InAckAck = ublox::message::AckAck<InMessage>;
    using InAckNak = ublox::message::AckNak<InMessage>;
    using MsgId = ublox::MsgId;

public:
    Session(boost::asio::io_service& io, const std::string& dev)
      : m_serial(io), m_device(dev), m_inputBuf(), m_inData(), 
        m_frame(), waitingAckMsg(false), sentMsgId((MsgId)0), msgAcked(false),
        enabledMsgs(), checkEnabledMsgs(false), ackCheck(false)
    {}

    ~Session() = default;

    bool getCheckEnabledMsgs() {return checkEnabledMsgs;}
    void setCheckEnabledMsgs(bool check = true) {checkEnabledMsgs = check;}
    bool getAckCheck() {return ackCheck;}
    void setAckCheck(bool check = true) {ackCheck = check;}

    bool start()
    {
        boost::system::error_code ec;
        m_serial.open(m_device, ec);
        if (ec) {
            std::cerr << "ERROR: Failed to open " << m_device << std::endl;
            return false;
        }

        m_serial.set_option(SerialPort::baud_rate(115200));
        m_serial.set_option(SerialPort::character_size(8));
        m_serial.set_option(SerialPort::parity(SerialPort::parity::none));
        m_serial.set_option(SerialPort::stop_bits(SerialPort::stop_bits::one));
        m_serial.set_option(SerialPort::flow_control(SerialPort::flow_control::none));

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
        bool resolvedTime = validField.getBitValue(OutValidFieldMask::BitIdx_fullyResolved);

        if (LOG_LEVEL == LOGGER_DEBUG) {
            std::cout << "PVT: fix=" << fix << 
                "; lat=" << comms::units::getDegrees<double>(msg.field_lat()) <<
                "; lon=" << comms::units::getDegrees<double>(msg.field_lon()) <<
                "; alt=" << comms::units::getMeters<double>(msg.field_height()) <<
                "; UTC date: " << date << 
                (validDate ? " valid" : " invalid") << " date" <<
                (validTime ? " valid" : " invalid") << " time" <<
                (resolvedTime ? " fully resolved" : " not fully resolved") << " time" <<
                std::endl;
        }

        std::string msgName(msg.doName());
        if (getCheckEnabledMsgs() && msgEnabled(msgName)) {
            enabledMsgs[msgName]++;
        }
    }

    void handle(InAckAck& msg)
    {
        if (getAckCheck()) {
            int ackMsgId = msg.field_msgId().value();
            char buf[32];
            memset(buf, 0, 32);
            sprintf(buf, "0x%04X", ackMsgId);
            std::string ackMsgIdStr(buf, strlen(buf));
            DLOG(("AckAck caught for ") << ackMsgIdStr);
            if (ackMsgId == sentMsgId) {
                waitingAckMsg = false;
                msgAcked = true;
                sentMsgId = ublox::MsgId_AckAck;
            }
        }
    }

    void handle(InAckNak& msg)
    {
        if (getAckCheck()) {
            int nakMsgId = msg.field_msgId().value();
            char buf[32];
            memset(buf, 0, 32);
            sprintf(buf, "0x%04X", nakMsgId);
            std::string nakMsgIdStr(buf, strlen(buf));
            DLOG(("AckNak caught for ") << nakMsgIdStr);
            if (nakMsgId == sentMsgId) {
                waitingAckMsg = false;
                sentMsgId = ublox::MsgId_AckNak;
            }
        }
    }

    void handle(InMessage& msg)
    {
        // ignore all other incoming
        static_cast<void>(&msg);
    }

    void performRead()
    {
        m_serial.async_read_some(
            boost::asio::buffer(m_inputBuf),
            [this](const boost::system::error_code& ec, std::size_t bytesCount)
            {
                if (ec == boost::asio::error::operation_aborted) {
                    return;
                }

                if (ec) {
                    std::cerr << "ERROR: read failed with message: " << ec.message() << std::endl;
                    m_serial.get_io_service().stop();
                    return;
                }

                auto dataBegIter = m_inputBuf.begin();
                auto dataEndIter = dataBegIter + bytesCount;
                m_inData.insert(m_inData.end(), dataBegIter, dataEndIter);
                processInputData();
                performRead();
            });
    }   

    void configureUbx()
    {
        disableAllMessages();
        configureUbxIO();
        configureUbxRTCUpdate();
        configureUbxPowerMode();
        configureUbxNavMode();
        enableDefaultMessages();
    }

    bool msgEnabled(const std::string& name) const
    {
        return (enabledMsgs.find(name) != enabledMsgs.end());
    }

    bool testEnabledMsgs()
    {
        if (getCheckEnabledMsgs()) {
            DLOG(("Session::testEnabledMsgs(): enabled"));
            bool done = true;
            using MsgItem = std::pair<std::string, int>;
            for (MsgItem msg : enabledMsgs) {
                DLOG(("Session::testEnabledMsgs(): msg: ") << msg.first << " - received: " << msg.second);
                done &= (msg.second > 1);
            }

            DLOG(("Session::testEnabledMsgs(): ") << (done ? "done" : "not done." ));
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
        for (MsgItem enabledMsg : enabledMsgs) {
            std::cout << enabledMsg.first << " - received: " << enabledMsg.second << std::endl;
        }
        std::cout << std::endl;
    }

private:

    using AllInMessages =
        std::tuple<
            InNavPvt,
            InAckAck,
            InAckNak
        >;

    using Frame = ublox::frame::UbloxFrame<InMessage, AllInMessages>;

    using SerialPort = boost::asio::serial_port;

    void processInputData()
    {
        if (!m_inData.empty()) {
            auto consumed = comms::processAllWithDispatch(&m_inData[0], m_inData.size(), m_frame, *this);
            m_inData.erase(m_inData.begin(), m_inData.begin() + consumed);
        }    
    }

    void sendMessage(const OutMessage& msg)
    {
        OutBuffer buf;
        buf.reserve(m_frame.length(msg)); // Reserve enough space
        auto iter = std::back_inserter(buf);
        auto es = m_frame.write(msg, iter, buf.max_size());
        if (es == comms::ErrorStatus::UpdateRequired) {
            auto* updateIter = &buf[0];
            es = m_frame.update(updateIter, buf.size());
        }
        static_cast<void>(es);
        assert(es == comms::ErrorStatus::Success); // do not expect any error

        // set the ID of the message to be ack/nak'd
        ackMsgInit(msg.getId());
        for (int i=0; i<3 && !msgAcked; ++i) {
            ackMsgInit(msg.getId());
            while (!buf.empty()) {
                boost::system::error_code ec;
                auto count = m_serial.write_some(boost::asio::buffer(buf), ec);

                if (ec) {
                    std::cerr << "ERROR: write failed with message: " << ec.message() << std::endl;
                    m_serial.get_io_service().stop();
                    return;
                }

                buf.erase(buf.begin(), buf.begin() + count);
            }

            if (getAckCheck()) {
                boost::asio::io_service readSvc;
                boost::asio::steady_timer readClock(readSvc);
                std::chrono::microseconds expireTime(USECS_PER_SEC/10);
                readClock.expires_from_now(expireTime);
                readClock.wait();
                performRead();

                int msgId = msg.getId();
                char buf[32];
                memset(buf, 0, 32);
                sprintf(buf, "0x%04X", msgId);
                std::string msgIdStr(buf, strlen(buf));
                if (waitingAckMsg) {
                    ackMsgInit(msg.getId());
                    ILOG(("Info: failed to receive any ACK/NAK message for try ") << i << " for msgId " << msgIdStr);
                }
                else if (!msgAcked) {
                    ILOG(("Info: Received a NAK for try ") << i << " of " << msgIdStr);
                }
                else {
                    ILOG(("Info: Received an ACK for try ") << i << " of " << msgIdStr);
                }

                if (waitingAckMsg || !msgAcked) {
                    ILOG(("Error: Failed to receive ACK message after three tries..."));
                }
            }
            else {
                break;
            }
        }
    }

    void ackMsgInit(MsgId msgId)
    {
        waitingAckMsg = true;
        sentMsgId = msgId;
        msgAcked = false;
    }

void disableAllMessages()
{
    DLOG(("disableAllMessages:"));

    // iterate over a list of all NMEA message IDs
    DLOG((" disabling NMEA Messages..."));
    using OutCfgMsgCurrent = ublox::message::CfgMsgCurrent<OutMessage>;
    OutCfgMsgCurrent msg;
    msg.field_rate().value() = 0;
    for (NMEA_MsgId msgId : All_NMEA_IDs) {
        char buf[32];
        memset(buf, 0, 32);
        sprintf(buf, "0x%04X", msgId);
        std::string msgIdStr(buf, strlen(buf));
        DLOG(("Disabling NMEA message ID: ") << msgIdStr);
        msg.field_msgId().value() = static_cast<ublox::MsgId>(msgId);
        sendMessage(msg);
    }

    // iterate over a list of all UBX message IDs
    DLOG((" disabling UBX Messages..."));
    for (ublox::MsgId msgId : All_UBX_IDs) {
        char buf[32];
        memset(buf, 0, 32);
        sprintf(buf, "0x%04X", msgId);
        std::string msgIdStr(buf, strlen(buf));
        DLOG(("Disabling UBX message ID: ") << msgIdStr);
        msg.field_msgId().value() = msgId;
        sendMessage(msg);
    }
}

    void configureUbxIO()
    {
        DLOG(("Info: Configuring UBX I/O..."));
        using OutCfgPrtUart = ublox::message::CfgPrtUart<OutMessage>;

        OutCfgPrtUart msg;
        auto& outProtoMaskField = msg.field_outProtoMask();

        using OutProtoMaskField = typename std::decay<decltype(outProtoMaskField)>::type;
        outProtoMaskField.setBitValue(OutProtoMaskField::BitIdx_outUbx, true);
        outProtoMaskField.setBitValue(OutProtoMaskField::BitIdx_outNmea, false);

        auto& inProtoMaskField = msg.field_inProtoMask();
        using InProtoMaskField = typename std::decay<decltype(inProtoMaskField)>::type;
        inProtoMaskField.setBitValue(InProtoMaskField::BitIdx_inUbx, true);
        inProtoMaskField.setBitValue(InProtoMaskField::BitIdx_inNmea, false);

        sendMessage(msg);
    }

    void configureUbxRTCUpdate()
    {
        DLOG(("Info: Configuring UBX RTC Update..."));
        using OutCfgPm = ublox::message::CfgPm<OutMessage>;

        OutCfgPm msg;
        auto& outRTCFlags = msg.field_flags().field_bitsHigh();

        using OutRTCFlags = typename std::decay<decltype(outRTCFlags)>::type;
        outRTCFlags.setBitValue(OutRTCFlags::BitIdx_updateRTC, true);
        outRTCFlags.setBitValue(OutRTCFlags::BitIdx_updateEPH, true);

        sendMessage(msg);
    }

    void configureUbxPowerMode()
    {
        DLOG(("Info: Configuring UBX Power Mode..."));
        using OutCfgRxm = ublox::message::CfgRxm<OutMessage>;

        OutCfgRxm msg;
        auto& outLpMode = msg.field_lpMode().value();

        using OutLpModeValType = typename std::decay<decltype(outLpMode)>::type;
        outLpMode = OutLpModeValType::Continuous;

        sendMessage(msg);
    }

    void configureUbxNavMode()
    {
        DLOG(("Info: Configuring UBX NAV Mode..."));
        using OutCfgNav5 = ublox::message::CfgNav5<OutMessage>;
        OutCfgNav5 msg;
        auto& outDynamicModel = msg.field_dynModel().value();

        using OutDynamicModelType = typename std::decay<decltype(outDynamicModel)>::type;
        // using OutDynamicModelType = typename ublox::message::CfgNav5FieldsCommon::DynModelVal;
        outDynamicModel = OutDynamicModelType::Stationary;

        sendMessage(msg);
    }

    void enableDefaultMessages()
    {
        DLOG(("Info: Enabling NAV PVT Message..."));
        using OutCfgMsgCurrent = ublox::message::CfgMsgCurrent<OutMessage>;
        OutCfgMsgCurrent msg;
        
        msg.field_rate().value() = 1; // 1/sec
        msg.field_msgId().value() = ublox::MsgId_NavPvt;
        sendMessage(msg);
        std::pair<std::string, int> NavPvt(InNavPvt().doName(), 0);
        enabledMsgs.insert(NavPvt);
    }

    SerialPort m_serial;
    std::string m_device;
    boost::array<std::uint8_t, 512> m_inputBuf;
    std::vector<std::uint8_t> m_inData;
    Frame m_frame;
    bool waitingAckMsg;
    MsgId sentMsgId;
    bool msgAcked;
    std::map<std::string, int> enabledMsgs;
    bool checkEnabledMsgs;
    bool ackCheck;
};

NidasAppArg AckCheck("-a,--ack-check", "",
        "Enable Ack/Nak checking when sending messages to the u-blox receiver.", "");
NidasAppArg Enable("NOT IMPLEMENTED -E,--enable-msg", "-E UBX-NAV-LLV",
        "Enable a specific u-blox binary message.", "");
NidasAppArg Disable("NOT IMPLEMENTED -D,--disable-msg", "-D UBX-NAV-LLV",
        "Disable a specific u-blox binary message.", "");
NidasAppArg NoBreak("-n,--no-break", "", 
        "Disables the feature to collect enough data to ascertain that the u-blox GPS receiver is operational, "
        "and then exit. This option .", "");
NidasAppArg Device("-d,--device", "i.e. /dev/ttygps?",
        "Serial device to which a u-blox GPS receiver is connected, and which this program uses.", "/dev/gps0");
NidasApp app("ubloxbin");

int usage(const char* argv0)
{
    std::cerr
<< argv0 << " is a utility to control the configuration of a u-blox NEO-M8Q GPS receiver." << std::endl
<< "By default it configures the NEO-M8Q GPS Receiver in the following manner: " << std::endl
<< "   * Disables all NMEA and UBX messages and enables the UBX-NAV-PVT message. " << std::endl
<< "   * Configures all UBX messages to use the onboard UART. " << std::endl
<< "   * Configures the Real Time Clock to be updated when the receiver has a fix. " << std::endl
<< "   * Configures the receiver to always run - i.e. never enter power saving mode. " << std::endl
<< "   * Collects sufficient enabled receiver messages to determine it is configured correctly and exits. " << std::endl
<< std::endl
<< "As described below, there is a command line option to specify the serial port, " << std::endl
<< "command line options to enable/disable UBX messages, and a command line option to " << std::endl
<< "enable a test to make sure the GPS is operational." << std::endl
<< std::endl
<< "Usage: " << argv0 << "[-d <device ID> | -b | -D UBX-NAV-PVT | -E UBX-NAV-POLL | -l <log level>]" << std::endl
<< "       " << argv0 << "-d <device ID> -l <log level>" << std::endl
<< "       " << argv0 << "-d <device ID> -l <log level>" << std::endl
<< "       " << argv0 << "-m -l <log level>" << std::endl << std::endl
<< app.usage();

    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(AckCheck | Device | Enable | Disable | NoBreak | app.Help | app.loggingArgs());

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

        if (AckCheck.specified()) {
            DLOG(("ubloxbin: AckCheck is specified."));
            session.setAckCheck();            
        }

        // We start up the asio io_service in a thread so that we can check for Ack/Nak 
        // after sending each configuration message.
        boost::thread run_thread([&] { io.run(); });

        session.configureUbx();

        session.printEnabledMsgs();

        // Wait for configuration to finish before checking for correct
        // operation
        if (!NoBreak.specified()) {
            session.setCheckEnabledMsgs();
        }

        boost::asio::io_service readSvc;
        boost::asio::steady_timer readClock(readSvc);
        boost::asio::signal_set readSignals(readSvc, SIGINT, SIGTERM);
        while (true) {
            readClock.expires_from_now(std::chrono::seconds(1));
            readClock.wait();
            session.performRead();
            if (!NoBreak.specified() && session.testEnabledMsgs()) 
                break;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "ERROR: Unexpected exception: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}

