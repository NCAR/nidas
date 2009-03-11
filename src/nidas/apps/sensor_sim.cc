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
#include <memory> // auto_ptr<>

#include <nidas/core/Looper.h>

#include <nidas/core/CharacterSensor.h>
#include <nidas/util/SerialPort.h>
#include <nidas/util/SerialOptions.h>

using namespace std;
using namespace nidas::core;
using std::cin;

namespace n_u = nidas::util;

enum sens_type
{ 
    CSAT3,
    FROM_FILE,
    FIXED, 
    ISS_CAMPBELL,
    UNKNOWN
};

enum sep_type
{
    EOM_SEPARATOR,
    BOM_SEPARATOR,
};

/**
 * Base class for sensor simulators.
 */
class SensorSimulator: public LooperClient
{
public:
    SensorSimulator(n_u::SerialPort* p,
        bool prompted, string prompt, float rate, int nmessages):
        _port(p), _prompted(prompted),_prompt(prompt),
        _rate(rate),_nmessages(nmessages),
        _interrupted(false) {}

    /**
     * Implement this to send a message from the simulated sensor.
     */
    virtual void sendMessage() throw(n_u::IOException) = 0;

    /**
     * Default implementation of run will call the sendMessage() method
     * either after receipt of a prompt or at the given rate if
     * the sensor is not prompted.  run() will return when
     * isInterrupted() is true.
     */
    virtual void run() throw(n_u::Exception);

    /**
     * Stop the simulation.
     */
    void interrupt() { _interrupted = true; }

    bool isInterrupted() const { return _interrupted; }

    n_u::SerialPort* port() { return _port; }

protected:
    void looperNotify() throw();
    void readPrompts() throw(n_u::IOException);

    n_u::SerialPort* _port;
    bool _prompted;
    string _prompt;
    float _rate;
    int _nmessages;
    bool _interrupted;
};

void SensorSimulator::looperNotify() throw()
{
    if (_interrupted) {
        Looper* looper = Looper::getInstance();
        looper->removeClient(this);
        looper->interrupt();
        return;
    }
    try {
        if (_nmessages >= 0 && _nmessages-- == 0) interrupt();
        else sendMessage();
    }
    catch (n_u::IOException& e) {
        cerr << e.what() << endl;
    }
}

void SensorSimulator::readPrompts() throw(n_u::IOException)
{
    const char* sop = _prompt.c_str();
    const char* eop = sop + _prompt.length();
    const char* pp = sop;
    for (;;) {
        if (isInterrupted()) break;
        char c = _port->readchar();
        if (c == *pp) {
            if (++pp == eop) {
                if (_nmessages >= 0 && _nmessages-- == 0) interrupt();
                else sendMessage();
                pp = sop;
            }
        }
        else if (c == *(pp = sop)) pp++;
        else cerr << "unrecognized prompt char: \"" << c << "\"" << endl;
    }
}

void SensorSimulator::run() throw(n_u::Exception)
{
    if (_prompted) {
        readPrompts();
    }
    else {
	unsigned long msecPeriod =
		(unsigned long)rint(MSECS_PER_SEC / _rate);
	// cerr << "msecPeriod=" << msecPeriod << endl;

        Looper* looper = Looper::getInstance();
        looper->addClient(this,msecPeriod);
        looper->join();
    }
}

/**
 * Send a fixed message at a given rate or after a prompt.
 */
class FixedSim: public SensorSimulator
{
public:
    FixedSim(n_u::SerialPort* p,const string& m,enum sep_type septype, string sep,
        bool prompted, string prompt, float rate,int nmessages);
    void sendMessage() throw(n_u::IOException);
private:
    string _msg;
    enum sep_type _septype;
    string _separator;
};

FixedSim::FixedSim(n_u::SerialPort* p,const string& msg,
    enum sep_type septype, string sep,
    bool prompted, string prompt, float rate,int nmessages):
   SensorSimulator(p,prompted,prompt,rate,nmessages),
   _septype(septype),_separator(sep)
{
    switch (_septype) {
    case BOM_SEPARATOR:
        _msg = _separator + msg;
        break;
    case EOM_SEPARATOR:
        _msg = msg + _separator;
        break;
    }
}

void FixedSim::sendMessage() throw(n_u::IOException)
{
    _port->write(_msg.c_str(),_msg.length());
}

/**
 * Read serial records from a file and send them at a
 * given rate or after a prompt.
 **/
class FileSim: public SensorSimulator
{
public:
    FileSim(n_u::SerialPort* p, const string& path,
        enum sep_type septype,string separator,
        bool prompted,string prompt,float rate, int nmessages,
        bool once,bool verbose = false):
        SensorSimulator(p,prompted,prompt,rate,nmessages),
	_path(path), _in(0),
        _septype(septype),_separator(separator),
        _reopen(false),_onceThru(once),_verbose(verbose)
    {
	open();
    }

    void
    open()
    {
	close();
	if (_verbose)
	    std::cerr << "opening " << _path << " ...\n";
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
	    // then disable them again.  If opening ever fails, disable
	    // reopen flag so we don't keep attempting it.
	    try {
		_infile.clear();
		_infile.exceptions(ios::failbit);
		_infile.open (_path.c_str());
		_infile.exceptions(std::ios::goodbit);
		_in = &_infile;
		_reopen = true;
	    }
	    catch (const std::exception& failure)
	    {
		_reopen = false;
		throw n_u::Exception(failure.what());
	    }
	}
	if (_verbose)
	    std::cerr << _path << " opened.\n";
    }

    void
    close()
    {
	_in = 0;
	_infile.close();
    }

    void
    rewind()
    {
	// Implement rewind with a seek rather than re-opening.
	if (_reopen)
	{
	    if (_verbose)
		std::cerr << "FileSim: rewinding " << _path << std::endl;
	    _infile.clear();
	    _infile.seekg(0);
	    return;
	}
	close();
	if (_reopen) try
	{
	    if (_verbose)
		std::cerr << "FileSim: re-opening input file " 
			  << _path << std::endl;
	    open();
	}
	catch (n_u::Exception& e)
	{
	    std::cerr << "FileSim: exception on file input: " 
		      << e.what() << std::endl;
	}
    }

    void sendMessage() throw(n_u::IOException);

private:
    string _path;
    std::ifstream _infile;
    std::istream* _in;
    std::string _msg;
    enum sep_type _septype;
    string _separator;
    bool _reopen;
    bool _onceThru;
    bool _verbose;
};

void FileSim::sendMessage() throw(n_u::IOException)
{
    // Grab the next line from input.  If the standard input has finished,
    // repeat the last message forever, otherwise loop over the file.
    string msg;

    if (_in && !std::getline(*_in, msg))
    {
        if (_onceThru) {
            interrupt();
            return;
        }
	rewind();
	if (_in && !std::getline(*_in, msg))
	{
	    // empty file, quit trying
	    std::cerr << "FileSim: file is empty, reopens disabled.\n";
	    close();
	    _reopen = false;
	}
    }
    // if file is still open, then we read a new message
    if (_in)
    {
        switch (_septype) {
        case BOM_SEPARATOR:
            msg = _separator + msg;
            break;
        case EOM_SEPARATOR:
            msg = msg + _separator;
            break;
        }
	n_u::replaceBackslashSequences(msg);
	_msg = msg;
    }
    if (_verbose) std::cout << _msg;
    _port->write(_msg.c_str(), _msg.length());
}

/**
 * Simulate a CSAT3 sonic.
 */
class Csat3Sim: public SensorSimulator
{
public:
    Csat3Sim(n_u::SerialPort* p,float rate,int nmessages):
       SensorSimulator(p,false,"",rate,nmessages)
       {}
    void run() throw(n_u::Exception);
    void sendMessage() throw(n_u::IOException);
private:
    int _cntr;
};

void Csat3Sim::sendMessage() throw(n_u::IOException)
{
    if (_cntr % 64 == 99) {
        // every once in a while send a bunch of junk
        unsigned char outbuf[8192];
        unsigned int len = sizeof(outbuf)/sizeof(outbuf[0]);
        for (unsigned int i = 0; i < len; i++) outbuf[i] = i % 0xff;
        _port->write((const char*)outbuf,len);
    }
    else {
        unsigned char outbuf[] =
            {0,4,0,4,0,4,0,4,0x40,0x05,0x55,0xaa};
        outbuf[8] = (outbuf[8] & 0xc0) + (_cntr & 0x3f);
        _port->write((const char*)outbuf,12);
    }
    _cntr++;
}

void Csat3Sim::run() throw(n_u::Exception)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(_port->getFd(),&rfds);
    int nfds = _port->getFd() + 1;
    struct timeval timeout = {0, rint(USECS_PER_SEC / _rate)};
    bool running = false;
    bool datamode = false;
    int nquest = 0;

    _cntr = 0;

    for (;;) {
        fd_set rtfds = rfds;
        struct timeval tval = timeout;

        int res = ::select(nfds,&rtfds,0,0,&tval);
        if (res < 0) throw n_u::IOException(_port->getName(),"select",errno);
        if (res == 0 && running && datamode) {
            if (_cntr == _nmessages) break;
            sendMessage();
        }
        else if (FD_ISSET(_port->getFd(),&rtfds)) {
            char buf[8];
            int nc = _port->read(buf,sizeof(buf));
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
                        _port->write(outmsg,strlen(outmsg));
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

class SensorSimApp {
public:
    SensorSimApp();
    int parseRunstring(int argc, char** argv);
    static int usage(const char* argv0);
    int main();
private:
    string _device;
    enum sens_type _type;
    enum sep_type _septype;
    string _outputMessage;
    string _separator;
    bool _prompted;
    string _prompt;
    bool _openpty;
    bool _verbose;
    float _rate;
    int _nmessages;
    string _fixedMessage;
    string _inputFile;
    bool _onceThru;
    static string defaultTermioOpts;
    string _termioOpts;
};

/* static */
string SensorSimApp::defaultTermioOpts = "9600n81lnr";

SensorSimApp::SensorSimApp(): _type(UNKNOWN),_septype(EOM_SEPARATOR),_separator("\r\n"),
    _prompted(false),_openpty(false),_verbose(false),
    _rate(1.0),_nmessages(-1),_onceThru(false),
    _termioOpts(defaultTermioOpts)
{
}

int SensorSimApp::parseRunstring(int argc, char** argv)
{
    extern char *optarg;       /* set by getopt() */
    extern int optind;       /* "  "     "     */
    int opt_char;     /* option character */

    while ((opt_char = getopt(argc, argv, "b:ce:f:F:igm:n:o:p:r:tv")) != -1) {
	switch (opt_char) {
        case 'b':
            _septype = BOM_SEPARATOR;
            _separator = n_u::replaceBackslashSequences(optarg);
            break;
	case 'c':
	    _type = CSAT3;
            _termioOpts = "9600n81lnr";
	    break;
        case 'e':
            _septype = EOM_SEPARATOR;
            _separator = n_u::replaceBackslashSequences(optarg);
            break;
        case 'f':
            _type = FROM_FILE;
            _inputFile = optarg;
            _onceThru = true;
            break;
	case 'F':
            _type = FROM_FILE;
            _inputFile = optarg;
            _onceThru = false;
	    break;
	case 'i':
	    _type = ISS_CAMPBELL;
            _termioOpts = "9600n81lnr";
	    break;
	case 'm':
	    _type = FIXED;
	    _outputMessage = optarg;
	    break;
	case 'n':
	    _nmessages = atoi(optarg);
	    break;
        case 'o':
            _termioOpts = optarg;
            break;
	case 'p':
	    _prompted = true;
            _prompt = n_u::replaceBackslashSequences(optarg);
	    break;
	case 'r':
	    _rate = atof(optarg);
	    break;
	case 't':
	    _openpty = true;
	    break;
	case 'v':
	    _verbose = true;
	    break;
	case '?':
	    return usage(argv[0]);
	}
    }
    if (optind == argc - 1) _device = string(argv[optind++]);
    if (_device.length() == 0) return usage(argv[0]);
    if (_type == UNKNOWN) return usage(argv[0]);
    if (optind != argc) return usage(argv[0]);
    return 0;
}

int SensorSimApp::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << " [-b sep] [-c] [-e sep] [-f file|-] [-F file|-]\n\
    [-i] [-m msg] [-o termio_opts] [-p prompt] [-r rate] [-v] [-t] device\n\
  -b sep: send separator at beginning of message\n\
    separator can contain backslash sequences, like \\r, \\n or \\xhh,\n\
    where hh are two hex digits\n\
  -c: simulate CSAT3 sonic anemometer (9600n81lnr, unprompted)\n\
  -e sep: send separator at end of message. Default record separator\n\
    option is \"-e \\n\"\n\
  -f file_input: input file of simulated sensor data.\n\
     Read standard input if file_input is '-'. Read until EOF.\n\
     Newlines in the file are replaced by the -b or -e option strings\n\
     before being sent. After opening the device,\n" <<
     argv0 << " will do a kill -STOP on itself before sending any\n\
     messages.  Do \"kill -CONT %1\" from the shell to resume execution\n\
  -F file_input: Like -f, but loop over the file until -n n messages\n\
    have been sent.  If the file is the standard input,\n\
    repeat the last message. Newlines in the file are replaced by the\n\
    -b or -e option strings before being sent.\n\
  -i: simulate ISS Campbell (9600n81lnr,unprompted)\n\
  -m msg:  send a fixed output message at the specified rate\n\
    Use -b or -e option to add a separator\n\
  -n n: number of messages to output, default is send till ctrl-C\n\
    If n is > 0, then " << argv0 << " will do a kill -STOP on itself\n\
    after opening the device, before sending any messages.\n\
    Do \"kill -CONT %1\" from the shell to resume execution\n\
  -o termio_opts, see below. Default is " << defaultTermioOpts << "\n\
  -p prompt: read given prompt string on serial port before sending data record\n\
  -r rate: generate data at given rate, in Hz (for unprompted sensor)\n\
  -v: Verbose mode.  Echo simulated output and other messages.\n\
  -t: create pseudo-terminal device instead of opening serial device\n\
  device: Name of serial device or pseudo-terminal, e.g. /dev/ttyS1, or /tmp/pty/dev0\n\n\
" << n_u::SerialOptions::usage() << "\n\
" << endl;
    return 1;
}

int SensorSimApp::main()
{
    try {
	auto_ptr<n_u::SerialPort> port;
	auto_ptr<SensorSimulator> sim;

	if (_openpty) {
	    int fd = n_u::SerialPort::createPtyLink(_device);
	    port.reset(new n_u::SerialPort("/dev/ptmx",fd));
	}
	else port.reset(new n_u::SerialPort(_device));

        n_u::SerialOptions options;
        options.parse(_termioOpts);

        port->setOptions(options);

	switch (_type) {
	case CSAT3:
	    sim.reset(new Csat3Sim(port.get(),_rate,_nmessages));
	    break;
	case FROM_FILE:
	    sim.reset(new FileSim(port.get(),_inputFile,_septype,_separator,
                _prompted,_prompt,_rate,_nmessages,_onceThru,_verbose));
	    break;
	case FIXED:
	    sim.reset(new FixedSim(port.get(),_outputMessage,_septype,_separator,
                _prompted,_prompt,_rate,_nmessages));
	    break;
	case ISS_CAMPBELL:
	    sim.reset(new FileSim(port.get(), _inputFile,_septype,_separator,
                _prompted,_prompt,_rate,_nmessages,_onceThru,_verbose));
	    break;
	case UNKNOWN:
	    return 1;
	}

	if (!_openpty) port->open(O_RDWR);

        // After terminal is opened, STOP and wait for instructions...
        if (_nmessages >= 0 || _onceThru) kill(getpid(),SIGSTOP);

        sim->run();
    }
    catch(n_u::Exception& ex) {
	cerr << ex.what() << endl;
	return 1;
    }
    return 0;
}

int main(int argc, char** argv)
{
    SensorSimApp sim;
    int res;
    if ((res = sim.parseRunstring(argc,argv)) != 0) return res;
    sim.main();
}

