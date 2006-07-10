/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include <fcntl.h>
#include <cstring>
#include <csignal>

#include <iostream>
#include <iomanip>
#include <vector>

#include <nidas/dynld/DSMSerialSensor.h>
#include <nidas/util/Thread.h>

// #include <dsm_serial.h>

using namespace std;
using namespace nidas::core;
using namespace nidas::dynld;

namespace n_u = nidas::util;

class SerialTest : public n_u::Thread {
public:
    SerialTest(const std::string& dev): Thread(dev),devname(dev) {
	 blockSignal(SIGINT);
	 blockSignal(SIGHUP);
         blockSignal(SIGTERM);
    }

    int run() throw(n_u::Exception);

private:
    std::string devname;

};

int SerialTest::run() throw(n_u::Exception)
{
    DSMSerialSensor s;
    s.setDeviceName(devname);

    try {
	s.setBaudRate(115200);
	std::cerr << "doing s.open" << std::endl;
	s.open(O_RDWR);
	struct dsm_serial_record_info recinfo;
	recinfo.sep[0] = '\n';
	recinfo.sepLen = 1;
	recinfo.atEOM = 1;
	recinfo.recordLen = 0;
	std::cerr << "doing s.ioctl SET_RECORD" << std::endl;
	s.ioctl(DSMSER_SET_RECORD_SEP,&recinfo,sizeof(recinfo));

	struct dsm_serial_prompt prompt;
	strcpy(prompt.str,"hitme\n");
	prompt.len = 6;
	prompt.rate = IRIG_10_HZ;
	s.ioctl(DSMSER_SET_PROMPT,&prompt,sizeof(prompt));

	s.ioctl(DSMSER_START_PROMPTER,0,0);
    }
    catch (n_u::IOException& ioe) {
      std::cerr << ioe.what() << std::endl;
      throw n_u::Exception(ioe.what());
    }

    // s.ioctl(DSMSER_WEEPROM,(const void*)0,0);

    int lread;

    struct {
	unsigned long tt;
	unsigned short len;
    } samp;
    unsigned long ttlast = 0;
    int headlen = sizeof(samp.tt) + sizeof(samp.len);

    // sleep(10);
    int badiff = 0;
    int tdiff;
    int mindiff = 9999999;
    int maxdiff = 0;
    try {
	for (int i = 0; ; i++) {
	    unsigned char buf[512];
	    // cerr << "serial_test: reading" << endl;

	    testCancel();

	    // if (interrupt) break;
	    if ((lread = s.read(&samp,headlen)) != headlen) {
		cerr << "bad lread=" << lread << " headlen=" << headlen << endl;
		testCancel();
	    }
	    // if (interrupt) break;
	    if (samp.len == 0) {
		cerr << "samp.len ==0" << endl;
		continue;
	    }

	    if ((lread = s.read(buf,samp.len)) != samp.len) {
		cerr << "bad lread=" << lread << " samp.len=" << samp.len << endl;
		testCancel();
	    }
	    // if (interrupt) break;
	    tdiff = samp.tt - ttlast;
	    if (tdiff != 10) {
		if (tdiff > maxdiff) maxdiff = tdiff;
		if (tdiff < mindiff) mindiff = tdiff;
		if (!(badiff++ % 100)) {
		    cerr << "maxdiff=" << maxdiff << " mindiff=" << mindiff << endl;
		    mindiff = 9999999;
		    maxdiff = 0;
		}
	    }
	    ttlast = samp.tt;

	    // if (!(i % 10)) s.write("quack",5);
	    if (!(i % 100)) {
		cout << "i=" << i << endl;
		struct dsm_serial_status status;
		s.ioctl(DSMSER_GET_STATUS,&status,sizeof(status));

		cout <<
    " min max uq oq  xmit inchar outchr sample parity  over frame" <<
	endl <<
    "  fifo      avail      lost   lost  overf  error   run error" <<
	endl;

		cout << resetiosflags(ios::left) << setiosflags(ios::right);

		cout << setw(4) << status.max_fifo_usage;
		cout << setw(4) << status.min_fifo_usage;
		cout << setw(3) << status.uart_queue_avail;
		cout << setw(3) << status.output_queue_avail;
		cout << setw(6) << status.char_xmit_queue_avail;
		cout << setw(7) << status.input_chars_lost;
		cout << setw(7) << status.output_chars_lost;
		cout << setw(7) << status.sample_overflows;
		cout << setw(6) << status.pe_cnt;
		cout << setw(6) << status.oe_cnt;
		cout << setw(6) << status.fe_cnt << endl;
	    }
	}
    }
    catch(n_u::IOException& ioe) {
	std::cerr << ioe.what() << std::endl;
	testCancel();
	throw n_u::Exception(ioe.what());
    }
}
    
std::vector<SerialTest*> tests;

void sigAction(int sig, siginfo_t* siginfo, void* vptr) {
    cerr << "received signal " << strsignal(sig) << "(" << sig << ")" <<
	" si_signo=" << (siginfo ? siginfo->si_signo : -1) <<
	" si_errno=" << (siginfo ? siginfo->si_errno : -1) <<
	" si_code=" << (siginfo ? siginfo->si_code : -1) << endl;
										
    switch(sig) {
    case SIGHUP:
    case SIGTERM:
    case SIGINT:
        for (size_t i = 0; i < tests.size(); i++)
	    tests[i]->cancel();
    break;
    }
}
int main(int argc, char** argv)
{

    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGHUP);
    sigaddset(&sigset,SIGTERM);
    sigaddset(&sigset,SIGINT);
    sigprocmask(SIG_UNBLOCK,&sigset,(sigset_t*)0);

    struct sigaction act;
    sigemptyset(&sigset);
    act.sa_mask = sigset;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = sigAction;
    sigaction(SIGHUP,&act,(struct sigaction *)0);
    sigaction(SIGINT,&act,(struct sigaction *)0);
    sigaction(SIGTERM,&act,(struct sigaction *)0);


    if (argc < 2) {
        cerr << "Usage: " << argv[0] << "/dev/dsmserX ..." << endl;
	return 1;
    }

    for (int iarg = 1; iarg < argc; iarg++) {
	const char* dev = argv[iarg];

	SerialTest* s = new SerialTest(dev);
	s->start();
	tests.push_back(s);
    }

    try {
	for (size_t i = 0; i < tests.size(); i++) {
	    tests[i]->join();
	}
    }
    catch(n_u::Exception& e) {
        cerr << "join exception: " << e.what() << endl;
	return 1;
    }
    return 0;
}
