/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************
*/

/*
 * tests on orion:
 * 100 cycles through loop, 2 second sleep each, no mutex locking
 * in Sample::freeReference/holdReference:
 *nsampout  nsmall  nmedium nsamples    sort1  client1    sort2  client2
    13410     1209     2779 17270001    13003 17256998    13003 17256998
    real    3m20.918s
    user    3m4.956s
    sys     1m24.971s

    17.27 million samples, user+sys= 04 minutes, 30 seconds
    No assertion failures.
    2nd test: 17.43 million samples, user+sys=04:30

 * 100 cycles through loop, 2 second sleep each, mutex locking
 * in Sample::freeReference/holdReference:
 *nsampout  nsmall  nmedium nsamples    sort1  client1    sort2  client2
    11948     2025     4488 16600001    11948 16588053    11948 16588053

    real    3m20.856s
    user    3m5.308s
    sys     1m24.844s

    16.59 million samples, user+sys= 04 minutes, 30 seconds
    No assertion failures.
    2nd test:   16.56 million samples, user+sys= 04:30

 * Test on 400Mhz viper:
 * 100 cycles through loop, 2 second sleep each, mutex locking
 * in Sample::freeReference/holdReference:
 *nsampout  nsmall  nmedium nsamples    sort1  client1    sort2  client2

      284       73      183   360001      284   359717      282   359719
       
       real    3m25.431s
       user    1m32.760s
       sys     0m3.260s

	360K samples, user+sys= 01:36


  * 400MHz viper: no mutexes
      248       84      188   350001      248   349753      247   349754
 
    real    3m24.051s
    user    1m33.010s
    sys     0m3.090s

	350K samples, user+sys= 01:36

	orion provides 46x more throughput than viper


 * The assertions would be tripped if the reference counting gets
 * messed up.
 *
 * So the lack of assertion failures when not using mutexes to protect
 * the  reference counts indicates that the mutexes aren't necessary,
 * even on a multiprocessor system.  However the mutexes only incur
 * 4.5% throughput penalty on orion, so it doesn't hurt much to use them.
 *
 * Next: see if atomic operations improve anything.
 */

#include <SampleSource.h>
#include <SampleClient.h>
#include <SampleSorter.h>
#include <DSMTime.h>
#include <atdUtil/Thread.h>

#include <iostream>
#include <iomanip>

using namespace dsm;
using namespace std;

class TestSource: public SampleSource, public atdUtil::Thread
{
public:
    TestSource():Thread("TestSource"),nsamples(0) {}

    int run() throw(atdUtil::Exception);

    unsigned long nsamples;

};

int TestSource::run() throw(atdUtil::Exception)
{
    for (;;) {
	// random sizes between 1 and 100
        SampleT<char>* samp = getSample<char>(random() / (RAND_MAX / 100)+1);

	dsm_sys_time_t tnow = getCurrentTimeInMillis();
	// add 10 milliseconds of noise
	samp->setTimeTag(
		(tnow + random() / (RAND_MAX / 100) ) % MSECS_PER_DAY);
	samp->setId(0x0010);
	distribute(samp);
	samp->freeReference();
	if (!(nsamples++ % 10000)) testCancel();
    }
}

class TestClient: public SampleClient
{
public:
    TestClient():nsamples(0) {}

    bool receive(const Sample *s) throw();
    int nsamples;
};

bool TestClient::receive(const Sample *s) throw()
{
    nsamples++;
    return true;
}


class SampleTest 
{
public:
    SampleTest():sorter1(100),sorter2(100) {}

    void test() throw(atdUtil::Exception);
    void print();
    void header();

protected:

    TestSource source;

    SampleSorter sorter1;
    TestClient client1;

    SampleSorter sorter2;
    TestClient client2;

};

void SampleTest::header()
{
    cout <<
    " nsampout   nsmall  nmedium nsamples    sort1  client1    sort2  client2" <<
    	endl;
}

void SampleTest::print()
{
    SamplePool<SampleT<char> >* pool = 
    	SamplePool<SampleT<char> >::getInstance();

    cout <<
    	setw(9) << pool->nsamplesOut << 
	setw(9) << pool->nsmall <<
	setw(9) << pool->nmedium <<
	setw(9) << source.nsamples << 
	setw(9) << sorter1.size() << 
	setw(9) << client1.nsamples << 
	setw(9) << sorter2.size() << 
	setw(9) << client2.nsamples << 
    endl;
}

void SampleTest::test() throw(atdUtil::Exception)
{

    sorter1.addSampleClient(&client1);
    source.addSampleClient(&sorter1);

    sorter2.addSampleClient(&client2);
    source.addSampleClient(&sorter2);

    // sorter1.setDebug(true);

    sorter1.start();
    sorter2.start();
    source.start();

    header();

    for (int i = 0; i < 100; i++) {
	sleep(2);
	print();
    }

    source.cancel();
    source.join();

    sorter1.interrupt();
    sorter1.join();

    sorter2.interrupt();
    sorter2.join();

    print();

    assert(source.nsamples == sorter1.size() + client1.nsamples);
    assert(source.nsamples == sorter2.size() + client2.nsamples);

}

int main(int argc, char** argv)
{
    SampleTest test;
    test.test();
}

