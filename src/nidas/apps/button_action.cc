
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
std::string Device1;
std::string Device2;

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
    auto devs=root.getMemberNames();
    if(devs.size()<2){
        cerr<<"Format error in json file."<<endl;
        return 1;
    }
    Device1=devs[1];
    Device2=devs[0];
    return 0;


}
struct threadargs{
    long i;
    std::shared_ptr<HardwareInterface> hwi;
    std::string Device;
};


int loop(std::shared_ptr<HardwareInterface> hwi,std::string Device){
    HardwareDevice device=hwi->lookupDevice(Device);
    if(exitloop){
        return 0;
        
    }
    if (device.isEmpty())
    {
        std::cerr << "unrecognized device: " <<Device<< endl;
        return 2;
    }
    auto output = device.iOutput();
    if(!output)
    {
        std::cerr<<"unable to open "<<Device<<endl;
        return 3;
    }
    bool buttonDown=false;
    do{
        if(exitloop){
            return 0;
        }
        sleep(1);
        auto button = device.iButton();
        buttonDown=button->isDown();
        
    }while(!(buttonDown));
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
        exitloop=true;
        return 0;
}
void *loopHandler(void *args){
    auto hwi=((struct threadargs*)args)->hwi;
    auto Device=((struct threadargs*)args)->Device;
    loop(hwi,Device);
    //PLOG(("test logging"));
    pthread_barrier_wait(&barr);
    long i=((struct threadargs*)args)->i;
    pthread_exit((void*)i);
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
    cout<<"test"<<endl;
    struct threadargs *button1=(struct threadargs *)malloc(sizeof(struct threadargs));
    struct threadargs *button2=(struct threadargs *)malloc(sizeof(struct threadargs)); 
    pthread_t thread1, thread2;;
    cout<<"test2"<<endl;
    button1->Device=Device1;
    button2->Device=Device2;
    button1->i=0;
    button2->i=1;
    app.setupDaemon();
    //PLOG(("test logging"));
    cout<<"test3"<<endl;
    while(true){
        cout<<"test4"<<endl;
        auto hwi= HardwareInterface::getHardwareInterface();
        cout<<"test 5"<<endl;
        exitloop=false;
        pthread_barrier_init(&barr,0,2);
        button1->hwi=hwi;
        button2->hwi=hwi;
        pthread_create(&thread1,NULL,loopHandler,(void *)button1);
        pthread_create(&thread2,NULL,loopHandler,(void *)button2);
        pthread_join(thread1,0);
        pthread_join(thread2,0);
        hwi.reset();
        sleep(5);
    }
    pthread_barrier_destroy(&barr);
    free(button1);
    free(button2);
    return 0;
}



