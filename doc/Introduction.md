# NCAR InSitu Data Acquisition Software (NIDAS)

NIDAS is a software system for data acquisition, archive, distribution and processing.
It has been developed for atmospheric research, with an emphasis on efficient sampling
of time series from a wide variety of research quality sensors at the surface and on aircraft,
at rates necessary for resolving turbulent atmospheric processes.

Each sensor is sampled independently in an asynchronous manner. A time tag
of microsecond resolution is assigned to each sample at the moment it is received,
based on a highly accurate system clock, which is continually conditioned using
Network Time Protocol (NTP) from a server, or from a directly
connected GPS with a pulse-per-second (PPS) signal.

At acquisition time, minimal data interpretation is performed to differentiate
individual messages from a sensor, assembling the data exactly as it was received
into a sample, with the associated time-tag and an identifier of the sensor
and data system.  The concatenated stream of samples from all sensors is then
passed on for archival and further processing. Redundant archive of raw samples
is commonly performed, including writing to local storage media, and distribution
over the network.

NIDAS is written in C++, and runs on Linux systems, ranging from low-power,
system-on-a-chip microprocessors to rack-mount servers.

A variety of sensor interfaces are supported, including RS232/422, USB,
TCP/UDP, I2C and radio mesh networks.  PC104 expansion cards are
also supported for sampling of analog voltages, frequency measurements and pulse
counting, RS232/422, aircraft navigation (ARINC-429) and IRIG-B time codes.
NIDAS includes Linux driver modules, written in C, for these PC104 expansion
cards, and for specific sensors with a USB interface.

Sensor initialization, prompting and real-time interaction, such as providing the
current aircraft airspeed, is also supported.

NIDAS provides for a tree-structured, distributed, data acquisition topology,
where one or more small footprint data systems can be networked together, each sampling 
one or more inputs, originating from one or more sensor transducers. The 
data systems can also run in an autonomous, stand-alone manner.

Support for each type of sensor in NIDAS includes code to generate processed
samples in scientific units from the raw, acquired samples, using associated
sensor meta-data, including time-varying calibrations.

Processing of the raw samples can be applied in near real-time for display and system control,
and at a later time as algorithms and meta-data are established or adjusted.
Sample processing can be distributed among the microprocessors or servers
as required by the specific application.  Processed samples can be distributed
over the network for successive data reduction, derivation and display,
using TCP or UDP protocols.

Configuration of NIDAS for a field application is done in XML. The configuration
can be stored in static form on each data system, or provided over the network
as each data system comes online.

The Unidata Network Common Data Form (NetCDF) is the usual file format for processed data.

NIDAS has been developed at [[UCAR/NCAR/EOL|https://www.eol.ucar.edu]] with the support of the [[National Science Foundation|https://www.nsf.gov/]] and is freely available under the GNU General Public License V2.0.