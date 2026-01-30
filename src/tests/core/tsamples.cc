
#define BOOST_TEST_DYN_LINK
#include <boost/test/unit_test.hpp>
using boost::unit_test_framework::test_suite;

#include <nidas/core/Sample.h>

#include <limits>
#include <sstream>

using namespace nidas::core;


template <typename T>
void check_sample_type(sampleType expectedType)
{
    SampleT<T> samp;
    sampleType enum_value{sample_type_traits<T>::sample_type_enum};
    BOOST_CHECK_EQUAL(samp.getType(), expectedType);
    BOOST_CHECK_EQUAL(enum_value, expectedType);
    BOOST_CHECK_EQUAL(SampleT<T>::sizeofDataType(), sizeof(T));
    samp.allocateData(10);
    BOOST_CHECK_EQUAL(samp.getAllocByteLength(), 10 * sizeof(T));
    samp.setValues({ T(1), T(2), T(3), T(4), T(5) });
    BOOST_CHECK_EQUAL(samp.getDataLength(), 5);
    for (unsigned int i = 0; i < samp.getDataLength(); ++i)
    {
        BOOST_CHECK_EQUAL(samp.getDataValue(i), (double)T(i + 1));
    }
    // reallocating should keep values, different data pointer
    void* oldDataPtr = samp.getVoidDataPtr();
    samp.reallocateData(15);
    BOOST_CHECK_NE(samp.getVoidDataPtr(), oldDataPtr);
    BOOST_CHECK_EQUAL(samp.getDataLength(), 5);
    for (unsigned int i = 0; i < samp.getDataLength(); ++i)
    {
        BOOST_CHECK_EQUAL(samp.getDataValue(i), (double)T(i + 1));
    }
    BOOST_CHECK_EQUAL(samp.getAllocByteLength(), 15 * sizeof(T));
}


BOOST_AUTO_TEST_CASE(test_sample_type)
{
    check_sample_type<char>(CHAR_ST);
    check_sample_type<unsigned char>(UCHAR_ST);
    check_sample_type<short>(SHORT_ST);
    check_sample_type<unsigned short>(USHORT_ST);
    check_sample_type<int32_t>(INT32_ST);
    check_sample_type<uint32_t>(UINT32_ST);
    check_sample_type<int>(INT32_ST);
    check_sample_type<unsigned int>(UINT32_ST);
    check_sample_type<float>(FLOAT_ST);
    check_sample_type<double>(DOUBLE_ST);
    check_sample_type<int64_t>(INT64_ST);

    short* hptr = 0;
    BOOST_CHECK_EQUAL(getSampleType(hptr), SHORT_ST);
}


BOOST_AUTO_TEST_CASE(test_sample_alloc)
{
    SampleT<int32_t> isamp;
    unsigned int allocLen = isamp.getAllocLength();
    BOOST_CHECK(allocLen == 0);

    BOOST_CHECK_THROW(isamp.setDataLength(1), SampleLengthException);

    isamp.allocateData(10);
    BOOST_CHECK_EQUAL(isamp.getAllocLength(), 10);
    BOOST_CHECK_EQUAL(isamp.getAllocByteLength(), 10 * sizeof(int32_t));
    BOOST_CHECK_EQUAL(isamp.getDataLength(), 0);
    isamp.setDataLength(5);
    BOOST_CHECK_EQUAL(isamp.getDataLength(), 5);
    isamp.setDataLength(10);
    BOOST_CHECK_EQUAL(isamp.getDataLength(), 10);

    unsigned int val = 11;
    try {
        isamp.setDataLength(val);
        BOOST_FAIL("Expected SampleLengthException not thrown");
    }
    catch (const SampleLengthException& e) {
        SampleLengthException xe{"SampleT::setDataLength:", val,
                                 isamp.getAllocLength()};
        BOOST_CHECK_EQUAL(xe.toString(), e.toString());
    }

    // test that we cannot allocate data that would overflow the number
    // of bytes representable in unsigned int.
    auto maxval = std::numeric_limits<unsigned int>::max() / sizeof(int32_t);
    val = maxval + 1;
    try {
        isamp.allocateData(val);
        BOOST_FAIL("Expected SampleLengthException not thrown");
    }
    catch (const SampleLengthException& e) {
        SampleLengthException xe{"SampleT::allocateData:", val, maxval};
        BOOST_CHECK_EQUAL(xe.toString(), e.toString());
    }
}


BOOST_AUTO_TEST_CASE(test_sample_header)
{
    // this should never change, so this test just verifies that.
    BOOST_CHECK_EQUAL(SampleHeader::getSizeOf(),
                       sizeof(dsm_time_t) + sizeof(dsm_sample_length_t) +
                       sizeof(dsm_sample_id_t));
    BOOST_CHECK_EQUAL(sizeof(SampleHeader), 16);
}
