#include <algorithm>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>

#include <nidas/core/NidasApp.h>
#include <nidas/core/HardwareInterface.h>
#include <nidas/util/Termios.h>

using namespace nidas::core;
using namespace nidas::util;

using std::cerr;
using std::cout;
using std::endl;


NidasApp app("button_action");


// device to act on
std::string Device;


void usage(){
    cerr << R""""(Usage: button_act [wifi|p1] 

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
int runaction(HardwareDevice device,bool isOn){ 



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
            runaction(device,false);
            ioutput->on(); //turns LED on
            
       }
       else
       {    runaction(device,true);
            ioutput->off(); //turns LED off
            
       }

    return 0;
}



