/*
 ******************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate: 2006-05-23 12:30:55 -0600 (Tue, 23 May 2006) $

    $LastChangedRevision: 3364 $

    $LastChangedBy: maclean $

    $HeadURL: http://svn/svn/nids/branches/nidas_reorg/src/nidas/dynld/DSMSerialSensor.cc $

 ******************************************************************
*/


#include <nidas/dynld/isff/GOES.h>

#include <iostream>
#include <iomanip>

using namespace std;
using namespace nidas::dynld::isff;

// #define DEBUG

void GOES::float_encode_5x6(float f,char* enc)
{
    unsigned char* uenc = (unsigned char*) enc;
    union {
        float f;
	unsigned char b[4];
    } uf;
    uf.f = f;

#ifdef DEBUG
    cout << "f=" << f << endl;
    cout << "dec=" << hex <<
    	setw(3) << (int)uf.b[0] <<
    	setw(3) << (int)uf.b[1] <<
    	setw(3) << (int)uf.b[2] <<
    	setw(3) << (int)uf.b[3] << dec << endl;
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
    char tmp = uf.b[0];
    uf.b[0] = uf.b[3];
    uf.b[3] = tmp;
    tmp = uf.b[1];
    uf.b[1] = uf.b[2];
    uf.b[2] = tmp;
#endif

    uenc[0] = ((uf.b[0] & 0xfc) >> 2) | 0x40;	// toss least significat 2 bits
    uenc[1] = (uf.b[1] & 0x3f) | 0x40;
    uenc[2] = ((uf.b[1] & 0xc0) >> 6) | ((uf.b[2] & 0x0f) << 2) | 0x40;
    uenc[3] = ((uf.b[2] & 0xf0) >> 4) | ((uf.b[3] & 0x03) << 4) | 0x40;
    uenc[4] = ((uf.b[3] & 0xfc) >> 2) | 0x40;

#ifdef DEBUG
    cout << "uenc=" << hex <<
    	setw(3) << (int)uenc[0] <<
    	setw(3) << (int)uenc[1] <<
    	setw(3) << (int)uenc[2] <<
    	setw(3) << (int)uenc[3] <<
    	setw(3) << (int)uenc[4] << dec << endl;
#endif
}

float GOES::float_decode_5x6(const char* enc)
{
    const unsigned char* uenc = (const unsigned char*) enc;

#ifdef DEBUG
    cout << "uenc=" << hex <<
    	setw(3) << (int)uenc[0] <<
    	setw(3) << (int)uenc[1] <<
    	setw(3) << (int)uenc[2] <<
    	setw(3) << (int)uenc[3] <<
    	setw(3) << (int)uenc[4] << dec << endl;
#endif

    union {
        float f;
	unsigned char b[4];
    } uf;

    uf.b[0] = (uenc[0] & 0x3f) << 2;
    uf.b[1] = (uenc[1] & 0x3f) | (uenc[2] & 0x03) << 6;
    uf.b[2] = (uenc[2] & 0x3c) >> 2 | (uenc[3] & 0x0f) << 4;
    uf.b[3] = (uenc[3] & 0x30) >> 4 | (uenc[4] & 0x3f) << 2;

#if __BYTE_ORDER == __BIG_ENDIAN
    char tmp = uf.b[0];
    uf.b[0] = uf.b[3];
    uf.b[3] = tmp;
    tmp = uf.b[1];
    uf.b[1] = uf.b[2];
    uf.b[2] = tmp;
#endif

#ifdef DEBUG
    cout << "uf.f=" << uf.f << endl;
    cout << "dec=" << hex <<
    	setw(3) << (int)uf.b[0] <<
    	setw(3) << (int)uf.b[1] <<
    	setw(3) << (int)uf.b[2] <<
    	setw(3) << (int)uf.b[3] << dec << endl;
#endif

    return uf.f;
}

void GOES::float_encode_4x6(float f,char* enc)
{
    unsigned char* uenc = (unsigned char*) enc;
    union {
        float f;
	unsigned char b[4];
    } uf;
    uf.f = f;

#ifdef DEBUG
    cout << "f=" << f << endl;
    cout << "dec=" << hex <<
    	setw(3) << (int)uf.b[0] <<
    	setw(3) << (int)uf.b[1] <<
    	setw(3) << (int)uf.b[2] <<
    	setw(3) << (int)uf.b[3] << dec << endl;
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
    char tmp = uf.b[0];
    uf.b[0] = uf.b[3];
    uf.b[3] = tmp;
    tmp = uf.b[1];
    uf.b[1] = uf.b[2];
    uf.b[2] = tmp;
#endif

    uenc[0] = (uf.b[1] & 0x3f) | 0x40;	// toss least significat 8 bits
    uenc[1] = ((uf.b[1] & 0xc0) >> 6) | ((uf.b[2] & 0x0f) << 2) | 0x40;
    uenc[2] = ((uf.b[2] & 0xf0) >> 4) | ((uf.b[3] & 0x03) << 4) | 0x40;
    uenc[3] = ((uf.b[3] & 0xfc) >> 2) | 0x40;

#ifdef DEBUG
    cout << "enc=" << hex <<
    	setw(3) << (int)enc[0] <<
    	setw(3) << (int)enc[1] <<
    	setw(3) << (int)enc[2] <<
    	setw(3) << (int)enc[3] << dec << endl;
#endif
}

float GOES::float_decode_4x6(const char* enc)
{
    const unsigned char* uenc = (const unsigned char*) enc;
#ifdef DEBUG
    cout << "enc=" << hex <<
    	setw(3) << (int)uenc[0] <<
    	setw(3) << (int)uenc[1] <<
    	setw(3) << (int)uenc[2] <<
    	setw(3) << (int)uenc[3] << dec << endl;
#endif

    union {
        float f;
	unsigned char b[4];
    } uf;

    uf.b[0] = 0;
    uf.b[1] = (uenc[0] & 0x3f) | (uenc[1] & 0x03) << 6;
    uf.b[2] = (uenc[1] & 0x3c) >> 2 | (uenc[2] & 0x0f) << 4;
    uf.b[3] = (uenc[2] & 0x30) >> 4 | (uenc[3] & 0x3f) << 2;

#if __BYTE_ORDER == __BIG_ENDIAN
    char tmp = uf.b[0];
    uf.b[0] = uf.b[3];
    uf.b[3] = tmp;
    tmp = uf.b[1];
    uf.b[1] = uf.b[2];
    uf.b[2] = tmp;
#endif

#ifdef DEBUG
    cout << "uf.f=" << uf.f << endl;
    cout << "dec=" << hex <<
    	setw(3) << (int)uf.b[0] <<
    	setw(3) << (int)uf.b[1] <<
    	setw(3) << (int)uf.b[2] <<
    	setw(3) << (int)uf.b[3] << dec << endl;
#endif

    return uf.f;
}
