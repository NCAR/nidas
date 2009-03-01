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
#include <memory>

#include <nidas/core/Looper.h>

#include <nidas/core/CharacterSensor.h>
#include <nidas/util/SerialPort.h>

using namespace std;
using namespace nidas::core;

namespace n_u = nidas::util;

/**
 * Simulate a CSAT3 sonic.
 */
class Csat3Sim
{
public:
    Csat3Sim(n_u::SerialPort* p,float r):port(p),rate(r) {}
    void run() throw(n_u::IOException);
private:
    n_u::SerialPort* port;
    float rate;
};

void Csat3Sim::run() throw(n_u::IOException)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(port->getFd(),&rfds);
    int nfds = port->getFd() + 1;
    struct timeval timeout = {0, rint(USECS_PER_SEC / rate)};
    bool running = false;
    bool datamode = false;
    int nquest = 0;

    for (int cntr = 0; ;) {
        fd_set rtfds = rfds;
        struct timeval tval = timeout;

        int res = ::select(nfds,&rtfds,0,0,&tval);
        if (res < 0) throw n_u::IOException(port->getName(),"select",errno);
        if (res == 0 && running && datamode) {
            if (cntr == 256) break;
            if (cntr % 64 == 99) {
                // every once in a while send a bunch of junk
                unsigned char outbuf[8192];
                unsigned int len = sizeof(outbuf)/sizeof(outbuf[0]);
                for (unsigned int i = 0; i < len; i++) outbuf[i] = i % 0xff;
                port->write((const char*)outbuf,len);
            }
            else {
                unsigned char outbuf[] =
                    {0,4,0,4,0,4,0,4,0x40,0x05,0x55,0xaa};
                outbuf[8] = (outbuf[8] & 0xc0) + (cntr & 0x3f);
                port->write((const char*)outbuf,12);
            }
            cntr++;
        }
        else if (FD_ISSET(port->getFd(),&rtfds)) {
            char buf[8];
            int nc = port->read(buf,sizeof(buf));
            const char* eob = buf + nc;
            for (const char* cp = buf; cp < eob; cp++) {
                switch (*cp) {
                case '&':
                    running = !running;
                    nquest = 0;
                    break;
                case 'D':
                    datamode = true;
                    nquest = 0;
                    break;
                case 'T':
                    datamode = false;
                    nquest = 0;
                    break;
                case '\r':
                    if (nquest == 2 && !datamode) {
                        const char* outmsg="\
ET= 60 ts=i XD=d GN=434a TK=1 UP=5 FK=0 RN=1 IT=1 DR=102 rx=2 fx=038 BX=0 AH=1  AT=0 RS=1 BR=0 RI=1 GO=00000 HA=0 6X=3 3X=2 PD=2 SD=0 ?d sa=1\r\
WM=o ar=0 ZZ=0 DC=1  ELo=010 010 010 ELb=010 010 010 TNo=99b d TNb=97a JD= 007\r\
C0o=-2-2-2 C0b=-2-2-2 RC=0 tlo=8 8 8 tlb=8 8 8 DTR=01740 CA=1 TD=  duty=086     AQ= 60 AC=1 CD=0 SR=1 UX=0 MX=0 DTU=02320 DTC=01160 RD=o ss=1 XP=2 RF=018 DS=007 SN0367 28may04 HF=005 JC=3 CB=3 MD=5 DF=05000 RNA=1 rev 3.0a cs=29072 &=0 os= \r>";
                        port->write(outmsg,strlen(outmsg));
                    }
                    nquest = 0;
                    break;
                case '?':
                    nquest++;
                    break;
                default:
                    cerr << "Unknown character received: 0x" << hex << (int)(unsigned char)*cp << dec << endl;
                    break;
                }
            }
        }
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
    if (_bom) msg = _separator + n_u::replaceBackslashSequences(msg);
    else msg = n_u::replaceBackslashSequences(msg) + _separator;
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
    string separator;
};

SensorSim::SensorSim(): type(UNKNOWN),rate(1.0)
{
}

int SensorSim::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */


    while ((opt_char = getopt(argc, argv, "b:cf:r:")) != -1) {
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
  device: Name of pseudo-terminal, e.g. /tmp/pty/dev0\n\
" << endl;
    return 1;
}

int SensorSim::run()
{
    try {
	auto_ptr<n_u::SerialPort> port;
	auto_ptr<LooperClient> sim;
	auto_ptr<Csat3Sim> csat3;
	Looper* looper = 0;


        int fd = n_u::SerialPort::createPtyLink(device);
        port.reset(new n_u::SerialPort("/dev/ptmx",fd));

	unsigned long msecPeriod =
		(unsigned long)rint(MSECS_PER_SEC / rate);
	// cerr << "msecPeriod=" << msecPeriod << endl;
        //

	switch (type) {
	case FROM_FILE:
	case BOM_FROM_FILE:
            looper = Looper::getInstance();
	    port->setBaudRate(57600);
	    port->iflag() = ICRNL;
	    port->oflag() = OPOST;
	    port->lflag() = ICANON;
	    sim.reset(new FileSim(port.get(),inputFile,looper,
                    type==BOM_FROM_FILE,separator));
            kill(getpid(),SIGSTOP);
            looper->addClient(sim.get(),msecPeriod);
            looper->join();
	    break;
	case CSAT3:
	    port->setBaudRate(9600);
	    port->iflag() = 0;
	    port->lflag() = 0;
	    port->setRaw(true);
	    csat3.reset(new Csat3Sim(port.get(),rate));
            kill(getpid(),SIGSTOP);
            csat3.get()->run();
	    break;
	case UNKNOWN:
	    return 1;
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

