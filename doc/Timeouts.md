# Sensor Timeouts

A sensor *times out* when no bytes have been read from it for the length of
its timeout period.   This is completely independent of whether the read bytes
could be parsed into valid samples for the particular sensor.  For example, a
sensor will not time out if it intermittently writes random bytes always
within the timeout period of the last byte. When a sensor does time out, NIDAS
closes the sensor and reopens it. The `timeout` attribute on a sensor element
controls how that works. A simple timeout is still just a number of seconds,
as in older configurations, but there are additional settings:

- a delay before each reopen, which can grow with each timeout (backoff), up
  to a maximum
- an interval N which cycles the sensor power every N timeouts, with an
  optional warmup period after power comes back on

The power cycling is off by default, and it is only available for DSM serial
card ports with device names `/dev/ttyDSM[0-7]`.

## Syntax

```
timeout[,delay[,backoff[,maxdelay]]] [cycle_timeouts[,warmup]]
```

The attribute has two groups of fields. Commas separate the fields within a
group, and whitespace separates the two groups. Only `timeout` is required,
and a field can only be given if every field before it in the same group is
given too. Spaces are allowed around the commas.

| Field            | Units   | Default | Meaning |
|------------------|---------|---------|---------|
| `timeout`        | seconds | 0       | How long the sensor can go without reporting data before it times out. 0 disables timeouts. |
| `delay`          | seconds | 1       | Delay before the first reopen in a series of timeouts. Must be from 1 to 60 seconds. |
| `backoff`        | factor  | 1.0     | How much the delay grows with each further timeout. 0 keeps the delay fixed. Must not be negative. |
| `maxdelay`       | seconds | 60      | Upper limit on the delay before a reopen. The limit can never be above 60 seconds. |
| `cycle_timeouts` | count   | 0       | Power cycle the sensor on every Nth timeout. 0 disables power cycling. Must not be negative. |
| `warmup`         | seconds | 0       | After power comes back on, wait this long before opening the sensor. Must not be negative. |

`timeout`, `delay`, `maxdelay`, and `warmup` are floating-point seconds,
rounded to the nearest millisecond. `backoff` is a floating-point factor, and
`cycle_timeouts` is an integer.

Any fields after `warmup` are ignored, so that future versions can add
settings without breaking older software.

### Invalid settings

If the attribute is not a valid timeout string, the sensor fails to load with
an invalid parameter error naming the `timeout` attribute. This happens when:

- the first field is not a number
- something follows a comma that is not a number
- something other than an integer follows the first group
- `delay` is outside the range 1 to 60 seconds
- `backoff`, `cycle_timeouts`, or `warmup` is negative

### Examples

```xml
<!-- Old style: time out after 3 seconds, then use the default delay and backoff. -->
<serialSensor ID="CDP" ... timeout="3.0">

<!-- Reopen every second and never back off. -->
<serialSensor ... timeout="1,1,0">

<!-- Back off slowly, and never wait more than 10 seconds between reopens. -->
<serialSensor ... timeout="2,1,.2,10">

<!-- Power cycle on every timeout, but back off quickly in case the sensor
     is not there at all. -->
<serialSensor ... timeout="5,1,2 1">

<!-- Power cycle on every third timeout, and give the sensor 2.5 seconds
     after power-on before opening it. -->
<serialSensor ... timeout="10,1,1,30 3,2.5">
```

## Behavior

### Detecting a timeout

All of a DSM's sensors are checked for timeouts at a regular interval. That
interval equals the shortest nonzero timeout of any sensor on the DSM, and
each sensor's timeout is rounded up to a whole number of check intervals. So
a sensor may go somewhat longer than its timeout before the timeout is
detected. A sensor whose timeout is 0 is never checked.

Each timeout is logged as a warning that includes how many timeouts there
have been. After 100 timeouts, only every 100th one is logged.

### Counting timeouts

Once a sensor times out, consecutive timeout periods (or reopen failures) are
counted up until the sensor reports data again. This count of timeouts in the
current series, `n`, sets the reopen delay and determines when to power cycle.
When the sensor reports data again, `n` goes back to zero, so the next timeout
starts a new series from the initial delay.

A failed reopen also counts as a timeout. For example, the open can fail if
the device does not exist. Every reopen attempt therefore goes through the
same delay and backoff, whether the sensor timed out or could not be opened.

A separate total of all timeouts and all power cycles is kept for the life of
the sensor. Those totals are logged when the sensor is closed:

```
closing: /dev/ttyDSM3, #timeouts=12, #cycles=4
```

### Reopen delay

Before each reopen, NIDAS waits for:

```
d = delay * (1 + backoff * (n - 1))
```

The result is then limited to between 1 second and `maxdelay` (and never above
60 seconds). On the first timeout of a series (`n = 1`), the wait is just
`delay`. For example, `delay=2`, `backoff=1.5`, and `maxdelay=6` give waits of
2, 5, 6, 6, ... seconds. A sensor that is opened for the first time, and has
never timed out, is opened right away.

All reopens happen one at a time in a single thread. While one sensor waits
out its delay or warmup, other sensors waiting to be reopened are held up as
well. This is why the delay is limited to 60 seconds.

The wait is logged as `open delay: <ms> ms`.

### Defaults when no timeout is given

A sensor with no `timeout` attribute never times out. It still uses the
default delay of 1 second, backoff of 1.0, and maximum delay of 60 seconds
whenever it has to be reopened, for example after a failed open. Repeated open
failures are retried after 1, 2, 3, ... seconds, up to one try per minute.

This is different than the historical behavior which forced all reopens to
wait 10 seconds, so these new settings may need to be re-evaluated after
practical experience with them.

### Power cycling

Power cycling is only possible for serial sensors on a port with hardware
power control. For any other sensor, the power fields are accepted but have
no effect.

On every timeout where `n` is a multiple of `cycle_timeouts`:

1. When the timeout is detected, the sensor is powered off (logged as
   `Power off: <device>`).
2. NIDAS waits for the reopen delay, with the sensor still off.
3. If `warmup` is greater than 0, the sensor is powered on and NIDAS waits
   `warmup` seconds (logged as `warmup delay: <ms> ms`).  The `warmup` period
   allows for sensors which need time after power-up to be ready to read an
   init string upon being opened.
4. The sensor is opened. Every serial sensor open powers the port on, so if
   there was no warmup, the sensor powers on at this point. This is right
   after the device opens, so the sensor's startup messages can be recorded.

If the time between power on and timeout (ie, `warmpup` plus `timeout`) is too
short for the sensor to power up and start reporting, then it will just time
out again.  During a power cycle, the `delay` controls how long the sensor
stays off before being powered back on.

The sensor is powered off only when a data timeout is detected, not when an
open fails. If the device itself has gone away, cycling the power will not fix
that, so the sensor is not cycled repeatedly on each failed open. Closing a
sensor never turns off its power.
