/*
 ******************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

 ******************************************************************
*/


#include <dsm_serial_fifo.h>
#include <dsm_serial.h>
#include <DSMSerialSensor.h>
#include <RTL_DevIoctlStore.h>
#include <asm/ioctls.h>

#include <iostream>

using namespace std;

DSMSerialSensor::DSMSerialSensor(const string& nameArg) :
    RTL_DSMSensor(nameArg)
{

}

DSMSerialSensor::~DSMSerialSensor() {
    try {
	close();
    }
    catch(atdUtil::IOException& ioe) {
      cerr << ioe.what() << endl;
    }
}
void DSMSerialSensor::open(int flags) throw(atdUtil::IOException)
{
  
    devIoctl = RTL_DevIoctlStore::getInstance()->getDevIoctl(prefix,portNum);
    if (devIoctl) devIoctl->open();

    ioctl(DSMSER_OPEN,&flags,sizeof(flags));
    cerr << "DSMSER_OPEN done" << endl;

    int accmode = flags & O_ACCMODE;

    if (accmode == O_RDONLY || accmode == O_RDWR) {
	infifofd = ::open(inFifoName.c_str(),O_RDONLY);
	if (infifofd < 0) throw atdUtil::IOException(inFifoName,"open",errno);
    }
    cerr << inFifoName << " opened" << endl;

    if (accmode == O_WRONLY || accmode == O_RDWR) {
	outfifofd = ::open(outFifoName.c_str(),O_WRONLY);
	if (outfifofd < 0) throw atdUtil::IOException(outFifoName,"open",errno);
    }
    cerr << outFifoName << " opened" << endl;

#ifdef DEBUG
    cerr << "sizeof(struct termios)=" << sizeof(struct termios) << endl;
    cerr << "termios=" << hex << getTermiosPtr() << endl;
    cerr << "c_iflag=" << &(getTermiosPtr()->c_iflag) << ' ' << getTermiosPtr()->c_iflag << endl;
    cerr << "c_oflag=" << &(getTermiosPtr()->c_oflag) << ' ' << getTermiosPtr()->c_oflag << endl;
    cerr << "c_cflag=" << &(getTermiosPtr()->c_cflag) << ' ' << getTermiosPtr()->c_cflag << endl;
    cerr << "c_lflag=" << &(getTermiosPtr()->c_lflag) << ' ' << getTermiosPtr()->c_lflag << endl;
    cerr << "c_line=" << (void *)&(getTermiosPtr()->c_line) << endl;
    cerr << "c_cc=" << (void *)&(getTermiosPtr()->c_cc[0]) << endl;

    cerr << "c_iflag=" << iflag() << endl;
    cerr << "c_oflag=" << oflag() << endl;
    cerr << "c_cflag=" << cflag() << endl;
    cerr << "c_lflag=" << lflag() << endl;
    cerr << dec;
#endif

    ioctl(DSMSER_TCSETS,getTermiosPtr(),SIZEOF_TERMIOS);

}

void DSMSerialSensor::close() throw(atdUtil::IOException)
{
    cerr << "doing DSMSER_CLOSE" << endl;
    ioctl(DSMSER_CLOSE,(const void*)0,0);
    RTL_DSMSensor::close();

}
