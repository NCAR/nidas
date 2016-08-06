/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2005, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

/* Simulate a PSI9116 sensor. */

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <ctime>
#include <cassert>

#include <nidas/util/Socket.h>
#include <nidas/util/UnixSocketAddress.h>
#include <nidas/util/McSocket.h>
#include <nidas/util/Thread.h>
#include <nidas/util/auto_ptr.h>

#include <nidas/core/DSMTime.h>

#include <vector>

using namespace std;

namespace n_u = nidas::util;

class DataWriter: public n_u::Thread
{
public:
    DataWriter(n_u::Socket* sock):
    	n_u::Thread("DataWriter"),socket(sock) {}

    ~DataWriter() { }

    int run() throw(n_u::Exception)
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
	    socket->send(bufout,bp-bufout);
	    nanosleep(&sleepspec,0);
	}
	return RUN_OK;
    }
private:
    n_u::Socket* socket;
};

class PSI: public n_u::Thread
{
public:
    PSI(): n_u::Thread("PSI") {}

    ~PSI() {}

    static int main() throw();

    int run() throw(n_u::Exception)
    {
	n_u::Inet4SocketAddress addr(n_u::Inet4Address(),9000);
        n_u::auto_ptr<n_u::Socket> socket;
	{
	    n_u::ServerSocket waiter(addr);
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
    n_u::SocketAddress* addrptr;
};

/* static */
int PSI::main() throw()
{

    n_u::auto_ptr<PSI> psi;
    try {
	psi.reset(new PSI());
	psi->start();

    }
    catch(n_u::Exception& e) {
        cerr << e.what() << endl;
	return 0;
    }
    try {
	psi->join();
    }
    catch(n_u::Exception& e) {
        cerr << e.what() << endl;
    }
    return 0;
}

int main(int argc, char** argv)
{
    return PSI::main();
}

