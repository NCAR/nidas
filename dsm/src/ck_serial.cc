/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

    Test the serial ports.

*/


#include <PortSelectorTest.h>

/* include file(s) for the sensor to be tested */
#include <DSMSerialSensor.h>
#include <dsm_serial.h>

#include <iostream>

using namespace std;
using namespace dsm;

/**
 * A little SampleClient for testing purposes.  Currently
 * just prints out some fields from the Samples it receives.
 */
class TestSampleClient : public SampleClient {
public:

  bool receive(const Sample *s) throw()
{
    cerr << dec << "timetag= " << s->getTimeTag() << " id= " << s->getId() <<
    	" len=" << s->getDataLength();
    float* data = (float*) s->getConstVoidDataPtr();

    for (unsigned int i = 0; i < s->getDataLength(); i++)
        std::cerr << ' ' << data[i];
    std::cerr << std::endl;
    return true;
}

};

int main(int argc, char** argv)
{
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << "/dev/dsmserX ..." << endl;
	return 1;
    }

    PortSelectorTest* handler = PortSelectorTest::createInstance();
    handler->start();

    /* Create the test SampleClient */

    TestSampleClient test;


    try {
	for (int iarg = 1; iarg < argc; iarg++) {
	    const char* dev = argv[iarg];


/* Substitute your sensor instantiation here ********************** */
	    DSMSerialSensor* sens = new DSMSerialSensor();
	    sens->setDeviceName(dev);

	    sens->setId(iarg);

	    sens->setBaudRate(115200);

	    sens->setMessageSeparator("\n");
	    sens->setMessageSeparatorAtEOM(true);
	    sens->setMessageLength(0);

	    sens->setPromptRate(IRIG_1_HZ);
	    sens->setPromptString("hitme\n");
	    sens->setScanfFormat("%*s%f%x");

	    std::cerr << "doing sens->open" << std::endl;
	    sens->open(O_RDWR);

/* Add the SampleClient to the sensor. It will receive all the samples */

	    sens->addSampleClient(&test);

/* Now your sensor to the PortSelectorTestor, and you should start
   to see samples being received by the SampleClient.
   When the PortSelectorTestor is destroyed, it will call the
   sensor destructors.
*/

	    handler->addSensorPort(sens);
	}
    }
    catch (atdUtil::IOException& ioe) {
      std::cerr << ioe.what() << std::endl;
      throw atdUtil::Exception(ioe.what());
    }

    try {
	handler->join();
    }
    catch(atdUtil::Exception& e) {
        cerr << "join exception: " << e.what() << endl;
	return 1;
    }
    delete handler;
    return 0;
}
