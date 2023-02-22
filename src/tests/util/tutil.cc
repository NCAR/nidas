#define BOOST_TEST_DYN_LINK
#define BOOST_AUTO_TEST_MAIN
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/util/EndianConverter.h>


using namespace nidas::util;


BOOST_AUTO_TEST_CASE(test_endianconverter)
{
    const EndianConverter* convert = EndianConverter::getConverter(
        EndianConverter::EC_LITTLE_ENDIAN, EndianConverter::EC_BIG_ENDIAN);

    BOOST_CHECK(convert);

    unsigned int one {1};
    unsigned int two {0};

    convert->int32Copy(one, &two);
    BOOST_CHECK_EQUAL(two, 1 << 24);
}

