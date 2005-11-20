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
	unsigned char bufout[69];
	unsigned char* bp;

	bufout[0] = 1;

	for (size_t seq = 1; ;seq++) {
	    bp = bufout + 1;
	    ::memcpy(bp,&seq,4);
	    bp += 4;
	    for (int i = 0; i < 16; i++) {
		float val = seq + i;
	        ::memcpy(bp,&val,4);
		bp += 4;
	    }
	    int l = socket->send(bufout,bp-bufout);
	    sleep(1);
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
	Inet4SocketAddress addr(Inet4Address::getByName("localhost"),9000);
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
	Inet4SocketAddress addr(Inet4Address::getByName("localhost"),9000);
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

