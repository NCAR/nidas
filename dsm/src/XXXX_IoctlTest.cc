/**
 * User space code that exercises ioctls on the XXXX module.
 */

#include <iostream>

#include <RTL_DSMSensor.h>

#include <xxxx_driver.h>

using namespace std;
using namespace dsm;

int main(int argc, char** argv)
{

    RTL_DSMSensor sensor("/dev/xxxx0");

    try {
        sensor.open(O_RDONLY);
    }
    catch (atdUtil::IOException& ioe) {
        std::cerr << ioe.what() << std::endl;
	return 1;
    }


    struct xxxx_get in;
    sensor.ioctl(XXXX_GET_IOCTL,&in,sizeof(in));
    std::cerr << "xxxx_get=" << in.c << std::endl;

    struct xxxx_set out;
    strcpy(out.c,"0123456789012345678");
    sensor.ioctl(XXXX_SET_IOCTL,&out,sizeof(out));
    std::cerr << "xxxx_set=" << out.c << std::endl;
}

