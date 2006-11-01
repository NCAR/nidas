/* -*- mode: c++; c-basic-offset: 4; -*-
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <cstring>

#include <nidas/core/Looper.h>

#include <nidas/core/CharacterSensor.h>
#include <nidas/util/SerialPort.h>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

class MensorSim: public LooperClient
{
public:
    MensorSim(n_u::SerialPort* p):port(p) {}
    void looperNotify() throw();
private:
    n_u::SerialPort* port;
};

void MensorSim::looperNotify() throw()
{
    char outbuf[128];
    sprintf(outbuf,"1%f\r\n",1000.0);
    port->write(outbuf,strlen(outbuf));
}

class ParoSim: public LooperClient
{
public:
    ParoSim(n_u::SerialPort* p):port(p) {}
    void looperNotify() throw();
private:
    n_u::SerialPort* port;
};

void ParoSim::looperNotify() throw()
{
    char outbuf[128];
    sprintf(outbuf,"*0001%f\r\n",1000.0);
    port->write(outbuf,strlen(outbuf));
}

class BuckSim: public LooperClient
{
public:
    BuckSim(n_u::SerialPort* p):port(p) {}
    void looperNotify() throw();
private:
    n_u::SerialPort* port;
};

void BuckSim::looperNotify() throw()
{
    const char* outbuf =
    	"14354,-14.23,0,0,-56,0, 33.00,05/08/2003, 17:47:08\r\n";
    port->write(outbuf,strlen(outbuf));
}

class Csat3Sim: public LooperClient
{
public:
    Csat3Sim(n_u::SerialPort* p):port(p),counter(0) {}
    void looperNotify() throw();
private:
    n_u::SerialPort* port;
    unsigned char counter;
};

void Csat3Sim::looperNotify() throw()
{
    unsigned char outbuf[] =
    	{0,4,0,4,0,4,0,4,0x40,0x05,0x55,0xaa};

    outbuf[8] = (outbuf[8] & 0xc0) + counter++;
    counter &= 0x3f;
    port->write((const char*)outbuf,12);
}

class FixedSim: public LooperClient
{
public:
    FixedSim(n_u::SerialPort* p,const string& m):
    	port(p),msg(CharacterSensor::replaceBackslashSequences(m)) {}
    void looperNotify() throw();
private:
    n_u::SerialPort* port;
    string msg;
};

void FixedSim::looperNotify() throw()
{
    port->write(msg.c_str(),msg.length());
}


using std::cin;

/**
 * Read serial records from a file and feed them at a fixed rate.
 **/
class FileSim: public LooperClient
{
public:
    FileSim(n_u::SerialPort* p,const string& path):
	_port(p),_path(path), _in(0)
    {
	if (_path.length() == 0)
	{
	    throw n_u::Exception("FileSim requires an input file.");
	}
	if (_path == "-")
	{
	    _in = &std::cin;
	}
	else
	{
	    // Enable exceptions to get at any failure messages on open, 
	    // then disable them again.
	    try {
		_infile.exceptions(ios::failbit);
		_infile.open (path.c_str());
	    }
	    catch (const std::exception& failure)
	    {
		throw n_u::Exception(failure.what());
	    }
	    _infile.exceptions(std::ios::goodbit);
	    _in = &_infile;
	}
    }

    ~FileSim()
    {
	_in = 0;
    }
	
    void looperNotify() throw();

private:
    n_u::SerialPort* _port;
    string _path;
    std::ifstream _infile;
    std::istream* _in;
};


void FileSim::looperNotify() throw()
{
    // Grab the next line from input.
    string msg;
    if (std::getline(*_in, msg))
    {
	msg += "\r\n";
	CharacterSensor::replaceBackslashSequences(msg);
	//std::cout << msg << std::endl;
	_port->write(msg.c_str(),msg.length());
    }
}



class SensorSim {
public:
    SensorSim();
    int parseRunstring(int argc, char** argv);
    int run();
    static int usage(const char* argv0);
private:
    string device;
    enum sens_type
    { 
      MENSOR_6100, PARO_1000, BUCK_DP, CSAT3, FIXED, 
      ISS_CAMPBELL, UNKNOWN
    } type;
    bool openpty;
    float rate;
    string outputMessage;
    string inputFile;
};

SensorSim::SensorSim(): type(UNKNOWN),openpty(false),rate(1.0)
{
}

int SensorSim::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    openpty = false;

    while ((opt_char = getopt(argc, argv, "cdmio:pr:tf:")) != -1) {
	switch (opt_char) {
	case 'c':
	    type = CSAT3;
	    break;
	case 'd':
	    type = BUCK_DP;
	    break;
	case 'm':
	    type = MENSOR_6100;
	    break;
	case 'i':
	    type = ISS_CAMPBELL;
	    break;
	case 'o':
	    outputMessage = optarg;
	    type = FIXED;
	    break;
	case 'p':
	    type = PARO_1000;
	    break;
	case 'r':
	    rate = atof(optarg);
	    break;
	case 't':
	    openpty = true;
	    break;
	case 'f':
	    inputFile = optarg;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    if (optind == argc - 1) device = string(argv[optind++]);
    if (device.length() == 0) return usage(argv[0]);
    if (type == UNKNOWN) return usage(argv[0]);
    if (optind != argc) return usage(argv[0]);
    return 0;
}

int SensorSim::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "[-p | -m]  device\n\
  -c: simulate CSAT3 sonic anemometer (9600n81, unprompted)\n\
  -d: simulate Buck dewpointer (9600n81, unprompted)\n\
  -m: simulate Mensor 6100 (57600n81,prompted)\n\
  -i: simulate ISS Campbell (96008n1,unprompted)\n\
  -o output_msg:  send a fixed output message at the specified rate\n\
  -p: simulate ParoScientific DigiQuartz 1000 (57600n81, unprompted)\n\
  -r rate: generate data at given rate, in Hz (for unprompted sensor)\n\
  -f file_input: input file for simulated sensors which need it\n\
  -t: open pseudo-terminal device\n\
  device: Name of serial device or pseudo-terminal, e.g. /dev/ttyS1, or /tmp/pty/dev0\n\
" << endl;
    return 1;
}

int SensorSim::run()
{
    try {
	auto_ptr<n_u::SerialPort> port;
	auto_ptr<LooperClient> sim;

	if (openpty) {
	    int fd = n_u::SerialPort::createPtyLink(device);
	    port.reset(new n_u::SerialPort("/dev/ptmx",fd));
	}
	else port.reset(new n_u::SerialPort(device));

	unsigned long msecPeriod =
		(unsigned long)rint(MSECS_PER_SEC / rate);
	// cerr << "msecPeriod=" << msecPeriod << endl;

	string promptStrings[] = { "#1?\n","","","","","" };

	switch (type) {
	case MENSOR_6100:
	    port->setBaudRate(57600);
	    port->iflag() = ICRNL;
	    port->oflag() = OPOST;
	    port->lflag() = ICANON;
	    sim.reset(new MensorSim(port.get()));
	    break;
	case PARO_1000:
	    port->setBaudRate(57600);
	    port->iflag() = 0;
	    port->oflag() = OPOST;
	    port->lflag() = ICANON;
	    sim.reset(new ParoSim(port.get()));
	    break;
	case BUCK_DP:
	    port->setBaudRate(9600);
	    port->iflag() = 0;
	    port->oflag() = OPOST;
	    port->lflag() = ICANON;
	    sim.reset(new BuckSim(port.get()));
	    break;
	case CSAT3:
	    port->setBaudRate(9600);
	    port->iflag() = 0;
	    port->setRaw(true);
	    sim.reset(new Csat3Sim(port.get()));
	    break;
	case FIXED:
	    port->setBaudRate(9600);
	    port->iflag() = 0;
	    port->oflag() = OPOST;
	    port->lflag() = ICANON;
	    sim.reset(new FixedSim(port.get(),outputMessage));
	    break;
	case ISS_CAMPBELL:
	    port->setBaudRate(9600);
	    port->iflag() = 0;
	    port->oflag() = OPOST;
	    port->lflag() = ICANON;
	    sim.reset(new FileSim(port.get(),inputFile));
	    break;
	case UNKNOWN:
	    return 1;
	}

	if (!openpty) port->open(O_RDWR);

	Looper* looper = 0;
	if (promptStrings[type].length() == 0) {
	    looper = Looper::getInstance();
	    looper->addClient(sim.get(),msecPeriod);
	    looper->join();
	}
	else {

	    for (;;) {
		char inbuf[128];
		int l = port->readLine(inbuf,sizeof(inbuf));
		inbuf[l] = '\0';
		if (!strcmp(inbuf,promptStrings[type].c_str()))
		    sim->looperNotify();
		else cerr << "unrecognized prompt: \"" << inbuf << "\"" << endl;
	    }
	}
    }
    catch(n_u::Exception& ex) {
	cerr << ex.what() << endl;
	return 1;
    }
    return 0;
}
int main(int argc, char** argv)
{
    SensorSim sim;
    int res;
    if ((res = sim.parseRunstring(argc,argv)) != 0) return res;
    sim.run();
}

