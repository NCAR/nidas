#include <cstdlib>
#include <iostream>
#include <nidas/util/Socket.h>

#include <unistd.h>

using namespace std;

int main(int argc, char** argv)
{

    if (argc < 4) return 1;

    string host  = argv[1];
    int port = atoi(argv[2]);
    int nsend  = atoi(argv[3]);

    nidas::util::Inet4Address toaddr = nidas::util::Inet4Address::getByName(host);
    nidas::util::Inet4SocketAddress tosock(toaddr,port);

    // nidas::util::DatagramSocket sock(53);
    nidas::util::DatagramSocket sock;

    char buf[8];
    strcpy(buf,"hello\n");

    for (int i=0; i < nsend; i++) {

        size_t l = sock.sendto(buf,strlen(buf),0,tosock);
        cout << "sent, l=" << l << endl;
        sleep(2);

    }
    sock.close();
}


