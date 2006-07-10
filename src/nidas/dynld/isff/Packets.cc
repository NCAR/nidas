/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-23 12:30:55 -0600 (Tue, 23 May 2006) $

    $LastChangedRevision: 3364 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/dynld/SampleInputStream.cc $
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
::regex_t** PacketParser::packetPreg = 0;

/* static */
::regex_t** PacketParser::infoPreg = 0;

/* static */
nidas::util::Mutex PacketParser::pregMutex;

PacketParser::PacketParser() throw(n_u::ParseException):
    nmatch(10),	// max number of parenthesized expressions
    pmatch(0),infoType(-1),packetInfo(0),stationId(-1)
{
    // 
    pmatch = new ::regmatch_t[nmatch];

    n_u::Autolock autolock(pregMutex);

    // Check if regular expressions need compiling
    if (!packetPreg) {
	int regstatus;

	const char *packetRE[] = {
// Extended regular expression for parsing a Packet NESDIS message
// stnid         yr           doy             hr        mn            sc
"^([0-9A-F]{8})([0-9]{2})([0-3][0-9][0-9])([0-2][0-9])([0-5][0-9])([0-5][0-9]) *",
	};

	nPacketTypes = sizeof(packetRE) / sizeof(packetRE[0]);
	packetPreg = new ::regex_t *[nPacketTypes];

	for (int i = 0; i < nPacketTypes; i++) {
	    packetPreg[i] = new ::regex_t;
	    if ((regstatus = ::regcomp(packetPreg[i],packetRE[i],REG_EXTENDED))
		!= 0) {
		delete packetPreg[i]; packetPreg[i] = 0;
		char regerrbuf[64];
		::regerror(regstatus,packetPreg[i],regerrbuf,sizeof regerrbuf);
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

	nInfoTypes = sizeof(infoRE) / sizeof(infoRE[0]);
	infoPreg = new ::regex_t *[nInfoTypes];

	for (int i = 0; i < nInfoTypes; i++) {
	    infoPreg[i] = new ::regex_t;
	    if ((regstatus = ::regcomp(infoPreg[i],infoRE[i],REG_EXTENDED))
		!= 0) {
		delete infoPreg[i]; infoPreg[i] = 0;
		char regerrbuf[64];
		::regerror(regstatus,infoPreg[i],regerrbuf,sizeof regerrbuf);
		throw n_u::ParseException(string("regcomp: ") + regerrbuf + ": " +
			infoRE[i]);
	    }
	}
    }
}

PacketParser::~PacketParser()
{
    delete [] pmatch;
}

PacketParser::packet_type PacketParser::parse(const char* packet)
	throw(n_u::ParseException)
{

    packetTime = n_u::UTime((time_t)0);
    stationId = -1;

    int regstatus;

    // parse beginning of packet 
    for (packetType = 0; packetType < nPacketTypes; packetType++) {
	if ((regstatus = ::regexec(packetPreg[packetType],packet,
	    	nmatch,pmatch,0)) == 0) break;
    }
    if (packetType == nPacketTypes) {      // not a packet
	ostringstream ost;
	ost << "Bad packet (" << strlen(packet) << " bytes) \"" <<
		packet << "\"";
	throw n_u::ParseException(ost.str());
    }


    packetPtr = packet;
    switch(packetType) {
    case NESDIS_PT:
	{
	    int year,jday,hour,minute,sec;
	    assert(pmatch[1].rm_so >= 0 && pmatch[2].rm_so >= 0);

	    sscanf(packetPtr + pmatch[1].rm_so,"%8x",&stationId);	// goesid
	    sscanf(packetPtr + pmatch[2].rm_so,"%2d%3d%2d%2d%2d",
		    &year,&jday,&hour,&minute,&sec);
	    packetTime = n_u::UTime(true,year,jday,hour,minute,sec);
	}
	break;
    }

    packetPtr += pmatch[0].rm_eo;
    // parse info fields
    int itype = 0;
    switch(packetType) {
    case NESDIS_PT:
	for (itype = 0; itype < nInfoTypes; itype++)
	    if ((regstatus = ::regexec(infoPreg[itype],packetPtr,
	    	nmatch,pmatch,0)) == 0) break;
	break;
    }

    if (itype == nInfoTypes) {      // not a packet
	ostringstream ost;
	ost << "Bad packet (" << strlen(packet) << " bytes) \"" <<
		packet << "\"";
	throw n_u::ParseException(ost.str());
    }

    switch(itype) {
    case NESDIS_IT:
	assert(pmatch[0].rm_so >= 0);
	if (itype != infoType) {
	    delete packetInfo;
	    packetInfo = new NESDISPacketInfo();
	}
	break;
    case SUTRON_IT:
	assert(pmatch[0].rm_so >= 0);
	if (itype != infoType) {
	    delete packetInfo;
	    packetInfo = new SutronPacketInfo();
	}
	break;
    }
    infoType = itype;

    packetInfo->scan(packetPtr + pmatch[0].rm_so);
    packetPtr += pmatch[0].rm_eo;

    int stringLength = ::strlen(packetPtr);

    // data bytes are in the range 0x40-0x77 (most signif. bits=01) 
    // NESDIS, or our download script, adds a space to the packet.
    // isspace looks for space,\f,\n,\r,\t and \v which are
    // not in the range 0x40-0x77, so we can trim any of them
    // off the end of the packet.
    while (::isspace(packetPtr[stringLength-1])) stringLength--;

#ifdef DEBUG
    cerr << "packet getLength=" << packetInfo->getLength() <<
    	" stringLength=" << stringLength << endl;
#endif

    int packetLength = std::min(packetInfo->getLength(),stringLength);
    endOfPacket = packetPtr + packetLength;

    if (packetPtr < endOfPacket) configId = *packetPtr++ & 0x3f;
    else configId = -1;

    if (packetPtr < endOfPacket) sampleId = *packetPtr++ & 0x3f;
    else sampleId = -1;

    return (packet_type) packetType;
}

void PacketParser::parseData(float* fptr, int nvars)
{
    int i;
    for (i = 0; i < nvars && packetPtr+4 <= endOfPacket; i++) {
	*fptr++ = GOES::float_decode_4x6(packetPtr);
	packetPtr += 4;
    }
    for ( ; i < nvars; i++) *fptr++ = nidas::core::floatNAN;
}


// Constructor for NESDIS PacketInfo
NESDISPacketInfo::NESDISPacketInfo():
    messageStatus(' '),sigdbm(0),freqError(99),
    modIndex('?'),dataQuality('?'),channel(0),EW('?'),
    len(0)
{
}

bool NESDISPacketInfo::scan(const char *str)
{
    int nf = sscanf(str,"%c%2d%2d%c%c%3d%c%*2c%5d",
	      &messageStatus,&sigdbm,&freqError,
	      &modIndex,&dataQuality,&channel,&EW,&len);
    if (nf == 8) len -= 5;	// length includes 5 length digits
    return nf == 8;
}

int NESDISPacketInfo::getStatusInt() const
{
    int status;
    // Appendix D of "Packet Data Collection System Automatic Processing System,
    // User Interface Manual", Version 1.0
    switch (messageStatus) {
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

    switch (modIndex) {
    case 'N': break;			// normal, 60 deg +- 5
    case 'L': status |= 1 << 4; break;	// low, < 50 deg
    case 'H': status |= 2 << 4; break;	// high, > 70 deg
    default:  status |= 3 << 4; break;
    }

    switch (dataQuality) {
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
	messageStatus <<
	setw(4) << sigdbm << 
	setw(5) << freqError * 50 <<
	"   " << modIndex <<
	"   " << dataQuality <<
	setw(4) << channel << EW <<
	setw(6) << len;
    ;
}

// Constructor for Sutron PacketInfo
SutronPacketInfo::SutronPacketInfo():
    modNumber(0),sigdbm(0.),freqError(0),modPhase(0),SNratio(0.)
{
}

bool SutronPacketInfo::scan(const char *str)
{
    return sscanf(str,"%d%f%d%d%f",
    	&modNumber,&sigdbm,&freqError,&modPhase,&SNratio) == 5;
}

const char *SutronPacketInfo::header() const {
    return "m#  dbm freq modP SNratio";
}
ostream & SutronPacketInfo::print(ostream &s) const {
    return s <<
	setw(2) << modNumber <<
	setw(5) << sigdbm <<
	setw(5) << freqError <<
	setw(5) << modPhase <<
	setw(5) << SNratio;
}

