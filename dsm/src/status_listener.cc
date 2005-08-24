/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <atdISFF/TimerThread.h>
#include <atdUtil/Inet4Address.h>
#include <atdUtil/Socket.h>
#include <atdUtil/Thread.h>
#include <Datagrams.h>

#include <iostream>
#include <fstream>
#include <string>
#include <map>

using namespace dsm;
using namespace std;
using namespace atdISFF;
using namespace atdUtil;

// -----------------------------------------------------------------------------
class Runstring {
public:
  Runstring(int argc, char** argv);
  static void usage(const char* argv0);
  string _website;
};
Runstring::Runstring(int argc, char** argv)
  :_website("disc/html/")
{
//extern char *optarg; /* set by getopt()  */
  extern int optind;   /*  "  "     "      */
  int opt_char;        /* option character */

  while ((opt_char = getopt(argc, argv, "")) != -1) {
    switch (opt_char) {
    case '?':
      usage(argv[0]);
    }
  }
  if (optind == argc - 1) _website = string(argv[optind++]);
  if (optind != argc) usage(argv[0]);
}
void Runstring::usage(const char* argv0)
{
  cerr << "Usage: " << argv0
       << " web accessible folder" << endl;
  exit(1);
}
// -----------------------------------------------------------------------------
/**
 * This class is a thread that listens to the multicast status messages from
 * all of the DSMs.
 */
class Listener: public Thread
{
public:
  Listener():Thread("Listener") {}
  int run() throw(Exception);

  /** This map contains the latest status message from each DSM */
  // _active["dsmXXX"] = "<html>.....";
  map<string, string> _active;

  /** Mutex for the _active member which is accessed from seperate tasks */
  Mutex _activeMutex;
};
int Listener::run() throw(Exception)
{
  MulticastSocket msock(DSM_MULTICAST_STATUS_PORT);
  msock.joinGroup(Inet4Address::getByName(DSM_MULTICAST_ADDR));
  Inet4SocketAddress from;
  char buf[8192];

  for (;;) {
    // blocking read on multicast socket
    size_t l = msock.recvfrom(buf,sizeof(buf),0,from);
    string IP = from.getInet4Address().getHostName();
    cerr << "Listener::run() " << IP << endl;

    if (l==8192) throw Exception(" char *buf exceeded!");

    // block me with a semaphor! another thread is reading _active
    _activeMutex.lock();
    _active[IP] = buf;
    _activeMutex.unlock();
  }
}
// -----------------------------------------------------------------------------
int main(int argc, char** argv)
{
  Runstring rstr(argc,argv);

  Listener listener;
  try {
    // start up the socket listener thread
    listener.start();
  } catch (Exception& e) {
    cerr << "Exception: " << e.toString() << endl;
    // stop the socket listener thread
    listener.cancel();
    listener.join();
  }

  // start a timer thread
  TimerThread timer("HTML renderer");
  timer.setWakeupIntervalMsec(10000);
  timer.start();

  // main loop
  while (listener.isRunning()) {

    // determine when to generate the HTML
    timer.lock();
    timer.wait();
    if (timer.isInterrupted() || !listener.isRunning()) {
      cerr << "status_listener::main timer interrupted" << endl;
      timer.unlock();
      break;
    }
    timer.unlock();
    cerr << timer.currentTimeMsec() << endl;

    // block me with a semaphor! another thread is writing _active
    ofstream outStat;
    listener._activeMutex.lock();
    map<string, string>::const_iterator mi;
    for ( mi =  listener._active.begin();
          mi != listener._active.end(); ++mi) {

      outStat.open( (rstr._website+mi->first+string(".html")).c_str(), ofstream::out );
      outStat << "<link href='index.css' rel='stylesheet' type='text/css'>" << endl;
      outStat << mi->second << endl;
      outStat.close();

      cerr << (rstr._website+mi->first+string(".html")).c_str() << endl;
    }
    listener._activeMutex.unlock();
  }
  cerr << "listener thread died for some reason" << endl; 
  timer.unlock();  // ~TimerThread does not unlock
}
