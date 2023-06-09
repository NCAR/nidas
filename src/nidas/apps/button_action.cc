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



Json::Value root;
std::string Path;

void usage(){
    cerr << R""""(Usage: button_action [wifi|p1] 

Read input from button and perform associated actions.
    
    {wifi|p1}:

    Wait for press of specified button, and depending on current state (indicated by led), turn associated functions on or off.
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
    return 0;


}

int loop(std::shared_ptr<HardwareInterface> hwi){
    HardwareDevice wifi=hwi->lookupDevice("wifi");
    HardwareDevice p1=hwi->lookupDevice("p1");
    if (wifi.isEmpty())
    {
        std::cerr << "unrecognized device: wifi" << endl;
        return 2;
    }
     if (p1.isEmpty())
    {
        std::cerr << "unrecognized device: p1" << endl;
        return 2;
    }
    auto wifioutput = wifi.iOutput();
    auto poutput=p1.iOutput();
    if(!wifioutput)
    {
        std::cerr<<"unable to open wifi"<<endl;
        return 3;
    }
    if(!poutput)
    {
        std::cerr<<"unable to open p1"<<endl;
        return 3;
    }
    bool wbuttonDown=false;
    bool pbuttonDown=false;
    do{
        sleep(1);
        auto wbutton = wifi.iButton();
        auto pbutton= p1.iButton();
        wbuttonDown=wbutton->isDown();
        pbuttonDown=pbutton->isDown();
        
    }while(!(wbuttonDown) && !(pbuttonDown));
    auto wledState=wifioutput->getState();
    auto pledState=poutput->getState();
    if(wbuttonDown){
        if(wledState==OutputState::OFF)
        {
            runaction("wifi",false);
            wifioutput->on(); //turns LED on
        }
        else
        {    
            runaction("wifi",true);
            wifioutput->off(); //turns LED off
                
        }
    }
    if(pbuttonDown){
        if(pledState==OutputState::OFF)
        {
            runaction("p1",false);
            poutput->on(); //turns LED on
        }
        else
        {    
            runaction("p1",true);
            poutput->off(); //turns LED off
                
        }

    }
        return 0;
        
}

int main(int argc, char* argv[]) {

    if (parseRunString(argc, argv))
     {
        exit(1);
     }
    bool run=true;
    //app.setupDaemon();
    int j=readJson();
        if(j!=0)
        {
            return 1;
        }    
    while(run){
        auto hwi= HardwareInterface::getHardwareInterface();
        loop(hwi);
        hwi.reset();
        sleep(5);
    }
    return 0;
}



