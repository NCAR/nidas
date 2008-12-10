// #define DEBUGFILT
/* ck_a2d.cc

   User space code that exercises ioctls on the a2d_driver module.

   Original Author: Grant Gray

   Copyright 2005 UCAR, NCAR, All Rights Reserved


   Revisions:

     $LastChangedRevision$
         $LastChangedDate$
           $LastChangedBy$
                 $HeadURL$
*/


#ifdef NEEDED
#include <fcntl.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <climits>
#include <iostream>
#include <iomanip>
#include <bits/posix1_lim.h>
#include <sys/select.h>

#include <ioctl_fifo.h>

#endif

#include <nidas/linux/ncar_a2d.h>
#include <nidas/dynld/raf/DSMAnalogSensor.h>

using namespace std;
using namespace nidas::core;
using namespace nidas::core;

namespace n_u = nidas::util;

int main(int argc, char** argv)
{
  int ii, i;
  int sleepytime = 1;
  int printmodulus = 1;
  int sample_rate = 500;
  int firstchan = 0;
  int lastchan = 7;

  if(argc < 2)
  {
	printf("\n\nUsage:\nck_a2d <#cycles> [print interval] [rate] [firstchan] [lastchan]\n");
	printf("Defaults:\nprint interval=%d, rate=%d, firstchan=%d,lastchan=%d\n",
		printmodulus,sample_rate,firstchan,lastchan);
		
	printf("\nExample:\nck_a2d 8 2 will do 8 cycles printing every other cycle.\n\n");
	exit(0);
  }

  sleepytime = atoi(argv[1]);
  if(sleepytime < 0)sleepytime = 1;

  if(argc > 2)
  {
	sleepytime += 1;	// Bump this to print last one
	printmodulus = atoi(argv[2]);
  	if(printmodulus > sleepytime)printmodulus = sleepytime;
  }

  if(argc > 3) sample_rate = atoi(argv[3]);
  if(argc > 4) firstchan = atoi(argv[4]);
  if(argc > 5) lastchan = atoi(argv[5]);

  cerr << endl << endl;
  cerr << "----CK_A2D----" << endl; 
  cerr << __FILE__ << ": Creating sensor class ..." << endl;

  nidas::dynld::raf::DSMAnalogSensor sensor;
  sensor.setDeviceName("/dev/dsma2d0");

  SampleTag* sample = new SampleTag();
  sample->setSensorId(0);
  sample->setSampleId(0);
  sample->setDSMId(0);
  sample->setRate(sample_rate);

  for (int i = firstchan; i <= lastchan; i++) {
      Variable* var = new Variable();
      ostringstream ost;
      ost << "c" << i;
      var->setName(ost.str());
      var->setUnits("V");

      ParameterT<float>* gain = new ParameterT<float>;
      gain->setName("gain");
      gain->setValue(1.0);
      var->addParameter(gain);

      ParameterT<bool>* bipolar = new ParameterT<bool>;
      bipolar->setName("bipolar");
      bipolar->setValue(true);
      var->addParameter(bipolar);

      ParameterT<int>* chan = new ParameterT<int>;
      chan->setName("channel");
      chan->setValue(i);
      var->addParameter(chan);

      sample->addVariable(var);
  }
  sensor.addSampleTag(sample);

  cerr << "opening" << endl;

  try {
    sensor.open(O_RDONLY);
  }
  catch (n_u::IOException& ioe) {
    cerr << ioe.what() << endl;    
    return 1;
  }

  cerr << __FILE__ << ": Up Fifo opened" << endl;

  // sample will contain an id and NUM_NCAR_A2D_CHANNELS values.
  const size_t MAX_DATA_SIZE = (NUM_NCAR_A2D_CHANNELS + 1) * sizeof(short);

  dsm_sample* buf = (dsm_sample*) malloc(SIZEOF_DSM_SAMPLE_HEADER +
  	MAX_DATA_SIZE);

  dsm_sample_time_t timetagLast = 0;
  for(i = 0; i <  sleepytime; i++)
  {
	size_t lread;

	// read header (timetag and sample size)
	lread = sensor.read(buf, SIZEOF_DSM_SAMPLE_HEADER);
	assert(lread == SIZEOF_DSM_SAMPLE_HEADER);
	// read data
	if (buf->length > MAX_DATA_SIZE) {
	    cerr << "sample length=" << buf->length <<
	    	" exceeds " << MAX_DATA_SIZE << endl;
	    break;
	}
	lread = sensor.read(buf->data, buf->length);
	assert(lread == buf->length);

//	cerr << "lread=" << lread << endl;

	if(i%printmodulus == 0)
	{	
		printf("\n\nindex = %6d\n", i);
		printf("time(msec)=%8d, dt=%d\n",
		    buf->timetag, 
		    (i > 0 ?
		    	(signed)(buf->timetag) - (signed)timetagLast : 0));

		short* dp = (short*) buf->data;
                for(ii = 0; ii < (signed)(buf->length / sizeof(short)); ii++)
                {
                        printf("%05d  ", *dp++);
                }
                printf("\n");	
	}
	timetagLast = buf->timetag;
  }

  sensor.close();

}

