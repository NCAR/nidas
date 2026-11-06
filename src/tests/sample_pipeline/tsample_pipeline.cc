#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/core/SamplePipeline.h>
#include <nidas/core/SampleSourceSupport.h>
#include <nidas/core/SampleClient.h>
#include <nidas/core/Sample.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/MutexCount.h>

#include <time.h>

using namespace nidas::core;

using nidas::util::Thread;
using nidas::util::Cond;
using nidas::util::Mutex;
using nidas::util::Synchronized;
using nidas::util::MutexCount;
using nidas::util::Exception;
using nidas::util::getSystemTime;

namespace {

const int NSAMPLES = 10;

// How long to wait for something that should happen.
const long long WAIT_TIMEOUT_USECS = 5 * USECS_PER_SEC;

// How long to watch for something that should not happen.
const long BLOCK_INTERVAL_MSECS = 250;


void
sleepMsecs(long msecs)
{
    struct timespec ts = { msecs / MSECS_PER_SEC,
                           (msecs % MSECS_PER_SEC) * NSECS_PER_MSEC };
    ::nanosleep(&ts, 0);
}


/**
 * Poll until done() returns true, returning false on timeout.  Cond has
 * no timed wait, so this stands in for one.
 */
template <typename Predicate>
bool
waitUntil(Predicate done)
{
    dsm_time_t end = getSystemTime() + WAIT_TIMEOUT_USECS;
    while (!done())
    {
        if (getSystemTime() > end)
            return false;
        sleepMsecs(10);
    }
    return true;
}


/**
 * SampleClient whose receive() blocks on the first sample until
 * release() is called.
 */
class BlockingClient: public SampleClient
{
public:
    BlockingClient(bool blocking=true):
        _cond(), _received(0), _blocking(blocking)
    {}

    bool receive(const Sample*) throw()
    {
        Synchronized lock(_cond);
        ++_received;
        while (_blocking)
            _cond.wait();
        return true;
    }

    void flush() throw() {}

    void release()
    {
        Synchronized lock(_cond);
        _blocking = false;
        _cond.broadcast();
    }

    /**
     * Wait until at least n samples have been received, returning false
     * on timeout.
     */
    bool waitForReceived(int n)
    {
        return waitUntil([this, n] { return received() >= n; });
    }

    int received()
    {
        Synchronized lock(_cond);
        return _received;
    }

private:
    Cond _cond;
    int _received;
    bool _blocking;
};


/**
 * Thread which distributes NSAMPLES samples from a SampleSourceSupport,
 * counting how many distribute() calls have returned.
 */
class Producer: public Thread
{
public:
    Producer(SampleSourceSupport& source):
        Thread("Producer"), _source(source), _distributed(0)
    {}

    int run() throw(Exception)
    {
        dsm_time_t tt = getSystemTime();
        for (int i = 0; i < NSAMPLES; ++i)
        {
            SampleT<char>* samp = getSample<char>(1);
            samp->setId(SET_DSM_ID(SET_SPS_ID(0, 10), 1));
            // Space the samples one second apart so that a SampleSorter
            // with a short length passes them on.
            samp->setTimeTag(tt + i * USECS_PER_SEC);
            samp->getDataPtr()[0] = (char)i;
            _source.distribute(samp);
            ++_distributed;
        }
        return RUN_OK;
    }

    /**
     * Wait for all samples to be distributed, returning false on timeout.
     */
    bool waitForDone()
    {
        return waitUntil([this] { return distributed() >= NSAMPLES; });
    }

    int distributed()
    {
        return _distributed.value();
    }

private:
    SampleSourceSupport& _source;
    MutexCount<int> _distributed;
};


/**
 * Connect a source and a BlockingClient through a SamplePipeline, either
 * through the raw or the processed side of the pipeline.  Return true if
 * the producer was held up while the client was blocked in receive().
 */
bool
clientCannotBlockProducer(float sorterLength, bool raw, bool blocking=true)
{
    BlockingClient client(blocking);
    SampleSourceSupport source(raw);
    SamplePipeline pipeline;

    pipeline.setRawSorterLength(sorterLength);
    pipeline.setProcSorterLength(sorterLength);
    pipeline.connect(&source);
    if (raw)
        pipeline.getRawSampleSource()->addSampleClient(&client);
    else
        pipeline.addSampleClient(&client);

    Producer producer(source);
    producer.start();

    // The client must see the first sample, otherwise samples are not
    // making it through the pipeline at all.  Release the client before
    // giving up, so the producer can be joined.
    bool started = client.waitForReceived(1);
    if (!started)
    {
        client.release();
        producer.join();
    }
    BOOST_REQUIRE(started);

    bool blocked = !producer.waitForDone();
    BOOST_TEST_MESSAGE("sorterLength=" << sorterLength << " raw=" << raw
                       << ": distributed " << producer.distributed()
                       << " while client blocked on sample "
                       << client.received());
    BOOST_TEST(!blocked, "Producer should not be blocked by client");
    BOOST_CHECK_EQUAL(producer.distributed(), NSAMPLES);
    // Only the first sample reaches the client while it is blocked.
    if (blocking)
        BOOST_CHECK_EQUAL(client.received(), 1);

    // Unblocking the client allows it to receive the rest of the samples.
    client.release();
    producer.join();
    pipeline.flush();
    BOOST_CHECK_EQUAL(client.received(), NSAMPLES);

    if (raw)
        pipeline.getRawSampleSource()->removeSampleClient(&client);
    else
        pipeline.removeSampleClient(&client);
    pipeline.disconnect(&source);
    pipeline.join();
    return !blocked;
}

}


BOOST_AUTO_TEST_CASE(test_zero_length_raw_sorter_nonblocking)
{
    // Producer cannot block whether or not the client is blocking.
    BOOST_CHECK(clientCannotBlockProducer(0, true, false));
    BOOST_CHECK(clientCannotBlockProducer(0, true, true));
}

BOOST_AUTO_TEST_CASE(test_zero_length_proc_sorter_nonblocking)
{
    BOOST_CHECK(clientCannotBlockProducer(0, false, false));
    BOOST_CHECK(clientCannotBlockProducer(0, false, true));
}

// The control cases: with a SampleSorter, the source is decoupled from the
// client by the sorter thread, so a blocked client does not block the
// source.  This shows the tests above can tell the difference.

BOOST_AUTO_TEST_CASE(test_nonzero_length_raw_sorter_nonblocking)
{
    BOOST_CHECK(clientCannotBlockProducer(0.1, true, false));
    BOOST_CHECK(clientCannotBlockProducer(0.1, true, true));
}

BOOST_AUTO_TEST_CASE(test_nonzero_length_proc_sorter_nonblocking)
{
    BOOST_CHECK(clientCannotBlockProducer(0.1, false, false));
    BOOST_CHECK(clientCannotBlockProducer(0.1, false, true));
}
