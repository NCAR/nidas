/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $
 ********************************************************************

*/

#ifndef DSM_ASCIISCANNER_H
#define DSM_ASCIISCANNER_H

/*
 * flex reads AsciiScanner.ll and creates AsciiScanner.cc.
 * In AsciiScanner.cc FLEX_SCANNER is defined, and it also defines
 * yyFlexlexer as below, and includes <FlexLexer.h>.  We
 * put the #ifndef FLEX_SCANNER here to prevent AsciiScanner.cc
 * from including FlexLexer.h twice which causes problems 
 * at least in flex versions 2.5.4 and 2.5.31.
 */

#ifndef FLEX_SCANNER
#undef yyFlexLexer
#define yyFlexLexer ScanfFlexLexer
#include <FlexLexer.h>		// Header that comes with flex. In /usr/include.
#endif

#include <atdUtil/ParseException.h>

#include <vector>

/**
 * Class providing sscanf functionality for parsing ASCII data.
 * One sets the sscanf format with setFormat then calls
 * sscanf method.
 * This class does a lexical parse of the sscanf conversion
 * format string to determine the number and types of the
 * conversions.
 */
class AsciiScanner: public ScanfFlexLexer {
public:

    AsciiScanner();
    virtual ~AsciiScanner();

    enum fieldtype { DOUBLE=258, FLOAT, LONG, SHORT, CHAR, ULONG, USHORT, UNKNOWN };

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
    void setFormat(const std::string& val) throw(atdUtil::ParseException);

    const std::string& getFormat() const { return format; }

    /**
     * SampleClient receive method.
     */
    int sscanf(const char* input, float* output, int nout) throw();

    /**
     * ScanfFlexLexer virtual function that we override in order
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

};

#endif
