
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>
#include <stdlib.h>
#include <nidas/core/NidasApp.h>
#include <nidas/core/HardwareInterface.h>
#include <nidas/util/Termios.h>
#include <json/json.h>

using namespace nidas::core;
using namespace nidas::util;

using std::cerr;
using std::cout;
using std::endl;


NidasApp app("button_action");




std::string Path;
float slp=1;

void usage()
{
    cerr << R""""(Usage: button_action [path] [sleep]

Read input from button and depending on current state (indicated by led), turn associated functions on or off.
    
    path:

    File path to json file containing commands to be executed on button press.

    sleep:

    Optional float indicating how long between each check for button press. If no value provided, defaults to 1.
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
    app.enableArguments(app.loggingArgs() | app.Help | app.DebugDaemon);

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
        if(pargs.size()>3){
            return toomany(arg);
        }
        if(Path.empty()){
            Path=arg;
            continue;
        }
        float f;
        try{
            f=std::stof(arg);
        }
        catch(...){
            std::cerr<<"Please enter valid sleep value >0."<<endl;
            return 1;
        }
        if(f>0){
            slp=f;
        }
        else{
            std::cerr<<"Please enter valid sleep value >0."<<endl;
            return 1;
        }
        continue;
  
        std::cerr << "operation unknown: " << arg << endl;
        return 1;
    }
    return 0;


}

//runs whatever action is associated with given button, such as turning wifi on/off
int runaction(std::string Device, bool isOn, Json::Value root)
{ 
    Json::Value devRoot=root[Device];
    std::string com;
    if(isOn)
    {
        com= devRoot["off"].asString();
        
    }
    else{
        com=devRoot["on"].asString();

    }
    ILOG(("Running command ")<<com);
    return system(com.c_str());
    

}

std::tuple<Json::Value, Json::Value::Members> readJson()
{
    if(Path.empty()){
        PLOG(("No json file path entered"));
        exit(1);
    }
    std::ifstream jFile(Path,std::ifstream::in);
    if(!jFile.is_open()){
        PLOG(("Could not open ")<<Path);
        exit(1);
    }
    Json::Value root;
    try{
        jFile>>root;
    }
    catch(...){
        PLOG(("Could not parse file ")<<Path);
        jFile.close();
        exit(1);
    }
    jFile.close();
    auto devs=root.getMemberNames();
    if(devs.size()<1){
        PLOG(("Format error in file ")<<Path);
        exit(1);
    }
    std::tuple<Json::Value, Json::Value::Members> res(root,devs);
    return res;

}
/*json file example format 
{
    "device1":{
        "on": "command",
        "off": "command",
    },
    "device2": {
        "on": "command"
        "off": "command"
    }

}
*/
//checks given device for button and led states, calls associated action, and toggles led
int check(std::string Device, bool &buttonPress, Json::Value root)
{
    HardwareDevice device= HardwareDevice::lookupDevice(Device);
    
    if (device.isEmpty())
    {
        PLOG(("Unrecognized device: ")<<Device);
        return 2;
    }
    auto output = device.iOutput();
    if(!output)
    {
        PLOG(("Unable to open ")<<Device);
        return 3;
    }
    auto button = device.iButton();
    if(button->isDown())
    {
        auto ledState=output->getState();
        if(ledState==OutputState::OFF)
        {
            runaction(Device,false,root);
            output->on(); //turns LED on
        }
        else
        {    
            runaction(Device,true,root);
            output->off(); //turns LED off
                
        }
        buttonPress=true;
    }
    return 0;

}

int main(int argc, char* argv[]) {

    if (parseRunString(argc, argv))
     {
        exit(1);
     }
    auto tup=readJson();
    auto root=std::get<0>(tup);
    auto devs=std::get<1>(tup);
    app.setupDaemon(); 
    while(true){
        bool buttonPress=false;
        for (auto i : devs)
        { 
            check(i,buttonPress, root);
        }
        if(buttonPress)
        {
            sleep(5);
        }
        else
        {
            sleep(slp);
        }
    }

}



