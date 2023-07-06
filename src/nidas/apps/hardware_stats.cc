#include <json/json.h>
#include <nidas/core/NidasApp.h>
#include <nidas/util/Process.h>
#include <nidas/util/Logger.h>
#include <nidas/util/auto_ptr.h>
#include <nidas/util/util.h>
#include <nidas/core/HardwareInterface.h>
#include <nidas/util/Termios.h>

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sys/stat.h>

#include <unistd.h>
#include <stdio.h>   


using namespace nidas::core;
using namespace nidas::util;

using std::cerr;
using std::cout;
using std::endl;

NidasApp app("hardware_stats");
std::string Path;
PortType PTYPE(PortType::RS232);
PortTermination PTERM(PortTermination::NO_TERM);
std::string RTS;

void usage()
{
    cerr << R""""(Usage: hardware_stats [path]

Write status of hardware to a json file.
    
    path:

    File path to create a json file and write to.
    )""""
    <<endl<<app.usage()<<endl;
}

int toomany(const std::string& msg)
{
    cerr << msg << ": too many arguments.  Use -h for help." << endl;
    return 1;
}

int parseRunString(int argc, char* argv[])
{
    app.enableArguments(app.loggingArgs() | app.Help);

    ArgVector args = app.parseArgs(argc, argv);
    if (app.helpRequested())
    {
        usage();
        return 1;
    }

      // Get positional args
    ArgVector pargs = app.unparsedArgs();
    if(pargs.size()==0)
    {
        usage();
        return 1;
    }
    for (auto& arg: pargs)
    {   
        if(pargs.size()>1){
            return toomany(arg);
        }
        Path=arg;
        continue;
  
        std::cerr << "operation unknown: " << arg << endl;
        return 1;
    }
    return 0;


}

void status(HardwareDevice& device, Json::Value& root)
{   
    std::string devName=device.id();
    std::string ptype;
    std::string term;
    bool isDown;
    bool isOn;
    bool error;
    if (auto oi = device.iOutput())
    {   
        error=false;
        root[devName]["error"]=error;
        if(oi->getState()==OutputState::ON){
            isOn=true;
        }
        else{
            isOn=false;
        }
        root[devName]["isOn"]=isOn;
        if (auto iserial = device.iSerial())
        {
            PortType p;
            PortTermination terminal;
            iserial->getConfig(p, terminal);
            ptype= p.toString(ptf_485);
            root[devName]["ptype"]=ptype;
            term=terminal.toShortString();
            root[devName]["term"]=term;
        }
        if (auto ibutton = device.iButton())
        {
            isDown=ibutton->isDown();
            root[devName]["isDown"]=isDown;
        }
    }
    else{
        error=true;
        root[devName]["error"]=error;
    }
}


void loop(){
    auto hwi = HardwareInterface::getHardwareInterface();
    Json::Value root;
    for (auto& device: hwi->devices())
    {
        status(device,root);
        
    }
    std::ofstream file(Path);
    file<<root;
    file.close();
}
int main(int argc, char *argv[]){
    if (parseRunString(argc, argv))
     {
        exit(1);
     }
    loop();
    
    return 0;
}