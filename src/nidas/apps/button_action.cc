
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
#include <pthread.h>
#include <json/json.h>

using namespace nidas::core;
using namespace nidas::util;

using std::cerr;
using std::cout;
using std::endl;


NidasApp app("button_action");



Json::Value root;
std::string Path;
pthread_barrier_t barr;
bool exitloop=false;
std::vector<Json::String> devs;
bool buttonPress=false;

void usage(){
    cerr << R""""(Usage: button_action [path]

Read input from button and depending on current state (indicated by led), turn associated functions on or off.
    
    path:

    File path to json file containing commands to be executed on button press.
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

//runs whatever action is associated with given button, such as turning wifi on/off
int runaction(std::string Device, bool isOn){ 
    Json::Value devRoot=root[Device];
    std::string com;
    if(isOn)
    {
        com= devRoot["off"].asString();
        
    }
    else{
        com=devRoot["on"].asString();

    }
    return system(com.c_str());
    

}

int readJson(){
    if(Path.empty()){
        cerr<<"Please enter json file path."<<endl;
        return 1;
    }
    std::ifstream jFile(Path,std::ifstream::in);
    if(!jFile.is_open()){
        cerr<< "Error opening file."<<endl;
        return 1;
    }
    try{
        jFile>>root;
    }
    catch(...){
        cerr<<"Could not parse json file."<<endl;
        jFile.close();
        return 1;
    }
    jFile.close();
    devs=root.getMemberNames();
    if(devs.size()<2){
        cerr<<"Format error in json file."<<endl;
        return 1;
    }
    return 0;

}


int check(std::shared_ptr<HardwareInterface> hwi,std::string Device){
    HardwareDevice device=hwi->lookupDevice(Device);
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
    if(button->isDown()){
        auto ledState=output->getState();
        if(ledState==OutputState::OFF)
        {
            runaction(Device,false);
            output->on(); //turns LED on
        }
        else
        {    
            runaction(Device,true);
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
    
    int j=readJson();
        if(j!=0)
        {
            return 1;
        }
    app.setupDaemon(); 
    while(true){
        auto hwi= HardwareInterface::getHardwareInterface();
        for (auto i : devs){
            check(hwi,i);
        }
        hwi.reset();
        if(buttonPress){
            sleep(5);
        }
        else{
            sleep(1);
        }
    }

}



