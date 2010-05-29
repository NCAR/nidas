/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$

    flex++ input file.  The lexical scanner will make sense
    of a scanf conversion format, like "%*2c %f %d %x",
    determining the number of conversions, and the type
    of each one.
 ********************************************************************

*/

/* definitions section */

%option c++

/* causes flex to create a subclass of FlexLexer called SscanfFlexLexer.
 * Then we can have more than one implementation of FlexLexer in a library.
 */
%option prefix="Sscanf"

/* tells flex that we further subclass SscanfFlexLexer with AsciiSscanf */
%option yyclass="AsciiSscanf"

%option never-interactive
%option noyywrap

%{

#include <nidas/core/AsciiSscanf.h>
#include <nidas/core/Sample.h>

#include <iostream>

#define NDEBUG      // define NDEBUG to turn off the asserts
#include <cassert>
#include <cmath>

using namespace nidas::core;

/* forward declarations */

%}

DIGIT	[0-9]
LETTER	[a-zA-Z]

%%

%{
/* rules section */
%}
	currentField->length = 1;

\%\%		;	/* two percent signs */

\%{DIGIT}*lf	{ return(DOUBLE); }
\%\*{DIGIT}*lf	;	/* %*lf means skip field */

\%{DIGIT}*lg	{ return(DOUBLE); }
\%\*{DIGIT}*lg	;	/* %*lg means skip field */

\%{DIGIT}*f	{ return(FLOAT); }
\%\*{DIGIT}*f	;	/* %*f means skip field */

\%{DIGIT}*g	{ return(FLOAT); }
\%\*{DIGIT}*g	;	/* %*g means skip field */

\%{DIGIT}*d	{ return(INT); }
\%{DIGIT}*o	{ return(INT); }
\%{DIGIT}*x	{ return(INT); }
\%{DIGIT}*ld	{ return(LONG); }
\%{DIGIT}*lo	{ return(LONG); }
\%{DIGIT}*lx	{ return(LONG); }
\%{DIGIT}*u	{ return(UINT); }
\%{DIGIT}*lu	{ return(ULONG); }
\%\*{DIGIT}*d	;	/* %*d means skip field */
\%\*{DIGIT}*o	;
\%\*{DIGIT}*x	;
\%\*{DIGIT}*u	;
\%\*{DIGIT}*ld	;
\%\*{DIGIT}*lo	;
\%\*{DIGIT}*lx	;
\%\*{DIGIT}*lu	;
\%\*\[[^]]+\]	;       /* %*[xxx] or %*[^xxx] for skipping fields */

\%{DIGIT}*hd	{ return(SHORT); }
\%{DIGIT}*ho	{ return(SHORT); }
\%{DIGIT}*hx	{ return(SHORT); }
\%{DIGIT}*hu	{ return(USHORT); }

\%\*{DIGIT}*hd	;	/* %*hd means skip field */
\%\*{DIGIT}*ho	;
\%\*{DIGIT}*hx	;
\%\*{DIGIT}*hu	;

\%{DIGIT}+c	{
		  currentField->length = atoi((char *)(yytext+1));
		  return(CHAR);
		}
\%c		{
		  currentField->length = 1;
		  return(CHAR);
		}

\%\*{DIGIT}*c	;

\%{LETTER}	{
		  return(UNKNOWN);
		}
[ \t]+		;

.		;

%%

/* user code section */

AsciiSscanf::AsciiSscanf(): 
	MAX_OUTPUT_VALUES(70),charfmt(0),allFloats(true),
	databuf0(0),bufptrs(new char*[MAX_OUTPUT_VALUES]),
	sampleTag(0)
{
    for (int i = 0; i < MAX_OUTPUT_VALUES; i++)
    	bufptrs[i] = 0;
}
    
AsciiSscanf::~AsciiSscanf()
{
    delete [] charfmt;
    delete [] databuf0;
    delete [] bufptrs;
    for (int i = 0; i < (int)fields.size(); i++)
    	delete fields[i];
}

int AsciiSscanf::LexerInput(char* buf, int max_size)
{
    int l = format.size() - lexpos;
    if (l > max_size) l = max_size;
    if (l == 0) return l;

    format.copy(buf,l,lexpos);
    lexpos += l;

    return l;
}

void AsciiSscanf::setFormat(const std::string& val)
	  throw(nidas::util::ParseException)
{

    format = val;

    delete [] charfmt;
    charfmt = new char[val.size()+1];
    strcpy(charfmt,val.c_str());

    lexpos = 0;

    int lexres;
    int size,length;
    int align_adj;
    int nfields;
    int tlen = 0;
    int maxsize = 0;

    char* bufptr = 0;

    allFloats = true;
    for (nfields = 0; ; nfields++) {

        currentField = new FormatField();

        lexres = yylex();
	if (!lexres) {
	    delete currentField;
	    break;
	}

	if (nfields == MAX_OUTPUT_VALUES) 
	    throw nidas::util::ParseException(
	    	"too many fields in scanf format string",format);

	currentField->type = (enum fieldtype) lexres;
	length = currentField->length;

	switch(currentField->type) {
	case DOUBLE:
	  currentField->size = sizeof(double);
	  allFloats = false;
	  break;
	case FLOAT:
	  currentField->size = sizeof(float);
	  break;
	case INT:
	case UINT:
	  currentField->size = sizeof(int);
	  allFloats = false;
	  break;
	case LONG:
	case ULONG:
	  currentField->size = sizeof(long);
	  allFloats = false;
	  break;
	case SHORT:
	case USHORT:
	  currentField->size = sizeof(short);
	  allFloats = false;
	  break;
	case CHAR:
	  currentField->size = sizeof(char);
	  allFloats = false;
	  break;
	case UNKNOWN:
	default:
	    throw nidas::util::ParseException(
	    	"unsupported field in scanf format",format);
	}

	size = currentField->size;

	/* Alignment */
	if ((align_adj = (((unsigned long)bufptr) % size))) {
	    bufptr += (size - align_adj);
	    tlen += (size - align_adj);
	}
	bufptrs[nfields] = bufptr;

	tlen += size * length;
	bufptr += size * length;

	fields.push_back(currentField);
#ifdef DEBUG
	std::cerr << "nfields=" << nfields << " lexres=" << lexres << 
		" length=" << currentField->length << std::endl;
#endif
	if (currentField->size > maxsize) maxsize = currentField->size;
    }

    delete [] databuf0;
    bufptr = databuf0 = new char[tlen + maxsize];

    // first address aligned with largest field
    align_adj = ((unsigned long)bufptr) % maxsize;
    if (align_adj) bufptr += (maxsize - align_adj);

    for (nfields = 0; nfields < (int)fields.size(); nfields++)
	bufptrs[nfields] += (unsigned long)bufptr;

    // initialize the rest to the last pointer value.
    // It should never be dereferenced, but valgrind complains
    for ( ; nfields < MAX_OUTPUT_VALUES; nfields++)
    	bufptrs[nfields] = bufptrs[nfields-1];
}

int AsciiSscanf::sscanf(const char* input, float* output, int nout) throw()
{

    // If there are more than MAX_OUTPUT_VALUES number of
    // % descriptors in charfmt, then the ::sscanf will seg fault.
    // The user should have been warned of the situation
    // earlier with a ParseException in setFormat(),
    // when (nfields == MAX_OUTPUT_VALUES).
    // So one should be able to safely disable these
    // asserts with #define NDEBUG.
    assert(nout <= MAX_OUTPUT_VALUES);

    /*
     * The following sscanf parses up to 70 values.  If one wants
     * to increase MAX_OUTPUT_VALUES, then one must add more
     * bufptrs[XX] here to the sscanf.
     */
    assert(MAX_OUTPUT_VALUES <= 70);

    int nparsed = ::sscanf(input,charfmt,
	bufptrs[ 0],bufptrs[ 1],bufptrs[ 2],bufptrs[ 3],bufptrs[ 4],
	bufptrs[ 5],bufptrs[ 6],bufptrs[ 7],bufptrs[ 8],bufptrs[ 9],
	bufptrs[10],bufptrs[11],bufptrs[12],bufptrs[13],bufptrs[14],
	bufptrs[15],bufptrs[16],bufptrs[17],bufptrs[18],bufptrs[19],
	bufptrs[20],bufptrs[21],bufptrs[22],bufptrs[23],bufptrs[24],
	bufptrs[25],bufptrs[26],bufptrs[27],bufptrs[28],bufptrs[29],
	bufptrs[30],bufptrs[31],bufptrs[32],bufptrs[33],bufptrs[34],
	bufptrs[35],bufptrs[36],bufptrs[37],bufptrs[38],bufptrs[39],
	bufptrs[40],bufptrs[41],bufptrs[42],bufptrs[43],bufptrs[44],
	bufptrs[45],bufptrs[46],bufptrs[47],bufptrs[48],bufptrs[49],
	bufptrs[50],bufptrs[51],bufptrs[52],bufptrs[53],bufptrs[54],
	bufptrs[55],bufptrs[56],bufptrs[57],bufptrs[58],bufptrs[59],
        bufptrs[60],bufptrs[61],bufptrs[62],bufptrs[63],bufptrs[64],
        bufptrs[65],bufptrs[66],bufptrs[67],bufptrs[68],bufptrs[69]);

    /*
    std::cerr << "nparsed=" << nparsed << " fmt=" << charfmt <<
    	" data=" << input << std::endl;
    */

    // sscanf returns EOF (-1) if end-of-string reached before parsing anything.
    if (nparsed <= 0) return 0;

    if (nparsed < nout) nout = nparsed;

    if (allFloats)
	memcpy(output,bufptrs[0],nout*sizeof(float));
    else {
	// convert to float by hand
	for (int i = 0; i < nout; i++) {
	    switch(fields[i]->type) {
	    case DOUBLE:
		output[i] = (float)*(double*)bufptrs[i];
		break;
	    case FLOAT:
		output[i] = *(float*)bufptrs[i];
		break;
	    case INT:
		output[i] = (float)*(int*)bufptrs[i];
		break;
	    case UINT:
		output[i] = (float)*(unsigned int*)bufptrs[i];
		break;
	    case LONG:
		output[i] = (float)*(long*)bufptrs[i];
		break;
	    case ULONG:
		output[i] = (float)*(unsigned long*)bufptrs[i];
		break;
	    case SHORT:
		output[i] = (float)*(short*)bufptrs[i];
		break;
	    case USHORT:
		output[i] = (float)*(unsigned short*)bufptrs[i];
		break;
	    case CHAR:
		// treats first character as unsigned int
		if (fields[i]->length == 1)
		    output[i] = (float)*(unsigned char*)bufptrs[i];
		else {
		// convert binary value of first two characters to float,
		// in little-endian style (first char is least-significant)
		    unsigned char* cp = (unsigned char*)bufptrs[i];
		    output[i] = (float)((int)*cp + (((int)*(cp+1)) << 8));
		}
		break;
	    case UNKNOWN:
		// should have been thrown as an exception during parsing
		// but to avoid a compiler error we'll include it in this
		// switch.
		output[i] = floatNAN;
		break;
	    }
	}
    }
    return nout;
}

/* #define DEBUG */

#ifdef DEBUG
int main (int argc, char *argv[])
{
    char* fmtstr = "%*c %f KDKDKD %ld %16c %*32c";

    AsciiSscanf parser;
    parser.setFormat(fmtstr);
}

#endif
