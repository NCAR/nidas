/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2009, Copyright University Corporation for Atmospheric Research
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

#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/auto_unit_test.hpp>

//#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <boost/regex.hpp>

#include "nidas/core/IOStream.h"
#include "nidas/core/UnixIOChannel.h"
#include "nidas/util/Logger.h"
#include <sstream>
#include <errno.h>

using namespace boost;
using namespace nidas::util;

static std::ostringstream oss;

using nidas::core::IOStream;
using nidas::core::UnixIOChannel;

class DummyChannel : public nidas::core::UnixIOChannel
{
public:
  DummyChannel(const std::string& name) : 
    UnixIOChannel(name, 0)
  {
    _buffer = 0;
    _buflen = 0;
    _partial = false; // turn on partial writes
    _nwrites = 0;
  }

  ~DummyChannel()
  {
    clear();
  }

  virtual size_t
  write(const void* buf, size_t len) throw (nidas::util::IOException)
  {
    if (_partial)
      len = len / 2;
    if (!_buffer)
    {
      _buffer = (char*)malloc(len);
    }
    else
    {
      _buffer = (char*)realloc(_buffer, _buflen + len);
    }
    memcpy(_buffer+_buflen, buf, len);
    _buflen += len;
    ++_nwrites;
    return len;
  }

  void
  clear()
  {
    if (_buffer) free(_buffer);
    _buffer = 0;
    _buflen = 0;
    _nwrites = 0;
  }

  char* _buffer;
  unsigned int _buflen;
  bool _partial;
  int _nwrites;
};



BOOST_AUTO_TEST_CASE(test_write)
{
  std::vector<float> numbers;
  for (int i = 0; i < 8000; ++i)
    numbers.push_back((float)i);

  DummyChannel channel("null");
  IOStream iostream(channel);

  BOOST_CHECK_EQUAL(iostream.available(), 0u);

  // write one small sample and wait, with flush=true: make sure it gets written right away,
  // buffer should be empty afterwards

  BOOST_CHECK_EQUAL(4u*60, iostream.write(&numbers[0], 4u*60,true));
  BOOST_CHECK_EQUAL(iostream.available(),0u);
  BOOST_CHECK_EQUAL(channel._buflen, 4u*60);
  channel.clear();

  // write two small samples and wait: both samples should be in buffer
  BOOST_CHECK_EQUAL(4u*80, iostream.write(&numbers[0],4u*80,false));
  BOOST_CHECK_EQUAL(4u*80, iostream.write(&numbers[0],4u*80,false));
  BOOST_CHECK_EQUAL(iostream.available(),4u*80 + 4*80);
  BOOST_CHECK_EQUAL(channel._buflen,0u);
  channel.clear();

  // write another small sample, with flush=true: all three samples should be written, and
  // buffer should be empty

  BOOST_CHECK_EQUAL(4u*100, iostream.write(&numbers[0], 4u*100,true));
  BOOST_CHECK_EQUAL(iostream.available(),0u);
  BOOST_CHECK_EQUAL(channel._buflen, 4u*(100+80+80));
  channel.clear();

  // immediately write a big block, it should write right away because
  // the buffer will be more than half full
  BOOST_CHECK_EQUAL(4*numbers.size(), 
		    iostream.write(&numbers[0], 4*numbers.size(),false));
  BOOST_CHECK_EQUAL(iostream.available(),0u);
  BOOST_CHECK_EQUAL(channel._buflen,4*numbers.size());
  channel.clear();

  // immediately write just under half the current buffer size, so it's
  // not written
  size_t wlen = 4*(numbers.size()/2 - 1);
  BOOST_CHECK_EQUAL(wlen, iostream.write(&numbers[0], wlen,false));
  BOOST_CHECK_EQUAL(iostream.available(), wlen);
  BOOST_CHECK_EQUAL(channel._buflen,0u);
  channel.clear();

  // write another big block with flush=true: both blocks should be written in separate writes,
  // but buffer will be empty afterwards
  BOOST_CHECK_EQUAL(4*numbers.size(),
		    iostream.write(&numbers[0], 4*numbers.size(),true));
  BOOST_CHECK_EQUAL(iostream.available(),0u);
  BOOST_CHECK_EQUAL(channel._nwrites, 2);
  BOOST_CHECK_EQUAL(channel._buflen, wlen + 4*numbers.size());
  channel.clear();
}


BOOST_AUTO_TEST_CASE(test_partial_writes)
{
  size_t n;
  std::vector<float> numbers;
  for (int i = 0; i < 8000; ++i)
    numbers.push_back((float)i);

  DummyChannel channel("null");
  IOStream iostream(channel);

  // Turn on partial writes.
  channel._partial = true;

  // Try to write a big block.  Only half will make it out, leaving the rest
  // in the top half of the iostream buffer.
  n = iostream.write(&numbers[0], 4*numbers.size(),false);
  BOOST_CHECK_EQUAL(n, 4*numbers.size());
  BOOST_CHECK_EQUAL(iostream.available(), 2*numbers.size());
  BOOST_CHECK_EQUAL(channel._buflen, 2*numbers.size());
  BOOST_CHECK(memcmp(&numbers[0], channel._buffer, 2*numbers.size()) == 0);
  channel.clear();

  // Write another big block.  It should succeed, but only after first
  // forcing the current half-block out.
  channel._partial = false;
  iostream.write(&numbers[0], 4*numbers.size(),false);
  BOOST_CHECK_EQUAL(iostream.available(),0u);
  BOOST_CHECK_EQUAL(channel._buflen, 6*numbers.size());
  BOOST_CHECK(memcmp(&numbers[numbers.size()/2], 
		     channel._buffer, 2*numbers.size()) == 0);
  BOOST_CHECK(memcmp(&numbers[0], 
		     channel._buffer + 2*numbers.size(),
		     4*numbers.size()) == 0);
  channel.clear();

  // Now do the same, only this time force the first half-block to be shifted
  // down to make room for a smaller block.
  channel._partial = true;
  n = iostream.write(&numbers[0], 4*numbers.size(),false);
  BOOST_CHECK_EQUAL(n, 4*numbers.size());
  BOOST_CHECK_EQUAL(iostream.available(), 2*numbers.size());
  BOOST_CHECK_EQUAL(channel._buflen, 2*numbers.size());
  BOOST_CHECK(memcmp(&numbers[0], channel._buffer, 2*numbers.size()) == 0);
  BOOST_CHECK_EQUAL(channel._nwrites, 1);
  channel.clear();

  channel._partial = false;
  n = iostream.write(&numbers[0], 1*numbers.size(),false);
  BOOST_CHECK_EQUAL(n, 1*numbers.size());
  BOOST_CHECK_EQUAL(iostream.available(),0u);
  BOOST_CHECK_EQUAL(channel._buflen, 3*numbers.size());
  BOOST_CHECK_EQUAL(channel._nwrites, 1);
  channel.clear();
}


BOOST_AUTO_TEST_CASE(test_steady_writes)
{
  std::vector<float> numbers;
  for (int i = 0; i < 25; ++i)
    numbers.push_back((float)i);

  DummyChannel channel("null");
  IOStream iostream(channel, 4096);

  // Write a continuous stream of smaller packets and check that writes get
  // bufferred up.  The buffer size is 8192, so half is 4096, so writing 41
  // blocks of 25 floats each will hit 4100 and drain the buffer.  The next
  // write will start at the beginning of the buffer again.  41 more blocks
  // will trigger another write.  So after 82 writes, there should have
  // been two write calls to the channel, 0 bytes left in the iostream
  // buffer, and 82*100 = 8200 bytes written to the channel.

  unsigned int wlen = 4*numbers.size();

  // The first block gets written right away, so write it and clear it out.
  BOOST_CHECK_EQUAL(iostream.write(&numbers[0], wlen,true), wlen);
  channel.clear();

  for (int i = 0; i < 41; ++i)
  {
    BOOST_CHECK_EQUAL(channel._nwrites, 0);
    BOOST_CHECK_EQUAL(iostream.write(&numbers[0], wlen, false), wlen);
  }
  BOOST_CHECK_EQUAL(channel._buflen, 4100u);
  BOOST_CHECK_EQUAL(channel._nwrites, 1);
  for (int i = 0; i < 41; ++i)
  {
    BOOST_CHECK_EQUAL(iostream.write(&numbers[0], wlen, false), wlen);
  }
  BOOST_CHECK_EQUAL(channel._buflen, 8200u);
  BOOST_CHECK_EQUAL(channel._nwrites, 2);
  BOOST_CHECK_EQUAL(iostream.available(), 0u);
}

