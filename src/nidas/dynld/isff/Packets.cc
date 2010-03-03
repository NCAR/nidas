/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************
*/

#include <nidas/dynld/isff/Packets.h>
#include <nidas/dynld/isff/GOES.h>
#include <nidas/core/Sample.h>
#include <nidas/util/Logger.h>

#include <iomanip>

#include <cassert>

using namespace nidas::dynld::isff;
using namespace std;

namespace n_u = nidas::util;

/* static */
::regex_t** PacketParser::_packetPreg = 0;

/* static */
::regex_t** PacketParser::_infoPreg = 0;

/* static */
nidas::util::Mutex PacketParser::_pregMutex;

PacketParser::PacketParser() throw(n_u::ParseException):
    _nmatch(10),	// max number of parenthesized expressions
    _pmatch(0),_infoType(-1),_packetInfo(0),_stationId(-1),
    _configId(-1),_sampleId(-1)
{
    // 
    _pmatch = new ::regmatch_t[_nmatch];

    n_u::Autolock autolock(_pregMutex);

    // Check if regular expressions need compiling
    if (!_packetPreg) {
	int regstatus;

	const char *packetRE[] = {
// Extended regular expression for parsing a Packet NESDIS message
// stnid         yr           doy             hr        mn            sc
"^([0-9A-F]{8})([0-9]{2})([0-3][0-9][0-9])([0-2][0-9])([0-5][0-9])([0-5][0-9]) *",
	};

	_nPacketTypes = sizeof(packetRE) / sizeof(packetRE[0]);
	_packetPreg = new ::regex_t *[_nPacketTypes];

	for (int i = 0; i < _nPacketTypes; i++) {
	    _packetPreg[i] = new ::regex_t;
	    if ((regstatus = ::regcomp(_packetPreg[i],packetRE[i],REG_EXTENDED))
		!= 0) {
		delete _packetPreg[i]; _packetPreg[i] = 0;
		char regerrbuf[64];
		::regerror(regstatus,_packetPreg[i],regerrbuf,sizeof regerrbuf);
		throw n_u::ParseException(string("regcomp: ") + regerrbuf + ": " +
			packetRE[i]);
	    }
	}
	const char *infoRE[] = {
// Extended regular expression for parsing NESDIS type info fields
//   quality      dbm    freq offset  mod  dq  chann   E/W      len
"^[G?WDQABITUMN][0-9]{2}[-+][0-9A-F][NLH][NFP][0-9]{3}[EW]..[0-9]{5} *",

// Extended regular expression for parsing SUTRON info fields
// "^[0-9] +[-+0-9.]+ +[0-9]+ +[0-9]+ +[-+0-9.]+ *",
"^[0-9] +(([-+]?[0-9]+\\.?[0-9]*)|([-+]?\\.?[0-9]+)) *[0-9]+ *[0-9]+ *(([-+]?[0-9]+\\.?[0-9]*)|([-+]?\\.?[0-9]+)) *",
	};

	_nInfoTypes = sizeof(infoRE) / sizeof(infoRE[0]);
	_infoPreg = new ::regex_t *[_nInfoTypes];

	for (int i = 0; i < _nInfoTypes; i++) {
	    _infoPreg[i] = new ::regex_t;
	    if ((regstatus = ::regcomp(_infoPreg[i],infoRE[i],REG_EXTENDED))
		!= 0) {
		delete _infoPreg[i]; _infoPreg[i] = 0;
		char regerrbuf[64];
		::regerror(regstatus,_infoPreg[i],regerrbuf,sizeof regerrbuf);
		throw n_u::ParseException(string("regcomp: ") + regerrbuf + ": " +
			infoRE[i]);
	    }
	}
    }
}

PacketParser::~PacketParser()
{
    delete [] _pmatch;
}

PacketParser::packet_type PacketParser::parse(const char* packet)
	throw(n_u::ParseException)
{

    _packetTime = n_u::UTime((time_t)0);
    _stationId = -1;
    _configId = -1;
    _sampleId = -1;

    int regstatus;

    // parse beginning of packet 
    for (_packetType = 0; _packetType < _nPacketTypes; _packetType++) {
	if ((regstatus = ::regexec(_packetPreg[_packetType],packet,
	    	_nmatch,_pmatch,0)) == 0) break;
    }
    if (_packetType == _nPacketTypes) {      // not a packet
	ostringstream ost;
	ost << "Bad packet (" << strlen(packet) << " bytes) \"" <<
		packet << "\"";
	throw n_u::ParseException(ost.str());
    }


    _packetPtr = packet;
    switch(_packetType) {
    case NESDIS_PT:
	{
	    int year,jday,hour,minute,sec;
	    assert(_pmatch[1].rm_so >= 0 && _pmatch[2].rm_so >= 0);

	    // hex goesid
	    if (sscanf(_packetPtr + _pmatch[1].rm_so,"%8x",&_stationId) != 1)
		throw n_u::ParseException(string("bad goesid in packet:") +
			string(_packetPtr + _pmatch[1].rm_so,8));

	    // date
	    if (sscanf(_packetPtr + _pmatch[2].rm_so,"%2d%3d%2d%2d%2d",
		    &year,&jday,&hour,&minute,&sec) != 5)
		throw n_u::ParseException(string("bad date in packet:") +
			string(_packetPtr + _pmatch[2].rm_so,11));

	    _packetTime = n_u::UTime(true,year,jday,hour,minute,sec);
	}
	break;
    }

    _packetPtr += _pmatch[0].rm_eo;
    // parse info fields
    int itype = 0;
    switch(_packetType) {
    case NESDIS_PT:
	for (itype = 0; itype < _nInfoTypes; itype++)
	    if ((regstatus = ::regexec(_infoPreg[itype],_packetPtr,
	    	_nmatch,_pmatch,0)) == 0) break;
	break;
    }

    if (itype == _nInfoTypes) {      // no match to any info type
	ostringstream ost;
	ost << "Bad packet (" << strlen(packet) << " bytes) \"" <<
		packet << "\"";
	throw n_u::ParseException(ost.str());
    }

    switch(itype) {
    case NESDIS_IT:
	assert(_pmatch[0].rm_so >= 0);
	if (itype != _infoType) {
	    delete _packetInfo;
	    _packetInfo = new NESDISPacketInfo();
	}
	break;
    case SUTRON_IT:
	assert(_pmatch[0].rm_so >= 0);
	if (itype != _infoType) {
	    delete _packetInfo;
	    _packetInfo = new SutronPacketInfo();
	}
	break;
    }
    _infoType = itype;

    _packetInfo->scan(_packetPtr + _pmatch[0].rm_so);
    _packetPtr += _pmatch[0].rm_eo;

    int stringLength = ::strlen(_packetPtr);

    // data bytes are in the range 0x40-0x77 (most signif. bits=01) 
    // NESDIS, or our download script, adds a space to the packet.
    // isspace looks for space,\f,\n,\r,\t and \v which are
    // not in the range 0x40-0x77, so we can trim any of them
    // off the end of the packet.
    while (::isspace(_packetPtr[stringLength-1])) stringLength--;

#ifdef DEBUG
    cerr << "packet getLength=" << _packetInfo->getLength() <<
    	" stringLength=" << stringLength << endl;
#endif

    int packetLength = std::min(_packetInfo->getLength(),stringLength);
    _endOfPacket = _packetPtr + packetLength;

    if (!(_packetInfo->getStatusInt() & 0xf)) {
	if (_packetPtr < _endOfPacket) _configId = *_packetPtr++ & 0x3f;
	if (_packetPtr < _endOfPacket) _sampleId = *_packetPtr++ & 0x3f;
    }

    return (packet_type) _packetType;
}

void PacketParser::parseData(float* fptr, int nvars)
{
    int i = 0;
    if (!(_packetInfo->getStatusInt() & 0xf)) {
	for ( ; i < nvars && _packetPtr+4 <= _endOfPacket; i++) {
	    *fptr++ = GOES::float_decode_4x6(_packetPtr);
	    _packetPtr += 4;
	}
    }
    for ( ; i < nvars; i++) *fptr++ = nidas::core::floatNAN;
}


// Constructor for NESDIS PacketInfo
NESDISPacketInfo::NESDISPacketInfo():
    _messageStatus(' '),_sigdbm(0),_freqError(99),
    _modIndex('?'),_dataQuality('?'),_channel(0),_EW('?'),
    _len(0)
{
}

bool NESDISPacketInfo::scan(const char *str)
{
    int nf = sscanf(str,"%c%2d%2d%c%c%3d%c%*2c%5d",
	      &_messageStatus,&_sigdbm,&_freqError,
	      &_modIndex,&_dataQuality,&_channel,&_EW,&_len);
    return nf == 8;
}

int NESDISPacketInfo::getStatusInt() const
{
    int status;
    // Appendix D of "Packet Data Collection System Automatic Processing System,
    // User Interface Manual", Version 1.0
    switch (_messageStatus) {
    case 'G': status = 0; break;	// good
    case '?': status = 1; break;	// parity errors
    case 'W': status = 2; break;	// wrong channel
    case 'D': status = 3; break;	// duplicate (multiple channels)
    case 'A': status = 4; break;	// address error (correctable)
    case 'B': status = 5; break;	// bad address
    case 'T': status = 6; break;	// time error, message received late/early
    case 'U': status = 7; break;	// unexpected message
    case 'M': status = 8; break;	// missing message
    case 'I': status = 9; break;	// invalid address (not in PDT)
    case 'N': status = 10; break;	// PDT entry for this platform is not complete
    case 'Q': status = 11; break;	// bad quality measurements
    case 'C': status = 12; break;	// comparison error on test retransmission
    default: status = 13; break;	// unknown
    }

    switch (_modIndex) {
    case 'N': break;			// normal, 60 deg +- 5
    case 'L': status |= 1 << 4; break;	// low, < 50 deg
    case 'H': status |= 2 << 4; break;	// high, > 70 deg
    default:  status |= 3 << 4; break;
    }

    switch (_dataQuality) {
    case 'N': break;			// normal, error rate better than 1E-6
    case 'F': status |= 1 << 6; break;	// fair, err rate between 1.e-4 and 1.e-6
    case 'P': status |= 2 << 6; break;	// poor, error rate worse than 1.e-4
    default:  status |= 3 << 6; break;
    }

    return status;
}

const char *NESDISPacketInfo::header() const {
  return "S dbm freq mod  dq chan   len";
}

ostream & NESDISPacketInfo::print(ostream &s) const {
  return s <<
	_messageStatus <<
	setw(4) << _sigdbm << 
	setw(5) << _freqError * 50 <<
	"   " << _modIndex <<
	"   " << _dataQuality <<
	setw(4) << _channel << _EW <<
	setw(6) << _len;
    ;
}

// Constructor for Sutron PacketInfo
SutronPacketInfo::SutronPacketInfo():
    _modNumber(0),_sigdbm(0.),_freqError(0),_modPhase(0),_SNratio(0.)
{
}

bool SutronPacketInfo::scan(const char *str)
{
    return sscanf(str,"%d%f%d%d%f",
    	&_modNumber,&_sigdbm,&_freqError,&_modPhase,&_SNratio) == 5;
}

const char *SutronPacketInfo::header() const {
    return "m#  dbm freq modP SNratio";
}
ostream & SutronPacketInfo::print(ostream &s) const {
    return s <<
	setw(2) << _modNumber <<
	setw(5) << _sigdbm <<
	setw(5) << _freqError <<
	setw(5) << _modPhase <<
	setw(5) << _SNratio;
}

