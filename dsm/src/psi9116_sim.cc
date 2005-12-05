//
//              Copyright 2004 (C) by UCAR
//

/* Simulate a PSI9116 sensor. */

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#include <atdUtil/Socket.h>
#include <atdUtil/UnixSocketAddress.h>
#include <atdUtil/McSocket.h>
#include <atdUtil/Thread.h>

#include <DSMTime.h>

#include <vector>

using namespace atdUtil;
using namespace std;

class DataWriter: public Thread
{
public:
    DataWriter(atdUtil::Socket* sock):
    	Thread("DataWriter"),socket(sock) {}

    ~DataWriter() { }

    int run() throw(atdUtil::Exception)
    {
	char bufout[sizeof(float)*12+5];
	char* bp;

	union flip {
	    unsigned long lval;
	    char bytes[4];
	} seqnum;

	struct timespec sleepspec = { 0, NSECS_PER_SEC / 50 };

	for (size_t seq = 1; ;seq++) {
	    bp = bufout;
	    *bp++ = 1;
	    // sequence number is big endian
	    seqnum.lval = seq;
#if __BYTE_ORDER == __BIG_ENDIAN
	    ::memcpy(&seqnum,bp,4);
	    bp += 4;
#else
	    *bp++ = seqnum.bytes[3];
	    *bp++ = seqnum.bytes[2];
	    *bp++ = seqnum.bytes[1];
	    *bp++ = seqnum.bytes[0];
#endif
	    for (int i = 0; i < 12; i++) {
		float val = seq + i;
	        ::memcpy(bp,&val,4);
		bp += 4;
	    }
	    int l = socket->send(bufout,bp-bufout);
	    nanosleep(&sleepspec,0);
	}
	return RUN_OK;
    }
private:
    atdUtil::Socket* socket;
};

class PSI: public Thread
{
public:
    PSI(): Thread("PSI") {}

    ~PSI() {}

    static int main() throw();

    int run() throw(atdUtil::Exception)
    {
	Inet4SocketAddress addr(Inet4Address(),9000);
	auto_ptr<atdUtil::Socket> socket;
	{
	    atdUtil::ServerSocket waiter(addr);
	    socket.reset(waiter.accept());
	    waiter.close();
	}

	char buf[64];

	DataWriter writer(socket.get());

	for (;;) {
	    int l = socket->recv(buf,sizeof(buf),0);
	    cerr << "l=" << l << ": " << string(buf,l) << endl;
	    socket->send("A",1);
	    if (!strncmp(buf,"c 01 0",6)) writer.start();
	}
	socket->close();
	return RUN_OK;
    }
private:
    SocketAddress* addrptr;
};

/* static */
int PSI::main() throw()
{

    auto_ptr<PSI> psi;
    try {
	psi.reset(new PSI());
	psi->start();

    }
    catch(atdUtil::Exception& e) {
        cerr << e.what() << endl;
	return 0;
    }
    try {
	psi->join();
    }
    catch(atdUtil::Exception& e) {
        cerr << e.what() << endl;
    }
    return 0;
}

int main(int argc, char** argv)
{
    return PSI::main();
}

