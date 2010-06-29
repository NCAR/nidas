/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#ifndef NIDAS_CORE_ASCIISSCANF_H
#define NIDAS_CORE_ASCIISSCANF_H

/*
 * flex reads AsciiSscanf.ll and creates AsciiSscanf.cc.
 * In AsciiSscanf.cc FLEX_SCANNER is defined, and it also defines
 * yyFlexlexer as below, and includes <FlexLexer.h>.  We
 * put the #ifndef FLEX_SCANNER here to prevent AsciiSscanf.cc
 * from including FlexLexer.h twice which causes problems 
 * at least in flex versions 2.5.4 and 2.5.31.
 */

#ifndef FLEX_SCANNER
#undef yyFlexLexer
#define yyFlexLexer SscanfFlexLexer
#include <FlexLexer.h>		// Header that comes with flex. In /usr/include.
#endif

#include <nidas/core/Sample.h>
#include <nidas/util/ParseException.h>

#include <vector>

namespace nidas { namespace core {

class SampleTag;

/**
 * Class providing sscanf functionality for parsing ASCII data.
 * One sets the sscanf format with setFormat then calls
 * sscanf method.
 * This class does a lexical parse of the sscanf conversion
 * format string to determine the number and types of the
 * conversions.
 */
class AsciiSscanf: public SscanfFlexLexer {
public:

    AsciiSscanf();
    virtual ~AsciiSscanf();

    enum fieldtype { DOUBLE=258, FLOAT, INT, SHORT, CHAR, UINT, USHORT, LONG, ULONG, UNKNOWN };

    /**
     * Set the format to be used to sscanf samples.
     * Throws ParseException if there is a conversion field
     * that it doesn't recognize.  Supports the basic
     * conversions:  %f, %g, %d, %o, %x, %u, %c.
     * The 'l' flag can be used with %f and %g for converting
     * to a double: %lf.  The 'h' flag can be used with the
     * integer conversions for converting to a short int: %hd.
     * Each can also include a maximum field width.
     * Each can also have the suppression flag ('*').
     * Example:   "%*2c%f%f%*5c%x" skips two characters,
     * converts 2 float fields, skips five characters, converts
     * a hex value.
     */
    void setFormat(const std::string& val) throw(nidas::util::ParseException);

    const std::string& getFormat() const { return format; }

    void setSampleTag(const SampleTag* val) { sampleTag = val; }
    const SampleTag* getSampleTag() const { return sampleTag; }

    /**
     * scan input, storing up to nout number of values into
     * output, as  floats.  If a conversion field is not for
     * a float, like "%d", then the field is scanned into
     * a the appropriate type (integer in this example)
     * and then cast to a float.  So a scanf of "99" with a "%d"
     * will result in a float, with value 99.0.
     */
    int sscanf(const char* input, float* output, int nout) throw();

    /**
     * SscanfFlexLexer virtual function that we override in order
     * to provide input to the lexical scanner.
     */
    int LexerInput( char* buf, int max_size );

    int getNumberOfFields() const { return fields.size(); }

    /**
     * Maximum number of fields that we can scan.
     */
    const int MAX_OUTPUT_VALUES;

    /**
     * Information we determine from each conversion field
     * in the sscanf format.
     */
    struct FormatField {
	enum fieldtype type;
	int size;
	int length;
    };

protected:

    /**
     * Function created for us by flex. Does the lexical scanning
     * of the format string.
     */
    int yylex();

private:

    /**
     * scanf format that we scan to count the number and type of
     * % converters. This format is then used to do sscanf's on
     * ASCII data coming from a sensor.
     */
    std::string format;

    /**
     * Same scanf format, converted to character for quick use
     * by sscanf.
     */
    char* charfmt;

    /**
     * Current lexical scanner position.
     */
    int lexpos;

    /**
     * Pointer to current FormatField that we are scanning.
     */
    struct FormatField* currentField;

    /**
     * Information scanned from each field.
     */
    std::vector<FormatField*> fields;

    /**
     * Are all fields floats?
     */
    bool allFloats;

    /**
     * A local buffer to store results of sscanf.
     */
    char* databuf0;

    /**
     * Pointers into the local buffer for each field in the
     * format string.  The pointers are aligned appropriately
     * for the type of the format conversion. The sscanf looks
     * like so:
     *	sscanf(inputstr,format,bufptrs[0],bufptrs[1],bufptrs[2],...)
     */
    char** bufptrs;

    /**
     * A scanner may produce dsm samples. sampleTag points to
     * to SampleTag describing the samples produced.
     */
    const SampleTag* sampleTag;

};

}}	// namespace nidas namespace core

#endif
