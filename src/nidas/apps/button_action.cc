#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <time.h>
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


// device to act on
std::string Device;
Json::Value root;

void usage(){
    cerr << R""""(Usage: button_action   [wifi|p1] 

Read input from button and perform associated actions.
    
    {wifi|p1}:

    Wait for press of specified button, and depending on current state(indicated by led) turn associated functions on or off.)"""";
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


    for (auto& arg: pargs)
    {
        if(pargs.size()>1){
            return toomany(arg);
        }
        if(arg=="wifi")
        {
            Device="wifi";
            continue;
        }
        if(arg=="p1"){
            Device="p1";
            continue;
        }
        if (Device.empty())
        {
            Device = arg;
            continue;
        }
  
        std::cerr << "operation unknown: " << arg << endl;
        return 1;
    }
    return 0;


}
//runs whatever action is associated with given button, such as turning wifi on/off
int runaction(std::string Device, bool isOn){ 
    Json::Value devRoot=root[Device];
    std::string com;
    const char* command;

        if(isOn)
        {
           com= devRoot["off"].asString();
           
        }
        else{
            com=devRoot["on"].asString();

        }
        command=com.c_str();
         system(command);
    


    return 0;

}

int readJson(){
    std::ifstream jFile("/home/daq/Documents/workspace/nidas/src/nidas/apps/button_action.json",std::ifstream::in);
    if(!jFile.is_open()){
        cerr<< "Error opening file."<<endl;
        return 1;
    }
    jFile>>root;
    if(root=="null"){
        cerr<<"Error parsing json file"<<endl;
        return 1;
    }
    jFile.close();
    return 0;


}

int main(int argc, char* argv[]) {

     if (parseRunString(argc, argv))
     {
        exit(1);
     }
    auto hwi= HardwareInterface::getHardwareInterface();
    HardwareDevice device=hwi->lookupDevice(Device);
    if (device.isEmpty())
    {
        std::cerr << "unrecognized device: " << Device << endl;
        return 2;
    }
    auto ioutput = device.iOutput();
    if(!ioutput)
    {
        std::cerr<<"unable to open "<<Device<<endl;
        return 3;
    }
    int j=readJson();
    if(j!=0){
        return 4;
    }
    auto ibutton = device.iButton();
    bool buttonDown=false;
    while(!buttonDown){
        sleep(3);
        //cout<<"test"<<endl;
        buttonDown=ibutton->isDown();
    }

    auto buttonState=device.iOutput()->getState();
    if(buttonState.toString()=="off")
        {
            runaction(Device,false);
            ioutput->on(); //turns LED on
            
       }
       else
       {    runaction(Device,true);
            ioutput->off(); //turns LED off
            
       }

    return 0;
}



