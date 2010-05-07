/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/core/DSMServerIntf.h>
#include <nidas/linux/ncar_a2d.h>

#include <nidas/core/Project.h>
#include <nidas/core/Datagrams.h> // defines DSM_SERVER_XMLRPC_PORT_TCP

// #include <nidas/util/Logger.h>

#include <dirent.h>
#include <iostream>

using namespace nidas::core;
using namespace std;
using namespace XmlRpc;

namespace n_u = nidas::util;

void List_NCAR_A2Ds::execute(XmlRpcValue& params, XmlRpcValue& result)
{
    cerr << "List_NCAR_A2Ds - params: " << params.toXml() << endl;
    Project *project = Project::getInstance();
    ostringstream ostr;

    map<string, list <int> > testVoltage;
    list <int> voltages;

    voltages.push_back(0);
    voltages.push_back(1);
    voltages.push_back(5);
    testVoltage["4F"] = voltages;
    testVoltage["2T"] = voltages;

    voltages.push_back(10);
    testVoltage["2F"] = voltages;

    voltages.push_front(-10);
    testVoltage["1T"] = voltages;

    ostr << "<body id=calib>";
    ostr << "<br><br>project: " << project->getName();

    // for each site...
    ostr << "<dl>";
    for (SiteIterator si = project->getSiteIterator(); si.hasNext(); ) {
        const Site *site = si.next();

        ostr << "<dt>site: " << site->getName();
        if (site->getNumber() > 0)
            ostr << ", stn# " << site->getNumber();

        // for each DSM...
        ostr << "<dl>";
        for (DSMConfigIterator di = site->getDSMConfigIterator(); di.hasNext(); ) {
            const DSMConfig *dsm = di.next();

            ostr << "<dt>" << dsm->getName() << " " << dsm->getLocation();

            int no_NCAR_A2D = 1;

            // establish an xmlrpc connection to this DSM
            XmlRpcClient dsm_xmlrpc_client(dsm->getName().c_str(), DSM_XMLRPC_PORT_TCP, "/RPC2");

            // for each NCAR based A2D card...
            ostr << "<dl>";
            for (SensorIterator si2 = dsm->getSensorIterator(); si2.hasNext(); ) {
                DSMSensor *sensor = si2.next();

                if (sensor->getClassName().compare("raf.DSMAnalogSensor"))
                    continue;

                no_NCAR_A2D = 0;

                ostr << "<dt>" << sensor->getDeviceName();
                ostr << ", (" << sensor->getDSMId() << "," << sensor->getSensorId() << ")";

                // extract the A2D board serial number
                string A2D_SN;
                CalFile *cf = sensor->getCalFile();
                if (cf) {
                    A2D_SN = cf->getFile();
                    A2D_SN = A2D_SN.substr(0,A2D_SN.find(".dat"));
                    ostr << ", (calfile " << A2D_SN << ".dat)";
                }
                // fetch the current setup from the card itself
                XmlRpcValue get_params, get_result;
                get_params["device"] = sensor->getDeviceName();
                cerr << "get_params: " << get_params.toXml() << endl;
                ncar_a2d_setup setup;
                if (dsm_xmlrpc_client.execute("GetA2dSetup", get_params, get_result)) {
                    if (dsm_xmlrpc_client.isFault()) {
                        string faultString = get_result["faultString"];
                        ostr << ", " << faultString;
                        continue;
                    }
                    for (int i = 0; i < NUM_NCAR_A2D_CHANNELS; i++) {
                        setup.gain[i]   = get_result["gain"][i];
                        setup.offset[i] = get_result["offset"][i];
                        setup.calset[i] = get_result["calset"][i];
                    }
                    setup.vcal = get_result["vcal"];
                }
                else {
                    ostr << ", xmlrpc client NOT responding.";
                    continue;
                }
                cerr << "get_result: " << get_result.toXml() << endl;
                int missMatch = 0;

                const Parameter * parm;

                // for each Sample...
                ostr << "<dl>";
                for (SampleTagIterator ti = sensor->getSampleTagIterator(); ti.hasNext(); ) {
                    const SampleTag * tag = ti.next();

                    // for each Variable...
                    for (VariableIterator vi = tag->getVariableIterator(); vi.hasNext(); ) {
                        const Variable * var = vi.next();
                        int channel = var->getA2dChannel();
                        int gain=1, offset=0;
                        if (channel < 0) continue;
                        parm = var->getParameter("gain");
                        if (parm)
                            gain = (int)parm->getNumericValue(0);

                        parm = var->getParameter("bipolar");
                        if (parm)
                            offset = !(parm->getNumericValue(0));

                        ostr << "<dt>";
                        ostr << var->getName();

                        ostringstream go;
                        go << gain << (offset ? "F" : "T");
    
                        // compare with what is currently configured
                        if ( (setup.gain[channel] != gain) || (setup.offset[channel] != offset) ) {
                        ostr << "<br> CANNOT TEST channel is running as: "
                                 << setup.gain[channel] << (setup.offset[channel] ? "F" : "T");
                            ostr << " but configured as: "
                                 << gain << (offset ? "F" : "T");
                            missMatch = 1;
                            continue;
                        }
                        // create a form to send a test voltage
                        ostr << "<form action=control_dsm.php method=POST target=scriptframe class=vsel>";
                        ostr << "<input type=hidden name=host value=" << dsm->getName().c_str() << ">";
                        ostr << "<input type=hidden name=mthd value=TestVoltage>";
                        ostr << "<input type=hidden name=rcvr value=recvResp>";
                        ostr << "<input type=hidden name=device value=" << sensor->getDeviceName() << ">";
                        ostr << "<input type=hidden name=channel value=" << channel << ">";
                        if (setup.calset[channel]) {
                            ostr << "<input type=submit value=unset onclick=recvResp(\"working...\")>";
                            ostr << "<input type=hidden name=voltage value=99>";
                            ostr << "voltage is: " << setup.vcal;
                        } else {
                            ostr << "<input type=submit value=set onclick=recvResp(\"working...\")>";
                            ostr << "<select name=voltage onclick=submit>";
                            map<string, list <int> >::iterator itv = testVoltage.find(go.str());
                            list<int>::iterator iv;
                            for (iv = itv->second.begin(); iv != itv->second.end(); iv++)
                                if (*iv == setup.vcal)
                                    ostr << "<option selected>" << *iv << "</option>";
                                else
                                    ostr << "<option>" << *iv << "</option>";
                            ostr << "</select>";
                        }
                        ostr << "</form>";
                    }
                }
                ostr << "</dl>";
                if (missMatch)
                    ostr << "Board does not match what is configured";
            }
            ostr << "</dl>";
            if (no_NCAR_A2D)
                ostr << "DSM does not contain any NCAR based A2D cards";

            dsm_xmlrpc_client.close();
        }
        ostr << "</dl>";
    }
    ostr << "</dl></body>";

    result = ostr.str();
    cerr << "List_NCAR_A2Ds - result: " << result.toXml() << endl;
}

void GetDsmList::execute(XmlRpcValue& params, XmlRpcValue& result)
{
    DSMConfigIterator di = Project::getInstance()->getDSMConfigIterator();
    for ( ; di.hasNext(); ) {
        const DSMConfig *dsm = di.next();
        result[dsm->getName()] = dsm->getLocation();
    }
}

int DSMServerIntf::run() throw(n_u::Exception)
{
    // Create an XMLRPC server
    _xmlrpc_server = new XmlRpcServer;

    // These constructors register methods with the XMLRPC server
    List_NCAR_A2Ds listNCARA2Ds (_xmlrpc_server);
    GetDsmList     getdsmlist   (_xmlrpc_server);

    // DEBUG - set verbosity of the xmlrpc server HIGH...
    XmlRpc::setVerbosity(5);

    // Create the server socket on the specified port
    if (!_xmlrpc_server->bindAndListen(DSM_SERVER_XMLRPC_PORT_TCP))
        throw n_u::IOException("XMLRpcPort", "bind", errno);

    // Enable introspection
    _xmlrpc_server->enableIntrospection(true);

    // Wait for requests indefinitely
    // This can be interrupted with a Thread::kill(SIGUSR1);
    _xmlrpc_server->work(-1.0);
    return RUN_OK;
}
