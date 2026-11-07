#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/core/NidasApp.h>
#include <nidas/core/SamplePipeline.h>
#include <nidas/core/SampleSorter.h>
#include <nidas/core/SampleSourceSupport.h>
#include <nidas/core/SampleClient.h>
#include <nidas/core/Sample.h>
#include <nidas/util/UTime.h>
#include <nidas/util/Thread.h>
#include <nidas/util/ThreadSupport.h>
#include <nidas/util/MutexCount.h>

#include <sys/resource.h>
#include <time.h>

#include <algorithm>
#include <vector>

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
 * SampleClient which records the time tags of the samples it receives, in
 * the order received.  If constructed with blocking true, receive() records
 * the first sample and then blocks until release() is called.
 */
class TestSampleClient: public SampleClient
{
public:
    TestSampleClient(bool blocking=false):
        _cond(), _times(), _blocking(blocking)
    {}

    bool receive(const Sample* samp) throw()
    {
        Synchronized lock(_cond);
        VLOG(("") << "TestSampleClient received sample " << _times.size());
        _times.push_back(samp->getTimeTag());
        while (_blocking)
            _cond.wait();
        return true;
    }

    void flush() throw() {}

    /**
     * Unblock receive(), and do not block in it again.
     */
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
        return _times.size();
    }

    /**
     * Return the time tags received so far, in the order received.
     */
    std::vector<dsm_time_t> times()
    {
        Synchronized lock(_cond);
        return _times;
    }

private:
    Cond _cond;
    std::vector<dsm_time_t> _times;
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
        dsm_time_t tt = getSystemTime() - 10*USECS_PER_SEC;
        for (int i = 0; i < NSAMPLES; ++i)
        {
            VLOG(("") << "Producer sending sample " << i);
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
 * Connect a source and a TestSampleClient through a SamplePipeline, either
 * through the raw or the processed side of the pipeline.  Return true if
 * the producer was held up while the client was blocked in receive().
 */
bool
clientBlocksProducer(bool realtime, float sorterLength,
                     bool raw, bool blocking=true)
{
    TestSampleClient client(blocking);
    SampleSourceSupport source(raw);
    SamplePipeline pipeline;

    // if not real-time, then nothing is buffered and client will definitely
    // block the client.
    pipeline.setRealTime(realtime);
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
                       << ", client received "
                       << client.received());
    if (realtime || sorterLength > 0)
    {
        // producer should have finished sending all samples without being
        // blocked by the client.
        BOOST_TEST(!blocked, "Producer should not be blocked by client");
        BOOST_CHECK_EQUAL(producer.distributed(), NSAMPLES);
    }
    // Only the first sample reaches the client while it is blocked.
    if (blocking)
        BOOST_CHECK_EQUAL(client.received(), 1);

    // Unblocking the client allows it to receive the rest of the samples.
    client.release();
    producer.join();
    // all should have been distributed by the producer by now, whether
    // synchornized with client or not.
    BOOST_CHECK_EQUAL(producer.distributed(), NSAMPLES);
    // flush() should be synchronized with consumer thread, so that it doesn't
    // return until consumer has received all the samples.
    pipeline.flush();
    BOOST_CHECK_EQUAL(client.received(), NSAMPLES);

    if (raw)
        pipeline.getRawSampleSource()->removeSampleClient(&client);
    else
        pipeline.removeSampleClient(&client);
    pipeline.disconnect(&source);
    pipeline.join();
    return blocked;
}

}


// Test that the producer cannot block whether or not the client is blocking,
// for both raw and processed sorters with zero and nonzero lengths, as long
// as the pipeline is in real-time mode.

BOOST_AUTO_TEST_CASE(test_zero_length_raw_sorter_nonblocking)
{
    BOOST_CHECK(!clientBlocksProducer(true, 0, true, false));
    BOOST_CHECK(!clientBlocksProducer(true, 0, true, true));
}

BOOST_AUTO_TEST_CASE(test_zero_length_proc_sorter_nonblocking)
{
    BOOST_CHECK(!clientBlocksProducer(true, 0, false, false));
    BOOST_CHECK(!clientBlocksProducer(true, 0, false, true));
}

BOOST_AUTO_TEST_CASE(test_nonzero_length_raw_sorter_nonblocking)
{
    BOOST_CHECK(!clientBlocksProducer(true, 0.1, true, false));
    BOOST_CHECK(!clientBlocksProducer(true, 0.1, true, true));
}

BOOST_AUTO_TEST_CASE(test_nonzero_length_proc_sorter_nonblocking)
{
    BOOST_CHECK(!clientBlocksProducer(true, 0.1, false, false));
    BOOST_CHECK(!clientBlocksProducer(true, 0.1, false, true));
}

// now check that producer is blocked if not real-time.  this is behavior that
// applications like data_stats rely on.

BOOST_AUTO_TEST_CASE(test_not_realtime_not_sorting_blocks)
{
    BOOST_CHECK(clientBlocksProducer(false, 0, true, true));
}


namespace {

const dsm_sample_id_t SAMPLE_ID = SET_DSM_ID(SET_SPS_ID(0, 10), 1);


SampleT<char>*
makeSample(dsm_time_t tt)
{
    SampleT<char>* samp = getSample<char>(1);
    samp->setId(SAMPLE_ID);
    samp->setTimeTag(tt);
    samp->getDataPtr()[0] = 0;
    return samp;
}


/**
 * Return the user plus system CPU time used by this process, in seconds.
 */
double
cpuSecs()
{
    struct rusage ru;
    ::getrusage(RUSAGE_SELF, &ru);
    return ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1.0e6 +
        ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1.0e6;
}


/**
 * Connect a raw or processed source and a TestSampleClient through a
 * SamplePipeline with the given sorter length, and disconnect them again
 * when done.
 */
class RecordingPipeline
{
public:
    RecordingPipeline(float sorterLength, bool raw):
        client(), source(raw), pipeline(), _raw(raw)
    {
        pipeline.setRawSorterLength(sorterLength);
        pipeline.setProcSorterLength(sorterLength);
        pipeline.connect(&source);
        if (_raw)
            pipeline.getRawSampleSource()->addSampleClient(&client);
        else
            pipeline.addSampleClient(&client);
    }

    ~RecordingPipeline()
    {
        if (_raw)
            pipeline.getRawSampleSource()->removeSampleClient(&client);
        else
            pipeline.removeSampleClient(&client);
        pipeline.disconnect(&source);
        pipeline.join();
    }

    void send(dsm_time_t tt)
    {
        source.distribute(makeSample(tt));
    }

    TestSampleClient client;
    SampleSourceSupport source;
    SamplePipeline pipeline;

private:
    bool _raw;
};


/**
 * Return time tags spaced 100 msecs apart, in an arrival order where each
 * block of five is reversed, so no sample arrives more than 400 msecs
 * behind the latest sample before it.
 */
std::vector<dsm_time_t>
shuffledTimes(dsm_time_t t0)
{
    const int NTIMES = 50;
    const int BLOCK = 5;
    std::vector<dsm_time_t> times;
    for (int i = 0; i < NTIMES; ++i)
        times.push_back(t0 + i * 100 * USECS_PER_MSEC);
    for (int i = 0; i < NTIMES; i += BLOCK)
        std::reverse(times.begin() + i, times.begin() + i + BLOCK);
    return times;
}


/**
 * Send the shuffled time tags through a pipeline with the given sorter
 * length, and return the time tags in the order the client received them.
 */
std::vector<dsm_time_t>
pipeShuffledTimes(float sorterLength, bool raw,
                  const std::vector<dsm_time_t>& sent)
{
    RecordingPipeline rp(sorterLength, raw);
    for (dsm_time_t tt : sent)
        rp.send(tt);
    rp.pipeline.flush();
    return rp.client.times();
}


void
checkSortedWithinSorterLength(bool raw)
{
    // 400 msecs of disorder is within a 1 second sorter, so the samples
    // must come out completely sorted.
    std::vector<dsm_time_t> sent = shuffledTimes(getSystemTime());
    std::vector<dsm_time_t> expected(sent);
    std::sort(expected.begin(), expected.end());
    BOOST_REQUIRE(sent != expected);

    std::vector<dsm_time_t> received = pipeShuffledTimes(1.0, raw, sent);
    BOOST_CHECK_EQUAL_COLLECTIONS(received.begin(), received.end(),
                                  expected.begin(), expected.end());
}


void
checkZeroLengthKeepsArrivalOrder(bool raw)
{
    // With no sorting, the samples come out in the order they went in.
    std::vector<dsm_time_t> sent = shuffledTimes(getSystemTime());

    std::vector<dsm_time_t> received = pipeShuffledTimes(0, raw, sent);
    BOOST_CHECK_EQUAL_COLLECTIONS(received.begin(), received.end(),
                                  sent.begin(), sent.end());
}


void
checkLateSampleBeyondSorterLength(bool raw)
{
    const dsm_time_t t0 = getSystemTime();
    const dsm_time_t sec = USECS_PER_SEC;

    // With a 1 second sorter, the sample at 10 ages off the samples at 0
    // and 2.  Once the client has them, a sample at 1 is too late to be
    // sorted in front of 2, and is passed on as is.
    {
        RecordingPipeline rp(1.0, raw);
        rp.send(t0);
        rp.send(t0 + 2 * sec);
        rp.send(t0 + 10 * sec);
        BOOST_REQUIRE(rp.client.waitForReceived(2));
        rp.send(t0 + 1 * sec);
        BOOST_REQUIRE(rp.client.waitForReceived(3));
        rp.pipeline.flush();

        std::vector<dsm_time_t> received = rp.client.times();
        std::vector<dsm_time_t> expected =
            { t0, t0 + 2 * sec, t0 + 1 * sec, t0 + 10 * sec };
        BOOST_CHECK_EQUAL_COLLECTIONS(received.begin(), received.end(),
                                      expected.begin(), expected.end());
    }

    // The same samples fit within a 20 second sorter, and nothing ages off
    // until the flush, so they come out sorted.
    {
        RecordingPipeline rp(20.0, raw);
        rp.send(t0);
        rp.send(t0 + 2 * sec);
        rp.send(t0 + 10 * sec);
        rp.send(t0 + 1 * sec);
        // Nothing is old enough to be passed on yet.
        sleepMsecs(BLOCK_INTERVAL_MSECS);
        BOOST_CHECK_EQUAL(rp.client.times().size(), 0u);
        rp.pipeline.flush();

        std::vector<dsm_time_t> received = rp.client.times();
        std::vector<dsm_time_t> expected =
            { t0, t0 + 1 * sec, t0 + 2 * sec, t0 + 10 * sec };
        BOOST_CHECK_EQUAL_COLLECTIONS(received.begin(), received.end(),
                                      expected.begin(), expected.end());
    }
}

}


BOOST_AUTO_TEST_CASE(test_raw_sorter_sorts_within_length)
{
    checkSortedWithinSorterLength(true);
}

BOOST_AUTO_TEST_CASE(test_proc_sorter_sorts_within_length)
{
    checkSortedWithinSorterLength(false);
}

BOOST_AUTO_TEST_CASE(test_zero_length_raw_sorter_keeps_arrival_order)
{
    checkZeroLengthKeepsArrivalOrder(true);
}

BOOST_AUTO_TEST_CASE(test_zero_length_proc_sorter_keeps_arrival_order)
{
    checkZeroLengthKeepsArrivalOrder(false);
}

BOOST_AUTO_TEST_CASE(test_raw_sorter_does_not_sort_beyond_length)
{
    checkLateSampleBeyondSorterLength(true);
}

BOOST_AUTO_TEST_CASE(test_proc_sorter_does_not_sort_beyond_length)
{
    checkLateSampleBeyondSorterLength(false);
}


// A sorter holding samples which have not aged off yet has nothing to do
// until another sample arrives, so its thread should be waiting, not
// spinning.
BOOST_AUTO_TEST_CASE(test_idle_sorter_does_not_spin)
{
    const long IDLE_MSECS = 1000;
    RecordingPipeline rp(10.0, true);
    dsm_time_t t0 = getSystemTime();
    rp.send(t0);
    rp.send(t0 + USECS_PER_MSEC);

    double cpu0 = cpuSecs();
    sleepMsecs(IDLE_MSECS);
    double cpu = cpuSecs() - cpu0;
    BOOST_TEST_MESSAGE("cpu used by idle sorter: " << cpu << " secs in "
                       << IDLE_MSECS << " msecs");
    // A spinning sorter uses close to all of the interval.
    BOOST_CHECK_LT(cpu, 0.1 * IDLE_MSECS / MSECS_PER_SEC);
    BOOST_CHECK_EQUAL(rp.client.times().size(), 0u);
    rp.pipeline.flush();
    BOOST_CHECK_EQUAL(rp.client.times().size(), 2u);
}


bool init_unit_test()
{
    return true;
}


// Parse the nidas logging options, such as --log debug, then pass the rest
// of the arguments to the boost test framework.
int main(int argc, char* argv[])
{
    NidasApp napp(argv[0]);
    napp.enableArguments(napp.loggingArgs());
    napp.allowUnrecognized(true);
    ArgVector args = napp.parseArgs(argc, argv);
    // NidasApp logs at info by default, but the sorter tests start and stop
    // thousands of threads which each log at info, so unless a log level was
    // requested, only log warnings and errors.
    if (!napp.LogConfig.specified())
    {
        nidas::util::Logger::getInstance()->setScheme
            (nidas::util::LogScheme().addConfig
             (nidas::util::LogConfig("warning")));
    }
    DLOG(("main entered, args parsed, debugging enabled..."));

    NidasAppArgv left(argv[0], args);
    return boost::unit_test::unit_test_main(&init_unit_test,
                                            left.argc, left.argv);
}
