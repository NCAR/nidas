/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

    A sensor test-bed.  All the details of the select() system call,
    the buffered reads from the data fifos, the assembly into
    samples and signal handling are taken care of.  One can
    use this to check that the samples coming from the kernel
    side are what is expected.

*/


#include <PortSelectorTest.h>
#include <TestSampleClient.h>
#include <DSMSerialSensor.h>

#include <dsm_serial.h>

#include <iostream>

using namespace std;
using namespace dsm;


int main(int argc, char** argv)
{
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << "/dev/dsmserX ..." << endl;
	return 1;
    }

    PortSelectorTest* handler = PortSelectorTest::createInstance();

    handler->start();

/* A SampleClient that just prints some of the sample fields to cerr */

    TestSampleClient test;

    try {
	for (int iarg = 1; iarg < argc; iarg++) {
	    const char* dev = argv[iarg];


/* Substitute your sensor instantiation here ********************** */
	    DSMSerialSensor* sens = new DSMSerialSensor(dev);

	    sens->setId(iarg);

	    sens->setBaudRate(115200);
	    std::cerr << "doing sens->open" << std::endl;
	    sens->open(O_RDWR);
	    struct dsm_serial_record_info recinfo;
	    recinfo.sep[0] = '\n';
	    recinfo.sepLen = 1;
	    recinfo.atBOM = 0;
	    recinfo.recordLen = 0;
	    sens->ioctl(DSMSER_SET_RECORD_SEP,&recinfo,sizeof(recinfo));

	    struct dsm_serial_prompt prompt;
	    strcpy(prompt.str,"hello\n");
	    prompt.len = 6;
	    prompt.rate = IRIG_1_HZ;
	    sens->ioctl(DSMSER_SET_PROMPT,&prompt,sizeof(prompt));

	    sens->ioctl(DSMSER_START_PROMPTER,(const void*)0,0);

/* Add the SampleClient to the sensor. It will receive all the samples */

	    sens->addSampleClient(&test);

/* Now add a pointer to your sensor to the PortSelectorTestor,
   and things will start running
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
