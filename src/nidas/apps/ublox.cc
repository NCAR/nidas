// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2006, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/
/*
  Configure a Ublox GPS via I2C.

  The Ublox 6Q has 3 boot-time configuration pins:
  CFG_COM0, CFG_COM1 and CFG_GPS0. See the NEO 6 data sheet.

  On the HAT board for the ISFS RaspberryPi2 these
  pins are at 3.3V, which results in the following settings:
  CFG_COM0=1, CFG_COM1=1: NMEA, 9600 baud
  CFG_GPS0=1, max performance mode.
*/

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <cstdlib>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>

#include <iostream>
#include <iomanip>
#include <sstream>
#include <map>
#include <vector>
#include <nidas/util/IOException.h>

using namespace std;

#define MSECS_PER_SEC (1000)
#define USECS_PER_SEC (1000 * 1000)

namespace n_u = nidas::util;

class ublox_pkt
{
public:
    ublox_pkt(uint8_t uclass, uint8_t id, uint16_t paylen);

    ublox_pkt(uint16_t paylen);

    ~ublox_pkt();

    uint8_t sync1() const { return _pkt[0]; }
    uint8_t sync2() const { return _pkt[1]; }
    uint8_t uclass() const { return _pkt[2]; }
    uint8_t id() const { return _pkt[3]; }

    uint8_t* payload() const { return _pkt + 6; }

    // set the payload
    void payload(void* ptr) { memcpy(payload(), ptr, _paylen); }

    // total length of packet
    int16_t len() const { return _paylen + 8; }

    // pointer to beginning of packet
    char* beginp() const { return (char*) _pkt; }

    // pointer to one-past-the end
    char* endp() const { return beginp() + len(); }

    // length of payload as found in packet
    int16_t paylen() const
    {
        return ((uint16_t) _pkt[5] << 8) + _pkt[4];
    }

    // pointer to first byte of checksum
    uint8_t* cksump() const { return (uint8_t*)beginp() + _paylen + 6; }

    // compute checksum
    void cksum();

private:
    int16_t _paylen;

    uint8_t* _pkt;

    // no copying
    ublox_pkt(const ublox_pkt&);

    // no assignment
    ublox_pkt& operator = (const ublox_pkt&);
};

ublox_pkt::ublox_pkt(uint8_t uclass, uint8_t id, uint16_t paylen):
    _paylen(paylen), _pkt(new uint8_t[len()])
{
    memset(_pkt, 0, len());

    _pkt[0] = 0xb5; // sync 1
    _pkt[1] = 0x62; // sync 2
    _pkt[2] = uclass;
    _pkt[3] = id;
    _pkt[4] = (_paylen & 0xff);
    _pkt[5] = (_paylen & 0xff00) >> 8;
}

ublox_pkt::ublox_pkt(uint16_t paylen):
    _paylen(paylen), _pkt(new uint8_t[len()])
{
    memset(_pkt, 0, len());
}

ublox_pkt::~ublox_pkt()
{
    delete [] _pkt;
}

void ublox_pkt::cksum()
{
    unsigned char ck_a = 0, ck_b = 0;

    for (uint8_t* cp = _pkt + 2 ; cp < cksump(); cp++) {
        ck_a += *cp;
        ck_b += ck_a;
    }
    cksump()[0] = ck_a;
    cksump()[1] = ck_b;
}

/**
 * Ubox IDs for standard NMEA messages
 */
enum NMEA_ID {
    GGA,    // 0x00
    GLL,    // 0x01
    GSA,    // 0x02
    GSV,    // 0x03
    RMC,    // 0x04
    VTG,    // 0x05
    GRS,    // 0x06
    GST,    // 0x07
    ZDA,    // 0x08
    GBS,    // 0x09
    DTM,    // 0x0a
    GPQ=0x40,
    TXT=0x41
};

class ublox {
public:

    ublox();

    ~ublox();

    int parseRunstring(int argc, char** argv);

    static int usage(const char* argv0);

    int run() throw();

    int write_smbus(const ublox_pkt& pkt) throw();

    int write_smbus(const string& str) throw();

    int write_rdwr(const ublox_pkt& pkt) throw();

    int write_rdwr(const string& str) throw();

    int read_rdwr_simple(string& str) throw();

    int read_rdwr_simple2(string& str) throw();

    int read_rdwr(string& str) throw();

    string read_smbus() throw();

    int read_byte() throw();

    int read_byte_data(unsigned char reg) throw();

    int write_byte(unsigned char b) throw();

    int write_byte_data(unsigned char b, unsigned char reg) throw();

    void reset() throw();

    void config_port() throw();

    void set_rate(int rate) throw();

    void config_ubx() throw();

    void config_nmea(enum NMEA_ID id, bool enable) throw();

    /**
     * Send a "$PUBX,40,..." proprietory message to enable/disable
     * NMEA messages.
     */
    void config_nmea(const string&, bool enable) throw();

    void config_nmea_all_targets(enum NMEA_ID id, bool enable) throw();

    static string nmea_cksum(const string& msg);

private:
    string progname;

    string _name;

    unsigned short _addr;

    int _fd;

    map<string,enum NMEA_ID> _nmea_map;

    vector<string> _enable_msgs;

    vector<string> _disable_msgs;

};

ublox::ublox():progname(),_name(),_addr(0), _fd(-1),
    _nmea_map(), _enable_msgs(), _disable_msgs()
{
    _nmea_map["GGA"] = GGA;
    _nmea_map["GLL"] = GLL;
    _nmea_map["GSA"] = GSA;
    _nmea_map["GSV"] = GSV;
    _nmea_map["RMC"] = RMC;
    _nmea_map["VTG"] = VTG;
    _nmea_map["GRS"] = GRS;
    _nmea_map["GST"] = GST;
    _nmea_map["ZDA"] = ZDA;
    _nmea_map["GBS"] = GBS;
    _nmea_map["DTM"] = DTM;
    _nmea_map["GPQ"] = GPQ;
    _nmea_map["TXT"] = TXT;
}

ublox::~ublox()
{
    if (_fd >= 0) close(_fd);
}

enum reset_type
{
    HOTSTART,
    WARMSTART,
    COLDSTART = 0xffff
};

void ublox::set_rate(int rate) throw()
{
    ublox_pkt pkt(0x06, 0x08, 6);  // CFG-RST packet

    struct cfg_rate {
        uint16_t measRate;
        uint16_t navRate;
        uint16_t timeRef;
    } payload;

    payload.measRate = 1000 / rate;    // milliseconds/sample
    payload.navRate = 1;    // milliseconds/sample
    payload.timeRef = 0;    // milliseconds/sample
    pkt.payload(&payload);
    pkt.cksum();
    write_rdwr(pkt);
}

void ublox::reset() throw()
{
    ublox_pkt pkt(0x06, 0x04, 4);  // CFG-RST packet
    struct rst {
        uint16_t navBbrMask;
        uint8_t resetMode;
        uint8_t res;
    } payload;
    payload.navBbrMask = HOTSTART;
    payload.resetMode = 0x9;    // controlled GPS start
    payload.res = 0x0;

    pkt.payload(&payload);
    pkt.cksum();

    write_smbus(pkt);
}

void ublox::config_port() throw()
{

    ublox_pkt msg(0x06, 0x00, 20);  // CFG-PRT packet

    /**
     * UBX payload packets.
     */
    struct cfg_prt_payload {
        unsigned char portID;
        unsigned char res0[3];
        unsigned int mode;
        unsigned int res1;
        unsigned short inProtoMask;
        unsigned short outProtoMask;
        unsigned short flags;
        unsigned short res2;
    } ppay;

    memset(&ppay,0,sizeof(ppay));
    ppay.portID = 0x0;          // DDC
    ppay.mode = 0x42;           // i2c address
    ppay.inProtoMask = 0x3;     // NMEA and UBX
    ppay.outProtoMask = 0x3;    // NMEA and UBX

    msg.payload(&ppay);
    msg.cksum();

    write_smbus(msg);
    usleep(USECS_PER_SEC / 4);
    read_smbus();

}

void ublox::config_nmea(const string& msg, bool enable) throw()
{
    ostringstream ost;
    ost << "$PUBX,40," << msg << ',' <<
        (int) enable << ',' << (int) enable << ',' << (int) enable << ',' <<
        (int) enable << ',' << (int) enable << ',' << 0;
    string cs = nmea_cksum(ost.str());

    ost << '*' << cs << "\r\n";
#ifdef DEBUG
    cerr << "config_nmea, ost=" << ost.str() << endl;
#endif

#define WRITE_RDWR
#ifdef WRITE_RDWR
    write_rdwr(ost.str());
#else
    write_smbus(ost.str());
#endif

    return;
}

void ublox::config_ubx() throw()
{
    int portId = 0;                 // port id is 0 for DDC (I2C)
    string inputProtMask = "0003";  // 2=NMEA, 1=UBX
    string outputProtMask = "0002"; // 2=NMEA, 1=UBX
    string baud = "100000";   //  does it matter for DDC (I2C)?
    int autobaud = 0;   //  shouldn't matter for DDC (I2C)

    ostringstream ost;
    ost << "$PUBX,41," << portId << ',' << 
        inputProtMask << ',' <<
        outputProtMask << ',' <<
        baud << ',' <<
        autobaud;

    cerr << "ost=" << ost.str() << endl;

    string cs = nmea_cksum(ost.str());

    ost << '*' << cs << "\r\n";
    cerr << "config_ubx, ost=" << ost.str() << endl;

    write_rdwr(ost.str());

    return;
}

void ublox::config_nmea_all_targets(enum NMEA_ID id, bool enable) throw()
{

    ublox_pkt pkt(0x06, 0x01, 8);

    pkt.payload()[0] = 0xf0;
    pkt.payload()[1] = id;
    for (int i = 2; i < 8; i++) pkt.payload()[i] = enable;

    pkt.cksum();

    write_smbus(pkt);
    usleep(USECS_PER_SEC / 4);
    read_smbus();

}

void ublox::config_nmea(enum NMEA_ID id, bool enable) throw()
{
    ublox_pkt pkt(0x06, 0x01, 3);
    pkt.payload()[0] = 0xf0;
    pkt.payload()[1] = id;
    pkt.payload()[2] = enable;

    pkt.cksum();

    write_smbus(pkt);
    usleep(USECS_PER_SEC / 4);
    read_smbus();
}

string ublox::nmea_cksum(const string& msg) 
{
    string::const_iterator cp = msg.begin();
    if (*cp == '$') cp++;

    char calcsum = 0;
    for ( ; cp < msg.end(); ) calcsum ^= *cp++;

    ostringstream ost;
    ost << hex << setfill('0') << setw(2) << (unsigned int) calcsum;
    return ost.str();
}

int ublox::write_rdwr(const string& str) throw()
{

    struct i2c_msg rdwr_msgs[2];
    struct i2c_rdwr_ioctl_data rdwr_data;

    rdwr_msgs[0].addr = _addr;
    rdwr_msgs[0].flags = 0; // write
    rdwr_msgs[0].len = str.length();
    rdwr_msgs[0].buf = (char*) str.c_str();

    rdwr_data.msgs = rdwr_msgs;
    rdwr_data.nmsgs = 1;
    
    int res = ioctl(_fd, I2C_RDWR, &rdwr_data );
    if (res < 0) {
        cerr << "write_rdwr(str), ioctl 1 res=" << res << endl;
        return res;
    }

#ifdef DEBUG
    cerr << "write_rdwr(str), ioctl 1 res=" << res << endl; // 1
#endif

    return res;
}

short len_swab(const char* cp)
{
    return ((unsigned char)cp[0] << 8) +
        (unsigned char)cp[1];
}

int ublox::read_rdwr_simple(string& str) throw()
{
    struct i2c_msg rdwr_msgs[3];
    struct i2c_rdwr_ioctl_data rdwr_data;

    int res;

    unsigned char reg = 0xff;
    rdwr_msgs[0].addr = _addr;
    rdwr_msgs[0].flags = 0; // write
    rdwr_msgs[0].len = 1;
    rdwr_msgs[0].buf = (char*)&reg;

    char buffer[8192];
    memset(buffer,0,sizeof(buffer));
    rdwr_msgs[1].addr = _addr;
    rdwr_msgs[1].flags = I2C_M_RD; // read
    rdwr_msgs[1].len = 8192;
    rdwr_msgs[1].buf = buffer;

    rdwr_data.msgs = rdwr_msgs;
    rdwr_data.nmsgs = 2;

    res = ioctl(_fd, I2C_RDWR, &rdwr_data );
    if (res < 0) {
        cerr << "read_rdwr_simple(str), ioctl res=" << res << endl;
        return res;
    }
    int l = 0;
    
    for (const char* cp = buffer; *cp != 0xff && *cp; cp++,l++)
        cerr << *cp << ' ';
    cerr << endl;
    cerr << "l=" << l << endl;
    str.assign(buffer,l);
    return l;
}

int ublox::read_rdwr_simple2(string& str) throw()
{
    struct i2c_msg rdwr_msgs[1];
    struct i2c_rdwr_ioctl_data rdwr_data;

    int res;

    char buffer[32];
    memset(buffer,0,sizeof(buffer));
    rdwr_msgs[0].addr = _addr;
    rdwr_msgs[0].flags = I2C_M_RD; // read
    rdwr_msgs[0].len = 32;
    rdwr_msgs[0].buf = buffer;

    rdwr_data.msgs = rdwr_msgs;
    rdwr_data.nmsgs = 1;

    res = ioctl(_fd, I2C_RDWR, &rdwr_data );
    if (res < 0) {
        cerr << "read_rdwr_simple(str), ioctl res=" << res << endl;
        return res;
    }

    int l = 0;
    for (const char* cp = buffer;
            cp < buffer + sizeof(buffer) && *cp != 0xff && *cp; cp++,l++) {
        if (isprint(*cp)) cerr << *cp << ' ';
        else cerr << "\\x" << setw(2) << setfill('0') << hex <<
            (int)*cp << dec << ' ';
    }
    cerr << endl;
    cerr << "l=" << l << endl;
    str.assign(buffer,l);
    return l;
}

int ublox::read_rdwr(string& str) throw()
{
    struct i2c_msg rdwr_msgs[3];
    struct i2c_rdwr_ioctl_data rdwr_data;

    int res;

    unsigned char reg = 0xfd;
    rdwr_msgs[0].addr = _addr;
    rdwr_msgs[0].flags = 0; // write
    rdwr_msgs[0].len = 1;
    rdwr_msgs[0].buf = (char*)&reg;

    char buffer[8192];
    memset(buffer,0,sizeof(buffer));
    rdwr_msgs[1].addr = _addr;
    rdwr_msgs[1].flags = I2C_M_RD; // read
    rdwr_msgs[1].len = 8192;
    rdwr_msgs[1].buf = buffer;

    rdwr_data.msgs = rdwr_msgs;
    rdwr_data.nmsgs = 2;

    res = ioctl(_fd, I2C_RDWR, &rdwr_data );
    if (res < 0) {
        cerr << "read_rdwr(str), ioctl 2 res=" << res << endl;
        return res;
    }
    cerr << "read_rdwr(str), ioctl 2 res=" << res << endl; // 2
    short len = len_swab(buffer);

    cerr << "read_rdwr(str), ioctl 2 len=" << len << endl; //

    cerr << "received: ";
    int l = 0;
    
    for (const char* cp = buffer + 2; *cp != 0xff && *cp; cp++,l++) {
        if (isprint(*cp)) cerr << *cp << ' ';
        else cerr << "\\x" << setw(2) << setfill('0') << hex <<
            (int)*cp << dec << ' ';
    }
    cerr << endl;
    cerr << "l=" << l << endl;
    str.assign(buffer,l);
    return l;
}

int ublox::write_rdwr(const ublox_pkt& pkt) throw()
{

    struct i2c_msg rdwr_msgs[2];
    struct i2c_rdwr_ioctl_data rdwr_data;

    rdwr_msgs[0].addr = _addr;
    rdwr_msgs[0].flags = 0; // write
    rdwr_msgs[0].len = pkt.len();
    rdwr_msgs[0].buf = pkt.beginp();

    rdwr_data.msgs = rdwr_msgs;
    rdwr_data.nmsgs = 1;
    
    int res = ioctl(_fd, I2C_RDWR, &rdwr_data );
    if (res < 0) {
        cerr << "ioctl res=" << res << endl;
        return res;
    }

    ublox_pkt ack(2);
    unsigned char reg = 0xfd;
    rdwr_msgs[0].addr = _addr;
    rdwr_msgs[0].flags = 0; // write
    rdwr_msgs[0].len = 1;
    rdwr_msgs[0].buf = (char*)&reg;

    rdwr_msgs[1].addr = _addr;
    rdwr_msgs[1].flags = I2C_M_RD; // read
    rdwr_msgs[1].len = ack.len();
    rdwr_msgs[1].buf = ack.beginp();

    rdwr_data.msgs = rdwr_msgs;
    rdwr_data.nmsgs = 2;

    res = ioctl(_fd, I2C_RDWR, &rdwr_data );
    if (res < 0) {
        cerr << "ioctl res=" << res << endl;
        return res;
    }

    cerr << hex << "sync1=" << (unsigned int)ack.sync1() <<
        ", sync2=" << (unsigned int)ack.sync2() <<
        ", uclass=" << (unsigned int)ack.uclass() <<
        ", id=" << (unsigned int)ack.id() <<
        ", payload[0]=" << (unsigned int)ack.payload()[0] <<
        ", payload[1]=" << (unsigned int)ack.payload()[1] << dec << endl;

#ifdef READ_BACK
#endif
    return res;
}

int ublox::write_smbus(const string& str) throw()
{
    string::const_iterator cp = str.begin();
    int res = 0;
    for ( ; cp != str.end(); cp++) {
        res = write_byte(*cp);
        if (res < 0) {
            cerr << "write res=" << res << endl;
            break;
        }
    }
    return res;
}
int ublox::write_smbus(const ublox_pkt& pkt) throw()
{
    char *mp = pkt.beginp();
    int res = 0;
    for (int i = 0; mp < pkt.endp(); mp++,i++) {
        cerr << "write byte " << i << "=" << hex <<
            (unsigned int) *mp <<  dec << endl;
        res = write_byte_data((unsigned char)*mp, 0);
        if (res < 0) {
            cerr << "write res=" << res << endl;
            break;
        }
    }
    return res;
}

string ublox::read_smbus() throw()
{
    string res;

#ifdef READ_LEN_REGS
    unsigned char reg = 0xfd;
    int c1 = read_byte_data(reg);
    int c2 = read_byte();
    if ((c1 & 0xff) != 0xff  && (c2 & 0xff) != 0xff) {
        int len = ((c1 & 0xff) << 8) + (c2 & 0xff);
        cerr << "response len " << len << endl;

        for (int i = 0; i < len; i++) {
            int db = read_byte();
            if (db < 0) {
                cerr << "read: byte " << i << " res=" << db << endl;
                break;
            }
            cerr << "read: byte " << i << "=" << hex <<
                (unsigned int)(db & 0xff) << dec << endl;
        }
    }
    else {
        cerr << "read: no len info from 0xfd, 0xfe" << endl;
        for (int i = 0; ; i++) {
            int db = read_byte();
            if (db < 0) {
                cerr << "read: byte " << i << " res=" << db << endl;
                break;
            }
            cerr << "read: byte " << i << "=" << hex <<
                (unsigned int)(db & 0xff) << dec << endl;
            if ((db & 0xff) == 0xff) break;
        }
    }
#else
    for (int i = 0; ; i++) {
        int db = read_byte();
        if (db < 0) {
            cerr << "read: byte " << i << " res=" << db << endl;
            break;
        }
#ifdef DEBUG
        cerr << "read: byte " << i << "=" << hex <<
            (unsigned int)(db & 0xff) << dec << endl;
#endif
        if ((db & 0xff) == 0xff) break;
        res.push_back((char)(db & 0xff));
    }
#endif
    return res;
}

int ublox::read_byte() throw()
{
    int res = i2c_smbus_read_byte(_fd);
    if (res < 0) {
        n_u::IOException e(_name,"read_byte",errno);
        cerr << "Error: " << e.what() << endl;
    }
    return res;
}

int ublox::read_byte_data(unsigned char reg) throw()
{
    int res = i2c_smbus_read_byte_data(_fd,reg);
    if (res < 0) {
        ostringstream ost;
        ost << "read_byte_data, reg=" << hex << (unsigned int) reg;
        n_u::IOException e(_name,ost.str(),errno);
        cerr << "Error: " << e.what() << endl;
    }
    return res;
}

int ublox::write_byte(unsigned char b) throw()
{
    int res = i2c_smbus_write_byte(_fd, b);
    if (res < 0) {
        n_u::IOException e(_name,"write_byte",errno);
        cerr << "Error: " << e.what() << endl;
    }
    return res;
}

int ublox::write_byte_data(unsigned char b, unsigned char reg) throw()
{
    int res = i2c_smbus_write_byte_data(_fd, reg, b);
    if (res < 0) {
        ostringstream ost;
        ost << "write_byte_data, reg=" << hex << (unsigned int) reg;
        n_u::IOException e(_name,ost.str(),errno);
        cerr << "Error: " << e.what() << endl;
    }
    return res;
}

int ublox::parseRunstring(int argc, char** argv)
{
    progname = argv[0];
    int iarg = 1;

    for ( ; iarg < argc; iarg++) {
	string arg = argv[iarg];
        if (arg == "-d") {
            if (++iarg == argc) return usage(argv[0]);
            _disable_msgs.push_back(argv[iarg]);
        }
        else if (arg == "-e") {
            if (++iarg == argc) return usage(argv[0]);
            _enable_msgs.push_back(argv[iarg]);
        }
        else {
            if (_name.length() == 0) _name = argv[iarg];
            else if (_addr == 0) _addr = strtol(argv[iarg], NULL, 0);
        }
    }

    if (_name.length() == 0) return usage(argv[0]);
    if (_addr < 3 || _addr > 255) {
        cerr << "i2caddr out of range" << endl;
        return usage(argv[0]);
    }
    return 0;
}

int ublox::usage(const char* argv0)
{
    cerr << "\
Usage: " << argv0 << "i2cdev i2caddr [-d msg] [-e msg]...\n\
  i2cdev: name of I2C bus to open, e.g. /dev/i2c-1\n\
  i2caddr: address of I2C device, usually in hex: e.g. 0x42\n\
  -d msg: a NMEA message to disable\n\
  -e msg: a NMEA message to enabl\n\
\n\
    msg is one of: GGA, GLL, GSA, GSV, RMC, VTG, GRS, GST, ZDA, GBS, DTM, GPQ, TXT\n\
" << endl;
    return 1;
}

int ublox::run() throw()
{

    int result = 0;

    try {

        _fd = open(_name.c_str(), O_RDWR);
        if (_fd < 0)
            throw n_u::IOException(_name, "open", errno);

        if (ioctl(_fd, I2C_TIMEOUT, 60* MSECS_PER_SEC / 10) < 0) {
            ostringstream ost;
            ost << "ioctl(,I2C_TIMEOUT,)";
            throw n_u::IOException(_name, ost.str(), errno);
        }

        if (ioctl(_fd, I2C_SLAVE, _addr) < 0) {
            ostringstream ost;
            ost << "ioctl(,I2C_SLAVE," << hex << _addr << ")";
            throw n_u::IOException(_name, ost.str(), errno);
        }


#ifdef DO_THIS
        // config_ubx();
        sleep(1);
        set_rate(1);
        sleep(1);
#endif

        for (unsigned int i = 0; i < _disable_msgs.size(); i++) {
            config_nmea(_disable_msgs[i], false);
            usleep(USECS_PER_SEC / 4);
        }

        for (unsigned int i = 0; i < _enable_msgs.size(); i++) {
            config_nmea(_enable_msgs[i], true);
            usleep(USECS_PER_SEC / 4);
        }

// #define READ_BACK
#ifdef READ_BACK
        for (int i = 0; i < 20; i++) {
            usleep(USECS_PER_SEC / 4);
// #define DO_READ_RDWR
#ifdef DO_READ_RDWR
            string str;
            int res = read_rdwr(str);
            if (res > 0) cerr << "config_nmea, read_rdwr len=" << res << endl;
            cerr << "string, len=" << str.length() << "," << str << endl;
#else
            string str = read_smbus();
#ifdef DEBUG
            cerr << "string=" << str << endl;
#endif
#endif
        }
#endif
    }
    catch(n_u::IOException& ioe) {
	cerr << "Error: " << ioe.what() << endl;
        result = 1;
    }
    return result;
}

int main(int argc, char** argv)
{
    ublox ub;
    int res;
    if ((res = ub.parseRunstring(argc,argv)) != 0) return res;

    return ub.run();
}

