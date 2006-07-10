//
//              Copyright 2004 (C) by UCAR
//

#ifndef  NIDAS_UTIL_BITARRAY_H
#define  NIDAS_UTIL_BITARRAY_H

#include <string>

namespace nidas { namespace util {

/**
 * A class for holding bits. Supports setting/getting individual bits
 * in an array. Could be used by an object that is setting/getting relays,
 * for example.
 */
class BitArray {

protected:

  /**
   * To make things portable across different machine
   * endiannesses we represent the bits in an array of chars,
   * rather than in integers.
   * Bits 0-7 are in bits[0], 8-15 in bits[1], etc.
   */
  unsigned char *bits;
  int lenBits;
  int lenBytes;

public:

    /**
     * Constructor, all bits will be initialized to 0.
     */
    BitArray(int lenbits): lenBits(lenbits),lenBytes((lenbits+7)/8) {
	bits = new unsigned char[lenBytes];
	setBits(0);
    }

    /**
     * Copy constructor.
     */
    BitArray(const BitArray& ba);

    /**
     * Assignment operator.
     */
    BitArray& operator = (const BitArray& ba);

    ~BitArray() {
	delete [] bits;
    }

    /**
     * Set all bits to 1 if value is true, otherwise set all to 0.
     */
    void setBits(bool value);

    /**
     * Set a bit, in range 0:(length()-1) to 1 if value is true,
     * otherwise false.  Silently ignores requests out of range.
     */
    void setBit(int num, bool value)
    {
	if (num >= getLengthInBytes() * 8) return;
	if (value) bits[num/8] |= 0x1 << (num%8);
	else bits[num/8] &= ~(0x1 << (num%8));
    }

    /**
     * Set all bits from begin, up to, but not including, end, 
     * to the corresponding bits in bitmask. Bit 0 from bitmask
     * is copied to begin, bit 1 to begin+1, etc.
     * @param begin Index of first bit, from 0, up to getLength()-1.
     * @param end Index of last bit, from begin, up to getLength()-1.
     * @param bitmask Long integer containing source bits
     *     to be copied, with bit 0 in the least significant byte.
     * If (end - begin) is greater than 32, then end is silently
     * adjusted to begin+32.
     */
    void setBits(int begin,int end, unsigned long bitmask);

    /**
     * Get value of a bit.
     * @param num Index of bit, in range 0:(getLength()-1).
     * Does not check for indices outside of range.
     */
    bool getBit(int num) const
    {
	if (num >= getLength()) return false;
	return (bits[num/8] & (0x1 << (num%8))) != 0;
    }

    unsigned long getBits(int begin, int end);

    /**
     * Return pointer to first byte of BitArray. This will
     * contain bits 0-7.
     */
    unsigned char* getPtr() { return bits; }

    /**
     * Return const pointer to first byte of BitArray. This will
     * contain bits 0-7.
     */
    const unsigned char* getConstPtr() const { return bits; }

    bool any() const
    {
        for (int i = 0; i < getLength(); i++)
	    if (getBit(i) != 0) return true;
	return false;
    }

    bool all() const
    {
        for (int i = 0; i < getLength(); i++)
	    if (getBit(i) == 0) return false;
	return true;
    }

    bool any(int begin,int end) const
    {
        for (int i = begin; i < std::min(end,getLength()); i++)
	    if (getBit(i) != 0) return true;
	return false;
    }

    bool all(int begin,int end) const
    {
        for (int i = begin; i < std::min(end,getLength()); i++)
	    if (getBit(i) == 0) return false;
	return true;
    }

    /**
     * Length of array, in bits.
     */
    int getLength() const { return lenBits; }

    /**
     * Length of array, in bytes.
     */
    int getLengthInBytes() const { return lenBytes; }

    std::string toString() const;

    BitArray& operator |= (const BitArray& x);

    BitArray operator | (const BitArray& x);

    BitArray& operator &= (const BitArray& x);

    BitArray operator & (const BitArray& x);

    BitArray& operator ^= (const BitArray& x);

    BitArray operator ^ (const BitArray& x);

};

BitArray operator | (const BitArray& x,const BitArray& y);

BitArray operator & (const BitArray& x,const BitArray& y);

BitArray operator ^ (const BitArray& x,const BitArray& y);

bool operator == (const BitArray& x,const BitArray& y);

bool operator != (const BitArray& x,const BitArray& y);

} }	// namespace nidas namespace util

#endif
