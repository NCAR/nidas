#ifndef NIDAS_CORE_SAMPLE_TRAITS
#define NIDAS_CORE_SAMPLE_TRAITS

namespace nidas { namespace core {

/**
 * maxValue is an overloaded function returning the
 * maximum value of its integer argument.
 */
inline unsigned int maxValue(unsigned short)
{
    return USHRT_MAX;
}

inline unsigned int maxValue(short)
{
    return SHRT_MAX;
}

inline unsigned int maxValue(int)
{
    return INT_MAX;
}

inline unsigned int maxValue(unsigned int)
{
    return UINT_MAX;
}

typedef enum sampleType {
	CHAR_ST, UCHAR_ST, SHORT_ST, USHORT_ST,
	INT32_ST, UINT32_ST, FLOAT_ST, DOUBLE_ST,
	INT64_ST, UNKNOWN_ST } sampleType;



template <typename T>
struct sample_type_traits_base
{
    typedef T sample_type;
    typedef T* sample_type_pointer;
};

template <typename T>
struct sample_type_traits : public sample_type_traits_base<T>
{
};

template <>
struct sample_type_traits<char>
{
    static const sampleType sample_type_enum = CHAR_ST;
};

template <>
struct sample_type_traits<unsigned char>
{
    static const sampleType sample_type_enum = UCHAR_ST;
};

template <>
struct sample_type_traits<unsigned short>
{
    static const sampleType sample_type_enum = USHORT_ST;
};

template <>
struct sample_type_traits<short>
{
    static const sampleType sample_type_enum = SHORT_ST;
};

template <>
struct sample_type_traits<unsigned int>
{
    static const sampleType sample_type_enum = UINT32_ST;
};

template <>
struct sample_type_traits<int>
{
    static const sampleType sample_type_enum = INT32_ST;
};

template <>
struct sample_type_traits<float>
{
    static const sampleType sample_type_enum = FLOAT_ST;
};

template <>
struct sample_type_traits<double>
{
    static const sampleType sample_type_enum = DOUBLE_ST;
};

template <>
struct sample_type_traits<long long>
{
    static const sampleType sample_type_enum = INT64_ST;
};

template <>
struct sample_type_traits<void>
{
    static const sampleType sample_type_enum = UNKNOWN_ST;
};


template <typename T>
inline sampleType getSampleType(T*)
{
    return sample_type_traits<T>::sample_type_enum;
}


}} //namespace nidas::core

#endif // NIDAS_CORE_SAMPLE_TRAITS
