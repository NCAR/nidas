# NIDAS Sample Time Tags

2020, Copyright University Corporation for Atmospheric Research

## Introduction

Many sensors sampled by NIDAS run asynchronously, with their own internal processor clock, which is not conditioned by any reference.

As samples are received from these sensors, the acquisition system assigns time tags to the samples, using its own clock.  This system clock is typically conditioned with NTP, and so has a high accuracy, often better tthan 50 microseconds relative to an absolute GPS reference.

However, the assignment of time tags suffers from several sources of latency. With a highly accurate system clock, we can assume that the assigned time tags are always later, never earlier, than the representative time of the sample.  

This latency has several sources:

 * latency in responding to interrupts
 * interrupts may also be systematically late. For example a serial interrupt generally does
not occur until the UART has detected inactivity on the serial line
 * acquisition system load causing delays in reading available data from buffers

This last latency can sometimes be significant, several seconds or more, which would seem to completely invalidate the time tags.

However, generally this latency is quite random, and for sensors with a known and consistent
reporting rate, one can determine when the latency is the smallest.  Using an
artificial sequence of time tags, starting with the first received sample time tag and
differing by the fixed sensor reporting interval, dt, compute the difference of each raw
time tag with the associated artificial time tag.  The most negative difference over a period
of time gives one an estimate of when the system had the minimum latency for that period.
 One can shift the next artificial sequence of time tags by this difference, and continue
the process.  In this way one can get a good idea of what the time tags would have been
if the system latency was always at its minimum over that period.

So this method does not remove a constant, systematic latency in the assignment of time tags, but
appears to be effective in reducing the random latency jitter.

## Serial Sensor Samples

For senors with a serial port interface, NIDAS assigns time tags to samples by reading
the system clock right after the serial data is read. An estimate of the transmission
time of the first byte in the buffer is computed as the read time minus the number of
bytes read, times the transmission time of one byte: 

        Tfirst = Tread - Nbyte * Tbyte
        Tbyte = (Nd + Ns + Np) / baud

where Nd,Ns,Np are the number of data, stop and parity bits.

Then as samples are parsed from the input buffer, the transmission time
of the first byte in a sample is estimated as:

        Traw = Tfirst + Noffset * Tbyte

Where Noffset is the offset in the input buffer of the first byte
of the sample.
 
Each raw sample is then archived, with its associated raw time tag, Traw.

## TimetagAdjuster

If the configuration value of "ttadjust" is positive for a sample, and TimetagAdjuster is
supported in the sensor class, then the it is used in post-processing to generate
corrected time tags of processed samples.  

In the XML, ttadjust is an attribute of \<sample\>:

        <sample id="1" rate="50" scanfFormat="*%*2d%*2d%f" ttadjust="1">

Timetag adjusting is done in the nidas::core::CharacterSensor::process() method, and so any
classes derived from CharacterSensor that do not override process() can enable TimetagAdjuster.
The TimetagAdjustor has been added to the process() method of several sub-classes of
CharacterSensor, including ATIK_Sonic, CSAT3_Sonic, CSI_CRX_Binary and CSI_IRGA_Sonic
in the nidas::dynld::isff namespace.

The archived, raw time tags, Traw[i], are passed one at a time to the
TimetagAdjuster::adjust() method, as each sample is processed.

Each time adjust() is called, it computes an adjusted time tag, Tadj[I], using a
fixed time delta from a base time, T0:

        I++
        Tadj[I] = T0 + I * dt

This value of Tadj[I] is returned as the adjusted time tag for
the next Npts input time tags.

The trick is to figure out good values for T0, dt and Npts.

        Npts = max(5, rate)

So Npts will be at least 5 samples, and for rates > 5, it will be
the number of samples per second. In some cirumstances I is
allowed to exceed Npts as discussed below.

Initially

        dt = 1/rate

as configured for the sensor.  Later dt is updated periodically
using a running average of the observed dt's:

        dtobs = Traw[i] - Traw[i-1]

Initially, and after any large gap exceeding BIG_GAP_SECONDS, T0 is
reset to the original time tag passed to adjust():

        T0 = Traw[i]
        I = 0

and the unadjusted time tag

        Tadj[0] = T0 + 0 * dt = Traw[i]

is returned from adjust().

The difference between each measured and adjusted time tag is
computed:

        tdiff = Traw[i] - Tadj[I]

tdiff is an estimate of the error in Tadj[I].

If tdiff is ever negative, then because we assume Traw[i] are never
too early, then Tadj[I] and T0 must be too late, and are corrected
earlier:

        T0 += tdiff
        Tadj[I] += tdiff
        tdiff = 0

Note that since TimetagAdjuster does not buffer samples, it does not
correct any previous Tadj[I] for this negative tdiff. 

Over the next Npts input time tags, the minimum value of
tdiff, which is now non-negative, is computed.  This tdiffmin is then an
estimate of how much T0 was too early over that time.
 
The idea is that sometimes over the Npts the system latency
is small, and for those times, the raw time tags, Traw[i],
are quite close to the true sample time. Correcting
T0 forward

        T0 = Tadj[Npts] + tdiffmin

will give a better series of times for the next Npts.

## Periods of Bad Latency

Sometimes a DSM goes "catatonic", such that serial port reads are blocked
by some other system task. During these times there does not seem
to be any lost data, the serial driver is filling its buffer with
the received characters with no loss, but the buffers are not being
read by the DSM user-space acquisition process.

A good example of this problem is the Nov 11, 2020 Honeywell PPT data
taken on the C-130 by dsm319 during WCR-TEST, variables QCF (sample id 19,111),
ADIFR (19,121), and BDIFR(19,131).

As a result, for these times there will be a gap in the Traw[i],
of perhaps several seconds, followed by many samples with time tags
spaced very closely in time, differing by

        Traw[i] - Traw[i-1] =  Nsamp * Tbyte

where Nsamp is the number of bytes in a sample.
So for sensor such as a Honeywell PPT, with a sample size of 15 bytes, at a
baud rate of 19200 bits/sec, the differences between the original time tags will be:

        Traw[i] - Traw[i-1] =  15 * Tbyte = 0.0078 sec

where

        Tbyte = 10 / 19200 = 0.52 millisecond

As of Dec 2020, the read buffer size in the acquisition process was 2048 bytes.
For a buffer length of 2048 bytes, there may be as many as 2048/15=136 samples
after the gap with a delta-T of 0.0078 sec.

In this case TimetagAdjuster will keep assigning time tags over the Npts as usual
using the T0 determined from the of tdiffmin before the gap:

        Tadj[I] = T0 + I * dt

At the gap, tdiff jumps to a large value, and then steadily decreases over time, 
but might still be large once Npts have been processed.

To handle this situation, ttdjust checks whether tdiff is decreasing
(the current tdiff is smaller than the previous) and will "flywheel" after
I > Npts, without changing T0, until tdiff starts to increase. At that point
tdiffmin will likely be small, indicating the system has caught up, and the
adjustment over the next set of points should be small.

If there are more than 2048 bytes in the serial driver buffers, then two
or more buffer reads may then happen in quick succession once the DSM system
load has reduced sufficiently. Since the transmission time of the first byte
in the buffer is estimated by

        Tfirst = Tread - Nbyte * Tbyte

the Tfirst for the next buffer may be earlier than the last sample time in the
previous buffer.  The system does not allow backwards time tags, so the
time tags of samples in the next buffer be set to the previous time tag plus
one micro-second.  This will be seen as another gap in the data followed
by one micro-second delta-Ts.

To acount for this second situation, TimetagAdjuster will flywheel forward until
tdiff increases twice in a row.

It is probably wise to increase the read buffer size, for example to 4096 bytes.

## Prompted Sensors

After a prompt is sent the prompt thread uses the modulus of the current
time (Tnow) with the desired output delta-T, to compute the amount of time to sleep,
before the next prompt is sent, at a precision (not accuracy) of nano-seconds:

        sleeptime = dt - (Tnow % dt)

During times that the input latency is bad, the expected number of samples are seen
in the output, so it appears that the prompting thread is not being blocked to the
extent that output prompts are skipped. The prompts might be buffered by the serial
driver, but the symptoms of the input latency indicate that the serial driver is not
getting behind, since no characters are lost, indicating that the problem is with the
user-side read process.  This also indicates that the serial driver is also
not getting significantly behind in sending prompts, but more investigation would be good.

There is, of course, some latency jitter in the sending of the time tags, but
it doesn't seem to be as large as a sample delta-T.

## Logs

### Summary Log Message

At the end of processing, an INFO log message is printed for every sample with a TimetagAdjuster:

    INFO|ttadjust: dsm319:/dev/ttyS5(19,111): max late: 4.5827s, dt min,max:  0.02000,  0.02005, outdt min,max:  0.02,  0.12, rate cfg,obs,diff: 50.00, 49.99717,  0.00, maxgap:   4.51, #neg: 0, #pos: 2, #tot: 464986

 * max late: maximum amount of time that a raw time tag was adjusted earlier.
 * dt min,max: minimum and maximum values for the averaged dt that was used in the generation of adjusted time tags
 * outdt min max: minimum and maximum successive differences in the adjusted, output time tags
 * rate cfg,obs,diff: the configured sampling rate for the sensor, the observed rate and the difference
 * max gap: maximum gap seen in the data.
 * #neg: number of tdiffs < -dt/2
 * #pos: number of tdiffs > dt/2
 * #tot: total number of points

In the above message it appears that there was one or more latency gaps of at least 4.51s (max gap),
which resulted time tags being adjusted earlier by as much as 4.5827s (max late). However the results
look pretty good, the largest dt in the results, "outdt max",  was 0.12 seconds or 6 sample times.

The most important field to check is probably "outdt max". A large value, more than a few sample dts,
indicates a problem in adjusting the time tags, resulting in gaps in the output time tags.

A signifant difference between dt min and dt max, indicates the code cannot figure out a good
sample reporting rate.

A large number of #neg or #pos is also a concern.

### Large Positive Tdiff

During processing, TimetagAdjuster logs some warnings.

When TimetagAdjuster is adding the minimum tdiff to T0 for the next set of adjusted points, and the value of
tdiff is larger than dt/2, a warning is logged:

    WARNING|ttadjust: tdiff > dt/2: 2020 11 11 19:30:02.166, id=20,131, tdiff=  0.01, dt=  0.02, #big=1

### Large Negative Tdiff

When TimetagAdjuster is finds a negative tdiff less than -dt/2, it logs the issue before applying it to T0 for the next set of adjusted points. This should be rare, hopefully only happening at startup, when there is significant latency jitter

    WARNING|ttadjust: tdiff < -dt/2: 2020 11 11 19:30:02.166, id=20,131, tdiff=  0.01, dt=  -0.02, #neg=1

## Debugging

High rate log messages can be printed for each sample for a given id, by adding log parameters to NIDAS data processing programs.  For example:

    data_dump -i 20,131 -p --logconfig enable,level=verbose,function=TimetagAdjuster::adjust --logparam trace_samples=20,131 --logconfig enable,level=info --logfields level,message input.ads 2> output.err > output.out

This will result in VERBOSE log messages in output.err looking like:

    VERBOSE|HR [20,131]@2020 11 11 22:04:59.683, toff tdiff tdiffUncorr tdiffmin nDt: 0.80856 0.002243 0.002243 6.1e-05 40

where for each sample, its time tag, Traw, and ttadjust variables are printed:

    * toff: I * dt
    * tdiff = Traw[i] - Tadj[I], set to 0 if negative
    * tdiffUncorr: tdiff, but not set to 0 if negative
    * tdiffmin: minimum value of tdiff so far in the current set of points
    * nDt: the point count in the current set of points

