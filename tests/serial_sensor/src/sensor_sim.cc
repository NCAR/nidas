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

/**
 * binary sensor
 */
class Csat3Sim: public LooperClient
{
public:
    Csat3Sim(n_u::SerialPort* p,Looper* l):port(p),counter(0),looper(l) {}
    void looperNotify() throw();
private:
    n_u::SerialPort* port;
    unsigned int counter;
    Looper* looper;
};

void Csat3Sim::looperNotify() throw()
{
    if (counter % 64 == 13) {
        // every once in a while send a bunch of junk
        unsigned char outbuf[8192];
        unsigned int len = sizeof(outbuf)/sizeof(outbuf[0]);
        for (unsigned int i = 0; i < len; i++) outbuf[i] = i % 0xff;
        port->write((const char*)outbuf,len);
        counter++;
    }
    else {
        unsigned char outbuf[] =
            {0,4,0,4,0,4,0,4,0x40,0x05,0x55,0xaa};
        outbuf[8] = (outbuf[8] & 0xc0) + (counter++ & 0x3f);
        port->write((const char*)outbuf,12);
    }
    if (counter >= 256) {
        looper->interrupt();
        looper->removeClient(this);
    }
}

/**
 * Read serial records from a file and feed them at a fixed rate.
 **/
class FileSim: public LooperClient
{
public:
    FileSim(n_u::SerialPort* p, const string& path, Looper* l,bool bom,
        string separator):
	_port(p),_path(path), _in(0), _looper(l),
        _bom(bom),_separator(separator)
    {
	open();
    }

    void
    open()
    {
	close();
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
		_infile.clear();
		_infile.exceptions(ios::failbit);
		_infile.open (_path.c_str());
		_infile.exceptions(std::ios::goodbit);
		_in = &_infile;
	    }
	    catch (const std::exception& failure)
	    {
		throw n_u::IOException(failure.what(),"open",errno);
	    }
	}
    }

    void
    close()
    {
	_in = 0;
	_infile.close();
    }

    void looperNotify() throw();

private:
    n_u::SerialPort* _port;
    string _path;
    std::ifstream _infile;
    std::istream* _in;
    Looper* _looper;
    bool _bom;
    string _separator;
};


void FileSim::looperNotify() throw()
{
    // Grab the next line from input.
    string msg;

    if (_in && !std::getline(*_in, msg))
    {
	_looper->interrupt();
        _looper->removeClient(this);
        return;
    }
    if (_bom) msg = _separator + CharacterSensor::replaceBackslashSequences(msg);
    else msg = CharacterSensor::replaceBackslashSequences(msg) + _separator;
    // cerr << "writing: " << msg << endl;
    _port->write(msg.c_str(),msg.length());
}

class SensorSim {
public:
    SensorSim();
    int parseRunstring(int argc, char** argv);
    int run();
    static int usage(const char* argv0);
private:
    string device;
    enum sens_type { UNKNOWN, FROM_FILE, BOM_FROM_FILE, CSAT3 } type;
    float rate;
    string inputFile;
    int initialSleep;
    string separator;
};

SensorSim::SensorSim(): type(UNKNOWN),rate(1.0),initialSleep(10)
{
}

int SensorSim::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */


    while ((opt_char = getopt(argc, argv, "b:cf:r:s:")) != -1) {
	switch (opt_char) {
	    break;
	case 'b':
	    type = BOM_FROM_FILE;
            separator = optarg;
	    break;
	case 'c':
	    type = CSAT3;
	    break;
	case 'f':
	    if (type == UNKNOWN) {
                type = FROM_FILE;
                separator = string("\r\n");
            }
	    inputFile = optarg;
            break;
	case 'r':
	    rate = atof(optarg);
	    break;
	case 's':
	    initialSleep = atoi(optarg);
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
Usage: " << argv0 << "[-c] | [-f file] [-r rate] device\n\
  -c: simulate CSAT3 sonic anemometer (9600n81, unprompted)\n\
  -f file: simulate serial sensor(57600n81, unprompted), with data from file\n\
  -r rate: generate data at given rate, in Hz (for unprompted sensor)\n\
  -s sec: initial sleep time before sending data to pseudo-terminal\n\
  device: Name of pseudo-terminal, e.g. /tmp/pty/dev0\n\
" << endl;
    return 1;
}

int SensorSim::run()
{
    try {
	auto_ptr<n_u::SerialPort> port;
	auto_ptr<LooperClient> sim;
	Looper* looper = 0;
        looper = Looper::getInstance();

        int fd = n_u::SerialPort::createPtyLink(device);
        port.reset(new n_u::SerialPort("/dev/ptmx",fd));

	unsigned long msecPeriod =
		(unsigned long)rint(MSECS_PER_SEC / rate);
	// cerr << "msecPeriod=" << msecPeriod << endl;

	switch (type) {
	case FROM_FILE:
	case BOM_FROM_FILE:
	    port->setBaudRate(57600);
	    port->iflag() = ICRNL;
	    port->oflag() = OPOST;
	    port->lflag() = ICANON;
	    sim.reset(new FileSim(port.get(),inputFile,looper,
                    type==BOM_FROM_FILE,separator));
	    break;
	case CSAT3:
	    port->setBaudRate(9600);
	    port->iflag() = 0;
	    port->setRaw(true);
	    sim.reset(new Csat3Sim(port.get(),looper));
	    break;
	case UNKNOWN:
	    return 1;
	}

        ::sleep(initialSleep);

        looper->addClient(sim.get(),msecPeriod);
        looper->join();
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

