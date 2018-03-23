
#define BOOST_TEST_DYN_LINK
#include <boost/test/auto_unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/core/CalFile.h>
#include <cmath> // isnan

using std::isnan;
using namespace nidas::util;
using namespace nidas::core;


BOOST_AUTO_TEST_CASE(test_calfile_normal_read)
{
  CalFile cfile;

  cfile.setPath(".");
  cfile.setFile("T_2m.dat");

  UTime when(LONG_LONG_MIN);
  int n;
  float data[2];
  int ndata = sizeof(data)/sizeof(data[0]);

  n = cfile.readCF(when, data, ndata);
  BOOST_REQUIRE_EQUAL(n, 2);
  BOOST_CHECK_EQUAL(data[0], 0.0);
  BOOST_CHECK_EQUAL(data[1], 1.0);
}

BOOST_AUTO_TEST_CASE(test_calfile_read_strings)
{
  CalFile cfile;

  cfile.setName("offsets_angles");
  cfile.setPath(".");
  cfile.setFile("csat_tse07_10m.dat");

  BOOST_CHECK_EQUAL(cfile.getFile(), "csat_tse07_10m.dat");
  BOOST_CHECK_EQUAL(cfile.getName(), "offsets_angles");

  UTime when(LONG_LONG_MIN);
  int n;
  float data[8];
  int ndata = sizeof(data)/sizeof(data[0]);
  std::vector<std::string> fields;

  n = cfile.readCF(when, data, ndata, &fields);
  BOOST_CHECK_EQUAL(n, 8);
  BOOST_CHECK_EQUAL(data[7], 1.0);
  BOOST_REQUIRE_EQUAL(fields.size(), 9);
  BOOST_CHECK_EQUAL(fields[8], "flipped");

  when = cfile.nextTime();
  fields.clear();
  n = cfile.readCF(when, data, ndata, &fields);
  BOOST_CHECK_EQUAL(n, 8);
  BOOST_CHECK_EQUAL(data[7], 1.0);
  BOOST_REQUIRE_EQUAL(fields.size(), 9);
  BOOST_CHECK_EQUAL(fields[5], "16.70");
  BOOST_CHECK_EQUAL(fields[8], "normal");
}

BOOST_AUTO_TEST_CASE(test_calfile_na)
{
  CalFile cfile;

  cfile.setPath(".");
  cfile.setFile("T_2m.dat");

  UTime when(LONG_LONG_MIN);
  int n;
  float data[2];
  int ndata = sizeof(data)/sizeof(data[0]);
  std::vector<std::string> fields;

  n = cfile.readCF(when, data, ndata, &fields);
  BOOST_CHECK_EQUAL(n, 2);
  // Now read the next line with only one number
  when = cfile.nextTime();
  fields.clear();
  n = cfile.readCF(when, data, ndata, &fields);
  BOOST_CHECK_EQUAL(n, 1);
  BOOST_CHECK_EQUAL(data[0], 100.0);
  BOOST_CHECK(isnan(data[1]));

  BOOST_REQUIRE_EQUAL(fields.size(), 1);
  BOOST_CHECK_EQUAL(fields[0], "100.00");

  // A line with only a time.
  when = cfile.nextTime();
  fields.clear();
  n = cfile.readCF(when, data, ndata, &fields);
  BOOST_CHECK_EQUAL(n, 0);
  BOOST_CHECK(isnan(data[0]));
  BOOST_CHECK(isnan(data[1]));
  BOOST_CHECK_EQUAL(fields.size(), 0);

  // A line with explicit nans.
  when = cfile.nextTime();
  fields.clear();
  n = cfile.readCF(when, data, ndata, &fields);
  BOOST_CHECK_EQUAL(n, 2);
  BOOST_CHECK(isnan(data[0]));
  BOOST_CHECK(isnan(data[1]));
  BOOST_CHECK_EQUAL(fields.size(), 3);
  BOOST_CHECK_EQUAL(fields[0], "nan");
  BOOST_CHECK_EQUAL(fields[1], "NA");
  BOOST_CHECK_EQUAL(fields[2], "extra");
}

