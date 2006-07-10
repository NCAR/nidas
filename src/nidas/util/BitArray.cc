//
//              Copyright 2004 (C) by UCAR
//


#include <nidas/util/BitArray.h>

#include <sstream>
#include <iostream>
#include <iomanip>

using namespace std;
using namespace nidas::util;

/** Copy constructor */
BitArray::BitArray(const BitArray& ba) {
  lenBits = ba.lenBits;
  lenBytes = ba.lenBytes;
  bits = new unsigned char[lenBytes];
  ::memcpy(bits,ba.bits,lenBytes);
}

/**
 * Assignment operator.
 */
BitArray& BitArray::operator = (const BitArray& ba) {
  // expand if necessary
  if (lenBytes < ba.lenBytes) {
    delete [] bits;
    bits = new unsigned char[ba.lenBytes];
    lenBytes = ba.lenBytes;
  }
  lenBits = ba.lenBits;
  ::memset(bits,0,lenBytes);
  ::memcpy(bits,ba.bits,ba.lenBytes);
  return *this;
}

void BitArray::setBits(bool value)
{
    unsigned char val = value ? 0xff : 0x0;
    for (int i = 0; i < getLengthInBytes(); i++) bits[i] = val;
    // zero out extra high-end bits
    if (value && (getLength() % 8) != 0) {
	for (int i = getLength(); i < getLengthInBytes() * 8; i++)
	    setBit(i,false);
    }
}

void BitArray::setBits(int begin,int end, unsigned long bitmask)
{
    end = std::min(std::min(end,
    	begin + (signed)sizeof(bitmask) * 8),getLength());
    unsigned long mask = 1;
    for (int i = begin; i < end; i++) {
	if (mask & bitmask) setBit(i,true);
	else setBit(i,false);
	mask <<= 1;
    }
}

unsigned long BitArray::getBits(int begin, int end) 
{
    unsigned long res = 0;
    end = std::min(std::min(end,begin + (signed)sizeof(res) * 8),
	    getLength());
    if (begin >= end) return res;

    if ((begin % 8) == 0) {
	int nb = (end - begin + 7) / 8;
	::memcpy(&res,bits + begin/8,nb);
	if (end % 8) {
	}
    }
    else {
	unsigned long mask = 1;
	for (int i = begin; i < end; i++) {
	    if (getBit(i)) res |= mask;
	    mask <<= 1;
	}
    }
    return res;
}

std::string BitArray::toString() const {

  std::ostringstream os;
  for (int i = getLength(); i-- > 0; ) 
    os << std::setw(3) << i;
  os << std::endl;

  for (int i = getLength(); i-- > 0; ) 
    os << std::setw(3) << getBit(i);
  return os.str();
}

bool nidas::util::operator == (const BitArray& x,const BitArray& y)
{
    const unsigned char* xptr = x.getConstPtr();
    const unsigned char* yptr = y.getConstPtr();

    int lbit = std::min(x.getLength(),y.getLength());

    int lbyte = std::min(x.getLengthInBytes(),y.getLengthInBytes());
    if ((lbit % 8) != 0) lbyte--;
    int i;
    for (i = 0; i < lbyte; i++) if (xptr[i] != yptr[i]) return false;

    // partial high order byte
    if ((lbit % 8) != 0) {
	unsigned char mask = 0;
	for (int k = 0; k < lbit % 8; k++) mask |= k << 1;
	if ((mask & xptr[i]) != (mask & yptr[i])) return false;
    }
    return true;
}

bool nidas::util::operator != (const BitArray& x,const BitArray& y)
{
    return !operator == (x,y);
}

BitArray nidas::util::operator | (const BitArray& x,const BitArray& y)
{
    const unsigned char* xptr = x.getConstPtr();
    const unsigned char* yptr = y.getConstPtr();

    int lbit = std::min(x.getLength(),y.getLength());
    BitArray res(lbit);
    unsigned char* rptr = res.getPtr();

    int lbyte = std::min(x.getLengthInBytes(),y.getLengthInBytes());
    if ((lbit % 8) != 0) lbyte--;
    int i;
    for (i = 0; i < lbyte; i++) rptr[i] = xptr[i] | yptr[i];

    // partial high order byte
    if ((lbit % 8) != 0) {
	unsigned char mask = 0;
	for (int k = 0; k < lbit % 8; k++) mask |= k << 1;
	rptr[i] = mask & (xptr[i] | yptr[i]);
    }
    return res;
}

BitArray BitArray::operator | (const BitArray& y)
{
    const unsigned char* xptr = getConstPtr();
    const unsigned char* yptr = y.getConstPtr();

    int lbit = std::min(getLength(),y.getLength());
    BitArray res(lbit);
    unsigned char* rptr = res.getPtr();

    int lbyte = std::min(getLengthInBytes(),y.getLengthInBytes());
    if ((lbit % 8) != 0) lbyte--;
    int i;
    for (i = 0; i < lbyte; i++) rptr[i] = xptr[i] | yptr[i];

    // partial high order byte
    if ((lbit % 8) != 0) {
	unsigned char mask = 0;
	for (int k = 0; k < lbit % 8; k++) mask |= k << 1;
	rptr[i] = mask & (xptr[i] | yptr[i]);
    }
    return res;
}

BitArray& BitArray::operator |= (const BitArray& y)
{
    unsigned char* xptr = getPtr();
    const unsigned char* yptr = y.getConstPtr();

    int lbyte = std::min(getLengthInBytes(),y.getLengthInBytes());
    int lbit = std::min(getLength(),y.getLength());
    if ((lbit % 8) != 0) lbyte--;
    int i;
    for (i = 0; i < lbyte; i++) xptr[i] |= yptr[i];

    // partial high order byte
    if ((lbit % 8) != 0) {
	unsigned char mask = 0;
	for (int k = 0; k < lbit % 8; k++) mask |= k << 1;
	xptr[i] |= (mask & yptr[i]);
    }
    return *this;
}

BitArray nidas::util::operator & (const BitArray& x,const BitArray& y)
{
    const unsigned char* xptr = x.getConstPtr();
    const unsigned char* yptr = y.getConstPtr();

    int lbit = std::min(x.getLength(),y.getLength());
    BitArray res(lbit);
    unsigned char* rptr = res.getPtr();

    int lbyte = std::min(x.getLengthInBytes(),y.getLengthInBytes());
    if ((lbit % 8) != 0) lbyte--;
    int i;
    for (i = 0; i < lbyte; i++)  rptr[i] = xptr[i] & yptr[i];

    // partial high order byte
    if ((lbit % 8) != 0) {
	unsigned char mask = 0;
	for (int k = 0; k < lbit % 8; k++) mask |= k << 1;
	rptr[i] = mask & (xptr[i] & yptr[i]);
    }
    return res;
}

BitArray BitArray::operator & (const BitArray& y)
{
    const unsigned char* xptr = getConstPtr();
    const unsigned char* yptr = y.getConstPtr();

    int lbit = std::min(getLength(),y.getLength());
    BitArray res(lbit);
    unsigned char* rptr = res.getPtr();

    int lbyte = std::min(getLengthInBytes(),y.getLengthInBytes());
    if ((lbit % 8) != 0) lbyte--;
    int i;
    for (i = 0; i < lbyte; i++)  rptr[i] = xptr[i] & yptr[i];

    // partial high order byte
    if ((lbit % 8) != 0) {
	unsigned char mask = 0;
	for (int k = 0; k < lbit % 8; k++) mask |= k << 1;
	rptr[i] = mask & (xptr[i] & yptr[i]);
    }
    return res;
}
BitArray& BitArray::operator &= (const BitArray& y)
{
    unsigned char* xptr = getPtr();
    const unsigned char* yptr = y.getConstPtr();

    int lbyte = std::min(getLengthInBytes(),y.getLengthInBytes());
    int lbit = std::min(getLength(),y.getLength());
    if ((lbit % 8) != 0) lbyte--;
    int i;
    for (i = 0; i < lbyte; i++)  xptr[i] &= yptr[i];

    // partial high order byte
    if ((lbit % 8) != 0) {
	unsigned char mask = 0;
	for (int k = 0; k < lbit % 8; k++) mask |= k << 1;
	xptr[i] &= (mask & yptr[i]);
    }
    return *this;
}

BitArray nidas::util::operator ^ (const BitArray& x,const BitArray& y)
{
    const unsigned char* xptr = x.getConstPtr();
    const unsigned char* yptr = y.getConstPtr();

    int lbit = std::min(x.getLength(),y.getLength());
    BitArray res(lbit);
    unsigned char* rptr = res.getPtr();

    int lbyte = std::min(x.getLengthInBytes(),y.getLengthInBytes());
    if ((lbit % 8) != 0) lbyte--;
    int i;
    for (i = 0; i < lbyte; i++)  rptr[i] = xptr[i] ^ yptr[i];

    // partial high order byte
    if ((lbit % 8) != 0) {
	unsigned char mask = 0;
	for (int k = 0; k < lbit % 8; k++) mask |= k << 1;
	rptr[i] = mask & (xptr[i] ^ yptr[i]);
    }
    return res;
}


BitArray BitArray::operator ^ (const BitArray& y)
{
    const unsigned char* xptr = getConstPtr();
    const unsigned char* yptr = y.getConstPtr();

    int lbit = std::min(getLength(),y.getLength());
    BitArray res(lbit);
    unsigned char* rptr = res.getPtr();

    int lbyte = std::min(getLengthInBytes(),y.getLengthInBytes());
    if ((lbit % 8) != 0) lbyte--;
    int i;
    for (i = 0; i < lbyte; i++)  rptr[i] = xptr[i] ^ yptr[i];

    // partial high order byte
    if ((lbit % 8) != 0) {
	unsigned char mask = 0;
	for (int k = 0; k < lbit % 8; k++) mask |= k << 1;
	rptr[i] = mask & (xptr[i] ^ yptr[i]);
    }
    return res;
}
BitArray& BitArray::operator ^= (const BitArray& y)
{
    unsigned char* xptr = getPtr();
    const unsigned char* yptr = y.getConstPtr();

    int lbyte = std::min(getLengthInBytes(),y.getLengthInBytes());
    int lbit = std::min(getLength(),y.getLength());
    if ((lbit % 8) != 0) lbyte--;
    int i;
    for (i = 0; i < lbyte; i++) xptr[i] ^= yptr[i];

    // partial high order byte
    if ((lbit % 8) != 0) {
	unsigned char mask = 0;
	for (int k = 0; k < (lbit % 8); k++) mask |= k << 1;
	xptr[i] = (mask & (xptr[i] ^ yptr[i]));
    }
    return *this;
}
