
#include <DSMConfigServer.h>

#include <iostream>

using namespace dsm;
using namespace std;

int main(int argc, char** argv)
{
    try {
	DSMConfigServer server(argv[1]);
	server.start();
	server.join();
    }
    catch (const atdUtil::Exception& e) {
	std::cerr << e.what() << std::endl;
    }
}
