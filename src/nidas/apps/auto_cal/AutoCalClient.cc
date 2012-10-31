#include "AutoCalClient.h"

#include <nidas/core/Project.h>
#include <nidas/core/Variable.h>
#include <nidas/core/CalFile.h>
#include <nidas/core/SocketAddrs.h>

#include <xmlrpcpp/XmlRpc.h>
//#include <xmlrpcpp/XmlRpcClient.h>

#include <gsl/gsl_statistics_float.h>
#include <gsl/gsl_fit.h>

#include <ctime>
#include <sstream>
#include <iomanip>
#include <unistd.h>

#include <QTextStream>
#include <QMessageBox>

typedef unsigned char uchar;

#define TDELAY 10 // time delay after setting a new voltage (seconds)

using namespace XmlRpc;
namespace n_u = nidas::util;

string fillStateDesc[] = {"SKIP", "PEND", "EMPTY", "FULL" };

AutoCalClient::AutoCalClient():
   nLevels(0),
   progress(1),
   testVoltage(false),
   idxVltLvl(-1),
   VltLvl(0)
{
    list <int> volts;
    voltageLevels["--"] = volts;

    volts.push_back(0);
    volts.push_back(1);
    volts.push_back(5);
    voltageLevels["4F"] = volts;
    voltageLevels["2T"] = volts;

    volts.push_back(10);
    voltageLevels["2F"] = volts;

    volts.push_back(-10);
    voltageLevels["1T"] = volts;

    volts.push_front(-99);
    voltageLevels["XX"] = volts;
};


string AutoCalClient::ChnSetDesc(unsigned int val) {
    ostringstream ostr;
    for (int i=7; i>-1; i--) {
        ostr << ((val & (1<<i)) ? "X" : "-");
        if (i>0)  ostr << " ";
    }
    return ostr.str();
}


void AutoCalClient::setTestVoltage(int dsmId, int devId)
{
    testVoltage = true;
    tvDsmId = dsmId;
    tvDevId = devId;
};


bool AutoCalClient::readCalFile(DSMSensor* sensor)
{
    cout << "AutoCalClient::readCalFile(" << sensor->getDSMName() << ":" << sensor->getDeviceName() << ")" << endl;
    uint dsmId = sensor->getDSMId();
    uint devId = sensor->getSensorId();

    dsm_time_t sysTime, calTime = 0;
    ostringstream ostr;

    // pre-fill with '0' in case a calFile is missing an entry
    // create unused (gain bplr) entries for (1 0) and (4 1) anyway
    for( int gain = 0; gain < 3; gain++)
        for( int bplr = 0; bplr < 2; bplr++)
            calFileTime[dsmId][devId][1<<gain][bplr] = 0;

    CalFile *cf = sensor->getCalFile();
    if (!cf) {
        ostr << "CalFile not set for..." << endl;
        ostr << "DSM: " << sensor->getDSMName() << " device: " << sensor->getDeviceName() << endl;
        cout << ostr.str() << endl;
        QMessageBox::warning(0, "CalFile ERROR", ostr.str().c_str());
        return true;
    }
    // extract the A2D board serial number from its CalFile
    calFilePath[dsmId][devId] =
      Project::getInstance()->expandString( cf->getPath() );
    calFileName[dsmId][devId] = cf->getFile();

    ostr << "calFilePath: " << calFilePath[dsmId][devId] << endl;
    ostr << "calFileName: " << calFileName[dsmId][devId] << endl;

    // get system time
    struct timeval tv;
    ::gettimeofday(&tv,0);
    sysTime = (dsm_time_t)tv.tv_sec * USECS_PER_SEC + tv.tv_usec;

    // Read CalFile  containing the following fields after the timeStamp
    // gain bipolar(1=true,0=false) intcp0 slope0 intcp1 slope1 ... intcp7 slope7
    while (sysTime >= calTime) {

        int nd = 2 + NUM_NCAR_A2D_CHANNELS * 2;
        float d[nd];
        try {
            calTime = cf->readTime().toUsecs();
            if (cf->eof()) break;
            int n = cf->readData(d,nd);
            if (n < 2) continue;

            int gain = (int)d[0];
            int bplr = (int)d[1];
            for (int i = 0; i < std::min((n-2)/2,NUM_NCAR_A2D_CHANNELS); i++) {
                calFileIntcp[dsmId][devId][i][gain][bplr] = d[2+i*2];
                calFileSlope[dsmId][devId][i][gain][bplr] = d[3+i*2];
                calFileTime[dsmId][devId][gain][bplr] = calTime;
            }
        }
        catch(const n_u::EOFException& e)
        {
            ostr << e.what(); 
            cout << ostr.str() << endl;
            QMessageBox::warning(0, "CalFile ERROR", ostr.str().c_str());
            break;
        }
        catch(const n_u::IOException& e)
        {
            ostr << e.what(); 
            cout << ostr.str() << endl;
            QMessageBox::warning(0, "CalFile ERROR", ostr.str().c_str());
            break;
        }
        catch(const n_u::ParseException& e)
        {
            ostr << e.what(); 
            cout << ostr.str() << endl;
            QMessageBox::warning(0, "CalFile ERROR", ostr.str().c_str());
            break;
        }
    }
    // re-opening CalFile
    if (cf->eof()) {
        cf->close();
        cf->open();
    }
    return false;
}


ncar_a2d_setup AutoCalClient::GetA2dSetup(int dsmId, int devId)
{
    ncar_a2d_setup setup;

    string dsmName = dsmNames[dsmId];
    string devName = devNames[devId];
    cout << "AutoCalClient::GetA2dSetup dsmName: " << dsmName << " devName: " << devName << endl;

#ifndef SIMULATE
    // establish an xmlrpc connection to this DSM
    XmlRpcClient dsm_xmlrpc_client(dsmName.c_str(),
                                   DSM_XMLRPC_PORT_TCP, "/RPC2");

    // fetch the current setup from the card itself
    XmlRpcValue get_params, get_result;
    get_params["device"] = devName;
    get_params["action"] = "getA2DSetup";
    cout << "  get_params: " << get_params.toXml() << endl;

    if (dsm_xmlrpc_client.execute("SensorAction", get_params, get_result)) {
        if (dsm_xmlrpc_client.isFault()) {
            ostringstream ostr;
            ostr << get_result["faultString"] << endl;
            ostr << "ignoring: " << dsmName << ":" << devName;
            cout << ostr.str() << endl;
            QMessageBox::warning(0, "xmlrpc client fault", ostr.str().c_str());

            dsm_xmlrpc_client.close();
            return setup;
        }
        dsm_xmlrpc_client.close();
        for (uint i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
            setup.gain[i]   = get_result["gain"][i];
            setup.offset[i] = get_result["offset"][i];
            setup.calset[i] = get_result["calset"][i];
        }
        setup.vcal = get_result["vcal"];
    }
    else {
        cout << "xmlrpc client NOT responding" << endl;
    }
#else
    for (uint i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
        setup.gain[i]   = Gains[dsmId][devId][i];
        setup.offset[i] = Bplrs[dsmId][devId][i];
        setup.calset[i] = 1;
    }
    setup.vcal = -99;
#endif
    return setup;
}


void AutoCalClient::TestVoltage(int channel, int level)
{
    cout << "AutoCalClient::TestVoltage   "
         << dsmNames[tvDsmId] << ":" << devNames[tvDevId] << ":"
         << ChnSetDesc(1 << channel) << ":" << level << "v" << endl;

#ifndef SIMULATE
    XmlRpcClient dsm_xmlrpc_client(dsmNames[tvDsmId].c_str(),
                                   DSM_XMLRPC_PORT_TCP, "/RPC2");
#endif

    XmlRpcValue set_params, set_result;
    set_params["device"] = devNames[tvDevId];
    set_params["action"] = "testVoltage";
    set_params["state"] = 1;
    set_params["voltage"] = level;
    set_params["calset"] = (1 << channel);

    cout << " set_params: " << set_params.toXml() << endl;

#ifndef SIMULATE
    // Instruct card to generate a calibration voltage.
    if (dsm_xmlrpc_client.execute("SensorAction", set_params, set_result)) {
        if (dsm_xmlrpc_client.isFault()) {
            cout << "xmlrpc client fault: " << set_result["faultString"] << endl;
        }
    }
    else {
        cout << "xmlrpc client NOT responding" << endl;
    }
    dsm_xmlrpc_client.close();
    cout << "set_result: " << set_result.toXml() << endl;
#endif
    emit updateSelection();
}


bool AutoCalClient::Setup(DSMSensor* sensor)
{
    cout << "AutoCalClient::Setup(" << sensor->getDSMName() << ":" << sensor->getDeviceName() << ")" << endl;

    string dsmName = sensor->getDSMName();
    string devName = sensor->getDeviceName();

    uint dsmId = sensor->getDSMId();
    uint devId = sensor->getSensorId();

    readCalFile(sensor);

#ifndef SIMULATE
    // establish an xmlrpc connection to this DSM
    XmlRpcClient dsm_xmlrpc_client(dsmName.c_str(),
                                   DSM_XMLRPC_PORT_TCP, "/RPC2");

    // fetch the current setup from the card itself
    XmlRpcValue get_params, get_result;
    get_params["device"] = devName;
    get_params["action"] = "getA2DSetup";
    cout << "  get_params: " << get_params.toXml() << endl;
    ncar_a2d_setup setup;

    try {
        if (dsm_xmlrpc_client.execute("SensorAction", get_params, get_result)) {
            if (dsm_xmlrpc_client.isFault()) {
                ostringstream ostr;
                ostr << get_result["faultString"] << endl;
                ostr << "ignoring: " << dsmName << ":" << devName;
                QMessageBox::warning(0, "xmlrpc client fault", ostr.str().c_str());

                dsm_xmlrpc_client.close();
                return true;
            }
            dsm_xmlrpc_client.close();
            for (uint i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
                setup.gain[i]   = get_result["gain"][i];
                setup.offset[i] = get_result["offset"][i];
                setup.calset[i] = get_result["calset"][i];
            }
            setup.vcal = get_result["vcal"];
#ifdef DONT_IGNORE_ACTIVE_CARDS
            if (setup.vcal != -99) {
                // TODO ensure that a -99 is reported back by the driver when nothing is active.
                ostringstream ostr;
                ostr << "A calibration voltage is active here.  Cannot auto calibrate this." << endl;
                ostr << "ignoring: " << dsmName << ":" << devName;
                cout << ostr.str() << endl;
                QMessageBox::warning(0, "card is busy", ostr.str().c_str());
                return true;
            }
#endif
        }
        else {
            ostringstream ostr;
            ostr << get_result["faultString"] << endl;
            ostr << "ignoring: " << dsmName << ":" << devName;
            cout << ostr.str() << endl;
            QMessageBox::warning(0, "xmlrpc client NOT responding", ostr.str().c_str());
            return true;
        }
    }
    catch (XmlRpc::XmlRpcException& e)
    {
        cout << "(" << e.getCode() << ") " << e.getMessage() << endl;
        return true;
    }
    cout << "get_result: " << get_result.toXml() << endl;
#endif
    for (SampleTagIterator ti = sensor->getSampleTagIterator(); ti.hasNext(); ) {
        const SampleTag *tag = ti.next();
        dsm_sample_id_t sampId = tag->getId();
        cout << "sampId: " << sampId << endl;
        if (sampId == 0) cout << "(sampId == 0)" << endl;

        uint varId = 0;
        for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); ) {
            const Variable * var = vi.next();

            int chan = var->getA2dChannel();
            if (chan < 0) {
                temperatureId[dsmId][devId] = sampId;
                break;
            }
            uint channel = chan;

            int gain=1, bplr=0;

            const Parameter * parm;
            parm = var->getParameter("gain");
            if (parm)
                gain = (int)parm->getNumericValue(0);

            parm = var->getParameter("bipolar");
            if (parm)
                bplr = (int)(parm->getNumericValue(0));
#ifndef SIMULATE
            // compare with what is currently configured
            if ( (setup.gain[channel] != gain) || (setup.offset[channel] != !bplr) ) {
                ostringstream ostr;
                ostr << "can not calibrate channel " << channel << " because it is running as: "
                     << setup.gain[channel] << (setup.offset[channel] ? "F" : "T")
                     << " but configured as: "
                     << gain << (bplr ? "T" : "F") << endl
                     << "(you need to reboot this DSM)" << endl
                     << "ignoring: " << dsmName << ":" << devName;
                cout << ostr.str() << endl;
                QMessageBox::warning(0, "miss-configured card", ostr.str().c_str());
                return true;
            }
#endif
            // channel is available
            ostringstream gb;
            gb << gain << (bplr ? "T" : "F");

            sampleInfo[sampId].channel[varId++] = channel;

            timeStamp[dsmId][devId][channel] = 0;
            VarNames[dsmId][devId][channel] = var->getName();
            Gains[dsmId][devId][channel] = gain;
            Bplrs[dsmId][devId][channel] = bplr;

            cout << "AutoCalClient::Setup channel: " << channel << " gain: " << gain << " bplr: " << bplr << endl;

            list<int>::iterator l;
            for ( l = voltageLevels[gb.str()].begin(); l != voltageLevels[gb.str()].end(); l++) {

                calActv[*l][dsmId][devId][channel] = PEND;
                calData[dsmId][devId][channel][*l].reserve( NSAMPS * sizeof(float) );

                cout << sampId;
                cout << " CcalActv[" << *l << "][" << dsmId << "][" << devId << "][" << channel << "] = ";
                cout << fillStateDesc[ calActv[*l][dsmId][devId][channel] ] << endl;

                if (slowestRate[*l] == 0)
                    slowestRate[*l] = UINT_MAX;

                if (slowestRate[*l] > tag->getRate())
                    slowestRate[*l] = (uint) tag->getRate();
            }
            if (nLevels < voltageLevels[gb.str()].size())
                nLevels = voltageLevels[gb.str()].size();
            cout << "nLevels: " << nLevels << endl;
        }
        sampleInfo[sampId].dsmId = dsmId;
        sampleInfo[sampId].devId = devId;
        sampleInfo[sampId].rate  = (uint) tag->getRate();
        sampleInfo[sampId].isaTemperatureId = false;
        sampleInfo[temperatureId[dsmId][devId]].isaTemperatureId = true;
        temperatureData[dsmId][devId].reserve( NSAMPS * sizeof(float) );

        cout << "sampleInfo[" << sampId << "].rate: " << sampleInfo[sampId].rate << endl;
    }
    dsmNames[dsmId] = dsmName;
    devNames[devId] = devName;
    lastTimeStamp = 0;

    list<int>::iterator l;
    for ( l = voltageLevels["1T"].begin(); l != voltageLevels["1T"].end(); l++)
        cout << "slowestRate[" << *l << "]: " << slowestRate[*l] << endl;

    return false;
}


void AutoCalClient::createQtTreeModel( map<dsm_sample_id_t, string>dsmLocations )
{
    // clear out the previous model description
    QTreeModel.str("");

    // for each level
    for (iLevel  = calActv.begin();
         iLevel != calActv.end(); iLevel++) {

        int level        =   iLevel->first;
        dsm_a_type* Dsms = &(iLevel->second);

        // Define QTreeModel with the '0' voltage level since all channels measure it.
        if (level != 0) continue;

        // for each DSM
        for (iDsm  = Dsms->begin();
             iDsm != Dsms->end(); iDsm++) {

            uint dsmId             =   iDsm->first;
            device_a_type* Devices = &(iDsm->second);

            QTreeModel << dsmLocations[dsmId] << "\t" << dsmNames[dsmId] << "\t" << dsmId << "\n";

            // for each device
            for (iDevice  = Devices->begin();
                 iDevice != Devices->end(); iDevice++) {

                uint devId               =   iDevice->first;

                string calFile;
                if ( calFileName[dsmId][devId].empty() )
                    calFile = "---";
                else
                    calFile = calFileName[dsmId][devId];

                QTreeModel << "  " << calFile << "\t" << devNames[devId] << "\t" << devId << "\n";
            }
        }
    }
    // start the Voltage level index
    iLevel = calActv.begin();
}


// This is a re-entrant function that avances to the next calibration voltage level.
// It iterates across the levels, dsmNames, devNames, and Channels.
//
enum stateEnum AutoCalClient::SetNextCalVoltage(enum stateEnum state)
{
    if (state == DONE) {
        // Point to voltage level that is common to all voltage ranges by advancing
        // once past the beginning.  If the range begins with '-10' then '0' is next.
        // Or if the range begins with '0' then '1' is next.  Both '0' and '1' are common
        // voltage levels.
        iLevel = calActv.begin();
        iLevel++;
        cout << __PRETTY_FUNCTION__ << " DONE state... clearing all DSM's channels\n";
    }
    cout << "AutoCalClient::SetNextCalVoltage" << endl;

    if (iLevel == calActv.end() ) {
        iLevel = calActv.begin();
        state = DONE;
    }
    bool alive       = false;
    int level        =   iLevel->first;
    dsm_a_type* Dsms = &(iLevel->second);
    cout << "SNCV " << level << endl;
    VltLvl = level;

    // for each DSM
    for (iDsm  = Dsms->begin();
         iDsm != Dsms->end(); iDsm++) {

        uint dsmId             =   iDsm->first;
        device_a_type* Devices = &(iDsm->second);
        cout << "  " << dsmId << endl;

        // for each device
        for (iDevice  = Devices->begin();
             iDevice != Devices->end(); iDevice++) {

            uint devId               =   iDevice->first;
            channel_a_type* Channels = &(iDevice->second);
            cout << "    " << devId << endl;

#ifndef SIMULATE
            XmlRpcClient dsm_xmlrpc_client(dsmNames[dsmId].c_str(),
                                           DSM_XMLRPC_PORT_TCP, "/RPC2");
#endif

            XmlRpcValue set_params, set_result;
            set_params["device"] = devNames[devId];
            set_params["action"] = "testVoltage";
            uchar ChnSet = 0;

            if (state == DONE) {
                cout << "leaving cal voltages and channels in an open state" << endl;
                set_params["state"] = 0;
                set_params["voltage"] = 0;
                ChnSet = 0xff;
            } else {
                set_params["state"] = 1;
                set_params["voltage"] = level;

                // for each channel
                for (iChannel  = Channels->begin();
                     iChannel != Channels->end(); iChannel++) {

                    uint  channel = iChannel->first;

                    ChnSet |= (1 << channel);
                    calActv[level][dsmId][devId][channel] = EMPTY;

                    cout << "      ";
                    cout << "ScalActv[" << level << "][" << dsmId << "][" << devId << "][" << channel << "] = ";
                    cout << fillStateDesc[ calActv[level][dsmId][devId][channel] ] << endl;
                }
            }
            cout << "    ";
            cout << "XMLRPC ChnSet:    " << ChnSetDesc(ChnSet) << endl;
            set_params["calset"] = ChnSet;

#ifndef SIMULATE
            cout << " set_params: " << set_params.toXml() << endl;

            // Instruct card to generate a calibration voltage.
            if (dsm_xmlrpc_client.execute("SensorAction", set_params, set_result)) {
                if (dsm_xmlrpc_client.isFault()) {
                    cout << "xmlrpc client fault: " << set_result["faultString"] << endl;

                    // skip other cards owned by this DSM
                    dsm_xmlrpc_client.close();
                    break;
                }
            }
            else {
                cout << "xmlrpc client NOT responding" << endl;

                // skip other cards owned by this DSM
                dsm_xmlrpc_client.close();
                break;
            }
            dsm_xmlrpc_client.close();
            cout << "set_result: " << set_result.toXml() << endl;
#endif
            alive = true;
        }
    }
    struct timeval tv;
    ::gettimeofday(&tv,0);
    lastTimeStamp = (dsm_time_t)tv.tv_sec * USECS_PER_SEC + tv.tv_usec;

    if (!alive) return DEAD;

    // re-entrant for each level
    iLevel++;
    idxVltLvl++;

    return state;
}


bool AutoCalClient::receive(const Sample* samp) throw()
{
    dsm_time_t currTimeStamp;
    static int prevSecond;

    struct timeval tv;
    ::gettimeofday(&tv,0);
    currTimeStamp = samp->getTimeTag();

#ifndef SIMULATE
    if (currTimeStamp < lastTimeStamp + TDELAY * USECS_PER_SEC)
        return false;
#endif

    dsm_sample_id_t sampId = samp->getId();
    uint dsmId             = sampleInfo[sampId].dsmId;
    uint devId             = sampleInfo[sampId].devId;

    if (dsmId == 0) { cout << "dsmId == 0\n"; return false; }
    if (devId == 0) { cout << "devId == 0\n"; return false; }

//  cout << n_u::UTime(currTimeStamp).format(true,"%Y %b %d %H:%M:%S") << endl;
//  cout << " AutoCalClient::receive " << sampId << " [" << VltLvl << "][" << dsmId << "][" << devId << "]" << endl;

    const float* fp =
            (const float*) samp->getConstVoidDataPtr();

    // store the card's onboard temperatureData
    // There is only one variable in this sample.
    if (sampleInfo[sampId].isaTemperatureId ) {

        // stop gathering after NSAMPS received
        if (temperatureData[dsmId][devId].size() > NSAMPS-1)
            return true;

//      cout << "RtemperatureData[" << dsmId << "][" << devId << "].size() = ";
//      cout << temperatureData[dsmId][devId].size();

        temperatureData[dsmId][devId].push_back(fp[0]);

//      cout << " " << temperatureData[dsmId][devId].size() << endl;
        return true;
    }
    bool channelFound = false;
    // store the card's generated calibration
    // There are one or more variables in this sample.
    for (uint varId = 0; varId < samp->getDataByteLength()/sizeof(float); varId++) {

        uint channel = sampleInfo[sampId].channel[varId];

        // remember the latest measured value for test display
        testData[dsmId][devId][channel] = fp[varId];

        // ignore samples that are not currently being gathered
        if ( calActv[VltLvl][dsmId][devId][channel] != EMPTY )
            continue;

        channelFound = true;

        // when testing in manual mode, don't gather data.
        if ( testVoltage ) continue;

        // timetag first data value received
        if (timeStamp[dsmId][devId][channel] == 0)
            timeStamp[dsmId][devId][channel] = currTimeStamp;

#ifdef SIMULATE
        calData[dsmId][devId][channel][VltLvl].push_back((double)VltLvl + ((channel+1) * 0.1) );
#else
        calData[dsmId][devId][channel][VltLvl].push_back(fp[varId]);
#endif

        int size = calData[dsmId][devId][channel][VltLvl].size();

        // stop gathering after NSAMPS received
        if (size > NSAMPS-1)
            calActv[VltLvl][dsmId][devId][channel] = FULL;

        // The progress bar to exhibits the rate of the slowest
        // channel gathered at the current voltage level.
        if (size)
            if (sampleInfo[sampId].rate == slowestRate[VltLvl])
                progress = idxVltLvl * NSAMPS + size;

//      cout << n_u::UTime(currTimeStamp).format(true,"%Y %b %d %H:%M:%S ");
//      cout << " progress: " << progress;
//      cout << " sampId: " << sampId;
//      cout << " value: " << setw(10) << fp[varId];
//      cout << " RcalData[" << dsmId << "][" << devId << "][" << channel << "][" << VltLvl << "].size() = ";
//      cout << size << endl;
    }
    // instruct the test voltage wizard page to update its display
    if ( testVoltage && ( prevSecond != tv.tv_sec ) )
        emit dispMesVolt();

    prevSecond = tv.tv_sec;

    if ( !channelFound )
        return false;

    return true;
}


// This funcion checks to see if enough data was gathered.
//
bool AutoCalClient::Gathered()
{
    bool isGathered = false;

    map<dsm_sample_id_t,struct sA2dSampleInfo>::iterator iSI;
    for ( iSI  = sampleInfo.begin();
          iSI != sampleInfo.end(); iSI++ )
    {
        struct sA2dSampleInfo *SI = &(iSI->second);
        if ( SI->isaTemperatureId ) continue;

        map<uint,uint>::iterator iC;
        for ( iC  = SI->channel.begin();
              iC != SI->channel.end(); iC++ )
        {
            uint channel = iC->second;
            enum fillState fillstate = calActv[VltLvl][SI->dsmId][SI->devId][channel];

            if ( fillstate == FULL )
                isGathered = true;
            else if ( fillstate == EMPTY )
                return false;
        }
    }
    if (isGathered)
        cout << "AutoCalClient::Gathered" << endl;

    return isGathered;
}


void AutoCalClient::DisplayResults()
{
    cout << "AutoCalClient::DisplayResults" << endl;

#ifdef SIMULATE
    calData[23][220][3][0].pop_back();
    calData[23][220][3][0].push_back(NAN);
    calData[25][200][2][5].pop_back();
    calData[25][200][2][5].push_back(NAN);
    calData[25][200][1][-10].pop_back();
    calData[25][200][1][-10].push_back(NAN);
#endif

    // for each level
    for (iLevel  = calActv.begin();
         iLevel != calActv.end(); iLevel++) {

        int level        =   iLevel->first;
        dsm_a_type* Dsms = &(iLevel->second);
        cout << "" << level << endl;

        // for each DSM
        for (iDsm  = Dsms->begin();
             iDsm != Dsms->end(); iDsm++) {

            uint dsmId             =   iDsm->first;
            device_a_type* Devices = &(iDsm->second);
            cout << "  " << dsmId << endl;

            // for each device
            for (iDevice  = Devices->begin();
                 iDevice != Devices->end(); iDevice++) {

                uint devId               =   iDevice->first;
                channel_a_type* Channels = &(iDevice->second);
                cout << "    " << devId << endl;

                // for each channel
                for (iChannel  = Channels->begin();
                     iChannel != Channels->end(); iChannel++) {

                    uint  channel = iChannel->first;
                    cout << "      " << channel << endl;

                    cout << "DcalActv[" << level << "][" << dsmId << "][" << devId << "][" << channel << "] = ";
                    cout << fillStateDesc[ calActv[level][dsmId][devId][channel] ] << endl;
                }
            }
        }
    }

    cout << "...................................................................\n";

    struct { int gain; int bplr; } GB[] = {{1,1},{2,0},{2,1},{4,0}};

    dsm_d_type::iterator     iiDsm;
    device_d_type::iterator  iiDevice;
    channel_d_type::iterator iiChannel;
    level_d_type::iterator   iiLevel;
    data_d_type::iterator    iiData;

    vector<float> voltageMin;
    vector<float> voltageMax;

    // for each DSM
    for (iiDsm  = calData.begin();
         iiDsm != calData.end(); iiDsm++) {

        uint dsmId             =   iiDsm->first;
        device_d_type* Devices = &(iiDsm->second);

        // for each device
        for (iiDevice  = Devices->begin();
             iiDevice != Devices->end(); iiDevice++) {

            QString devErr;

            uint devId               =   iiDevice->first;
            channel_d_type* Channels = &(iiDevice->second);

            map<uint, double> c0;  // indexed by channel
            map<uint, double> c1;  // indexed by channel

            // detect bad internal cal voltages on a per board basis
            map<int, bool> detected;

            // for each channel
            for (iiChannel  = Channels->begin();
                 iiChannel != Channels->end(); iiChannel++) {

                uint channel         =   iiChannel->first;
                level_d_type* Levels = &(iiChannel->second);

                double aVoltageLevel, aVoltageMean, aVoltageWeight;
                double aVoltageMin, aVoltageMax;
                vector<double> voltageMean;
                vector<double> voltageLevel;
                vector<double> voltageWeight;

                // for each voltage level
                // NOTE these levels could be from for any (gain, bplr) range.
                for (iiLevel  = Levels->begin();
                     iiLevel != Levels->end(); iiLevel++) {

                    int level         =   iiLevel->first;
                    data_d_type* Data = &(iiLevel->second);
                    size_t nPts = Data->size();
                    cout << "nPts:   " << nPts << endl;

                    // alert user of any out of bound values
                    for (iiData  = Data->begin(); iiData != Data->end(); iiData++)
                        if (isnan(*iiData))
                            if (isNAN[dsmId][devId][channel][level] == false) {
                                isNAN[dsmId][devId][channel][level] = true;

                                QTextStream cout(stdout, QIODevice::WriteOnly);
                                QString qstr;
                                QTextStream(&qstr) << QString::fromStdString(dsmNames[dsmId]) << ":";
                                QTextStream(&qstr) << QString::fromStdString(devNames[devId]);
                                QTextStream(&qstr) << "\n\nchannel: " << channel << " level: " << level << "v";
                                QTextStream(&qstr) << " is out of range.\n\nYou may need to adjust ";
                                QTextStream(&qstr) << "the 2 volt offset potentiometer on this card.";
                                cout << "----------------------------------------------" << endl;
                                cout << qstr << endl;
                                cout << "----------------------------------------------" << endl;
                                emit errMessage(qstr);
                                break;
                            }

                    // create a vector from the voltage levels
                    aVoltageLevel = static_cast<double>(level);
                    voltageLevel.push_back( aVoltageLevel );

                    // create a vector from the computed voltage min
                    aVoltageMin = gsl_stats_float_min(
                      &(*Data)[0], 1, nPts);
                    voltageMin.push_back( aVoltageMin );

                    // create a vector from the computed voltage max
                    aVoltageMax = gsl_stats_float_max(
                      &(*Data)[0], 1, nPts);
                    voltageMax.push_back( aVoltageMax );

                    // create a vector from the computed voltage means
                    aVoltageMean = gsl_stats_float_mean(
                      &(*Data)[0], 1, nPts);
                    voltageMean.push_back( aVoltageMean );

                    // create a vector from the computed voltage weights
                    aVoltageWeight = gsl_stats_float_variance(
                      &(*Data)[0], 1, nPts);
                    aVoltageWeight = (aVoltageWeight == 0.0) ? 1.0 : (1.0 / aVoltageWeight);
                    voltageWeight.push_back( aVoltageWeight );

                    cout << "   aVoltageLevel: "  << setprecision(7) << setw(12) << aVoltageLevel;
                    cout << " | aVoltageMin: "    << setprecision(7) << setw(12) << aVoltageMin;
                    cout << " | aVoltageMax: "    << setprecision(7) << setw(12) << aVoltageMax;
                    cout << " | aVoltageMean: "   << setprecision(7) << setw(12) << aVoltageMean;
                    cout << " | aVoltageWeight: " << setprecision(7) << setw(12) << aVoltageWeight;
                    cout << endl;
                    cout << "calData[" << dsmId << "][" << devId << "][" << channel << "][" << level << "]" << endl;
                    for (iiData  = Data->begin(); iiData != Data->end(); iiData++)
                        cout << setprecision(7) << setw(12) << *iiData;
                    cout << endl;

                    // detect measured values outside of desired level
                    if ( (aVoltageMean < (aVoltageLevel - 1.0)) || 
                         (aVoltageMean > (aVoltageLevel + 1.0)) )  {

                        if (detected[level]) continue;
                        detected[level] = true;

                        QTextStream cout(stdout, QIODevice::WriteOnly);

                        QTextStream(&devErr) << "defective card?    ";
                        QTextStream(&devErr) << calFileName[dsmId][devId].c_str() << endl << endl;
                        QTextStream(&devErr) << "channel: " << channel << " level: " << level << "v" << endl;
                        QTextStream(&devErr) << "Internal calibration voltage measures as "<< aVoltageMean << "v" << endl;
                    }
                }
                size_t nPts = voltageLevel.size();
                double cov00, cov01, cov11, chisq;

                vector<double>::iterator iVM = voltageMean.begin();
                cout << "channel: " << channel << endl;
                for ( ; iVM != voltageMean.end(); iVM++ )
                    cout << "iVM: " << *iVM << endl;
                cout << "voltageLevel.size(): " << nPts << endl;

                // compute weighted linear fit to the data
                gsl_fit_wlinear (&voltageMean[0], 1,
                                 &voltageWeight[0], 1,
                                 &voltageLevel[0], 1,
                                 nPts,
                                 &c0[channel], &c1[channel], &cov00, &cov01, &cov11, &chisq);

                // store results for access by the Qt interface.
                int gain = Gains[dsmId][devId][channel];
                int bplr = Bplrs[dsmId][devId][channel];
                resultIntcp[dsmId][devId][channel][gain][bplr] = c0[channel];
                resultSlope[dsmId][devId][channel][gain][bplr] = c1[channel];
            }
            // compute temperature mean
            resultTemperature[dsmId][devId] =
              gsl_stats_float_mean(&(temperatureData[dsmId][devId][0]), 1,
                                     temperatureData[dsmId][devId].size());

            // record results to the device's CalFile
            ostringstream ostr;
            ostr << setprecision(5);
            ostr << endl;
            ostr << "# auto_cal results..." << endl;
            ostr << "# temperature: " << resultTemperature[dsmId][devId] << endl;
            ostr << "#  Date              Gain  Bipolar";
            for (uint ix=0; ix<NUM_NCAR_A2D_CHANNELS; ix++)
                ostr << "  CH" << ix << "-off   CH" << ix << "-slope";
            ostr << endl;

            // for each (gain, bplr) range
            for (uint iGB=0; iGB<4; iGB++) {

                // find out if a channel was calibrated at this range
                uint channel = 99;
                for (uint ix=0; ix<NUM_NCAR_A2D_CHANNELS; ix++) {
                    if ( ( Channels->find(ix) != Channels->end() ) &&
                         ( Gains[dsmId][devId][ix] == GB[iGB].gain ) &&
                         ( Bplrs[dsmId][devId][ix] == GB[iGB].bplr ) ) {
                        channel = ix;
                        break;
                    }
                }
                // display calibrations that were performed at this range
                if ( channel != 99 ) {
                    ostr << n_u::UTime(timeStamp[dsmId][devId][channel]).format(true,"%Y %b %d %H:%M:%S");
                    ostr << setw(6) << dec << GB[iGB].gain;
                    ostr << setw(9) << dec << GB[iGB].bplr;

                    for (uint ix=0; ix<NUM_NCAR_A2D_CHANNELS; ix++) {
                        if ( ( Channels->find(ix) != Channels->end() ) &&
                             ( Gains[dsmId][devId][ix] == GB[iGB].gain ) &&
                             ( Bplrs[dsmId][devId][ix] == GB[iGB].bplr ) )

                            ostr << "  " << setw(9) << c0[ix]
                                 << " "  << setw(9) << c1[ix];
                        else
                            ostr << "          0         1";
                    }
                    ostr << endl;
                }
            }
            // TODO provide the user the option to review the results before storing them
            cout << "calFileName[" << dsmId << "][" << devId << "] = ";
            cout << calFileName[dsmId][devId] << endl;

            calFileResults[dsmId][devId] = ostr.str();
            cout << calFileResults[dsmId][devId] << endl;

            // review the device error results
            if (devErr.length())
                emit errMessage(devErr);
        }
    }
    // show totals for Min and Max
    cout << "voltageMin.size() = " << voltageMin.size() << endl;
    cout << "voltageMax.size() = " << voltageMax.size() << endl;
    double allVoltageMin = gsl_stats_float_min(
      &(voltageMin[0]), 1, voltageMin.size());
    double allVoltageMax = gsl_stats_float_max(
      &(voltageMax[0]), 1, voltageMax.size());
    cout << "allVoltageMin = " << allVoltageMin << endl;
    cout << "allVoltageMax = " << allVoltageMax << endl;

    progress = maxProgress();
}


void AutoCalClient::SaveCalFile(uint dsmId, uint devId)
{
    ostringstream ostr;
    string aCalFile = calFilePath[dsmId][devId] +
                      calFileName[dsmId][devId];

    if (calFileSaved[dsmId][devId]) {
        ostr << "results already saved to: " << aCalFile;
        cout << ostr.str() << endl;
        QMessageBox::information(0, "notice", ostr.str().c_str());
        return;
    }

    cout << "Appending results to: ";
    cout << aCalFile << endl;
    cout << calFileResults[dsmId][devId] << endl;

    int fd = open( aCalFile.c_str(), O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        ostr << "failed to save results to: " << aCalFile << endl;
        ostr << strerror(errno);
        cout << ostr.str() << endl;
        QMessageBox::warning(0, "error", ostr.str().c_str());
        return;
    }
    write(fd, calFileResults[dsmId][devId].c_str(),
              calFileResults[dsmId][devId].length());
    close(fd);
    ostr << "saved results to: " << aCalFile;
    cout << ostr.str() << endl;
    QMessageBox::information(0, "notice", ostr.str().c_str());

    calFileSaved[dsmId][devId] = true;
}


list<int> AutoCalClient::GetVoltageLevels()
{
    return voltageLevels["XX"];
}


list<int> AutoCalClient::GetVoltageLevels(uint dsmId, uint devId, uint chn)
{
    if ( VarNames[dsmId][devId][chn] == "" )
        return voltageLevels["--"];

    int gain = Gains[dsmId][devId][chn];
    int bplr = Bplrs[dsmId][devId][chn];

    ostringstream gb;
    gb << gain << (bplr ? "T" : "F");

    return voltageLevels[gb.str()];
}


string AutoCalClient::GetVarName(uint dsmId, uint devId, uint chn)
{
    if ( VarNames[dsmId][devId][chn] == "" )
        return "---";

    QStrBuf.str(""); QStrBuf << VarNames[dsmId][devId][chn];
    return QStrBuf.str();
}


string AutoCalClient::GetOldTimeStamp(uint dsmId, uint devId, uint chn)
{
    int gain = Gains[dsmId][devId][chn];
    int bplr = Bplrs[dsmId][devId][chn];

    if (calFileTime[dsmId][devId][gain][bplr] == 0)
        return "---- --- -- --:--:--";

    return n_u::UTime(calFileTime[dsmId][devId][gain][bplr]).format(true,"%Y %b %d %H:%M:%S");
}


string AutoCalClient::GetNewTimeStamp(uint dsmId, uint devId, uint chn)
{
    if (timeStamp[dsmId][devId][chn] == 0)
        return "---- --- -- --:--:--";

    return n_u::UTime(timeStamp[dsmId][devId][chn]).format(true,"%Y %b %d %H:%M:%S");
}


string AutoCalClient::GetOldTemperature(uint dsmId, uint devId, uint chn)
{ QStrBuf.str(""); QStrBuf << "temperature[" << dsmId << "][" << devId << "][" << chn << "]"; return QStrBuf.str(); } 


string AutoCalClient::GetNewTemperature(uint dsmId, uint devId, uint chn)
//{ QStrBuf.str(""); QStrBuf << "TEMPERATURE[" << dsmId << "][" << devId << "][" << chn << "]"; return QStrBuf.str(); } 
{
    QStrBuf.str(""); QStrBuf << resultTemperature[dsmId][devId];
    return QStrBuf.str();
}



string AutoCalClient::GetOldIntcp(uint dsmId, uint devId, uint chn)
{
    int gain = Gains[dsmId][devId][chn];
    int bplr = Bplrs[dsmId][devId][chn];

    QStrBuf.str(""); QStrBuf << calFileIntcp[dsmId][devId][chn][gain][bplr];
    return QStrBuf.str();
}


string AutoCalClient::GetNewIntcp(uint dsmId, uint devId, uint chn)
{
    int gain = Gains[dsmId][devId][chn];
    int bplr = Bplrs[dsmId][devId][chn];

    QStrBuf.str(""); QStrBuf << resultIntcp[dsmId][devId][chn][gain][bplr];
    return QStrBuf.str();
}


string AutoCalClient::GetOldSlope(uint dsmId, uint devId, uint chn)
{
    int gain = Gains[dsmId][devId][chn];
    int bplr = Bplrs[dsmId][devId][chn];

    QStrBuf.str(""); QStrBuf << calFileSlope[dsmId][devId][chn][gain][bplr];
    return QStrBuf.str();
}


string AutoCalClient::GetNewSlope(uint dsmId, uint devId, uint chn)
{
    int gain = Gains[dsmId][devId][chn];
    int bplr = Bplrs[dsmId][devId][chn];

    QStrBuf.str(""); QStrBuf << resultSlope[dsmId][devId][chn][gain][bplr];
    return QStrBuf.str();
}
