/* -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*- */
/* vim: set shiftwidth=4 softtabstop=4 expandtab: */
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
	_currentField->length = 1;

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
		  _currentField->length = atoi((char *)(yytext+1));
		  return(CHAR);
		}
\%c		{
		  _currentField->length = 1;
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
	MAX_OUTPUT_VALUES(120),_format(),_charfmt(0),
        _lexpos(0),_currentField(0),_fields(),_allFloats(true),
        _databuf0(0),_bufptrs(new char*[MAX_OUTPUT_VALUES]),
	_sampleTag(0)
{
    for (int i = 0; i < MAX_OUTPUT_VALUES; i++)
    	_bufptrs[i] = 0;
}
    
AsciiSscanf::~AsciiSscanf()
{
    delete [] _charfmt;
    delete [] _databuf0;
    delete [] _bufptrs;
    for (int i = 0; i < (int)_fields.size(); i++)
    	delete _fields[i];
}

int AsciiSscanf::LexerInput(char* buf, int max_size)
{
    int l = _format.size() - _lexpos;
    if (l > max_size) l = max_size;
    if (l == 0) return l;

    _format.copy(buf,l,_lexpos);
    _lexpos += l;

    return l;
}

void AsciiSscanf::setFormat(const std::string& val)
	  throw(nidas::util::ParseException)
{

    _format = val;

    delete [] _charfmt;
    _charfmt = new char[val.size()+1];
    strcpy(_charfmt,val.c_str());

    _lexpos = 0;

    int lexres;
    int size,length;
    int align_adj;
    int nfields;
    int tlen = 0;
    int maxsize = 0;

    char* bufptr = 0;

    _allFloats = true;
    for (nfields = 0; ; nfields++) {

        _currentField = new FormatField();

        lexres = yylex();
	if (!lexres) {
	    delete _currentField;
	    break;
	}

	if (nfields == MAX_OUTPUT_VALUES) 
	    throw nidas::util::ParseException(
	    	"too many fields in scanf format string",_format);

	_currentField->type = (enum fieldtype) lexres;
	length = _currentField->length;

	switch(_currentField->type) {
	case DOUBLE:
	  _currentField->size = sizeof(double);
	  _allFloats = false;
	  break;
	case FLOAT:
	  _currentField->size = sizeof(float);
	  break;
	case INT:
	case UINT:
	  _currentField->size = sizeof(int);
	  _allFloats = false;
	  break;
	case LONG:
	case ULONG:
	  _currentField->size = sizeof(long);
	  _allFloats = false;
	  break;
	case SHORT:
	case USHORT:
	  _currentField->size = sizeof(short);
	  _allFloats = false;
	  break;
	case CHAR:
	  _currentField->size = sizeof(char);
	  _allFloats = false;
	  break;
	case UNKNOWN:
	default:
	    throw nidas::util::ParseException(
	    	"unsupported field in scanf format",_format);
	}

	size = _currentField->size;

	/* Alignment */
	if ((align_adj = (((unsigned long)bufptr) % size))) {
	    bufptr += (size - align_adj);
	    tlen += (size - align_adj);
	}
	_bufptrs[nfields] = bufptr;

	tlen += size * length;
	bufptr += size * length;

	_fields.push_back(_currentField);
#ifdef DEBUG
	std::cerr << "nfields=" << nfields << " lexres=" << lexres << 
		" length=" << _currentField->length << std::endl;
#endif
	if (_currentField->size > maxsize) maxsize = _currentField->size;
    }

    delete [] _databuf0;
    bufptr = _databuf0 = new char[tlen + maxsize];

    // first address aligned with largest field
    align_adj = ((unsigned long)bufptr) % maxsize;
    if (align_adj) bufptr += (maxsize - align_adj);

    for (nfields = 0; nfields < (int)_fields.size(); nfields++)
	_bufptrs[nfields] += (unsigned long)bufptr;

    // initialize the rest to the last pointer value.
    // It should never be dereferenced, but valgrind complains
    for ( ; nfields < MAX_OUTPUT_VALUES; nfields++)
    	_bufptrs[nfields] = _bufptrs[nfields-1];
}

int AsciiSscanf::sscanf(const char* input, float* output, int nout) throw()
{

    // If there are more than MAX_OUTPUT_VALUES number of
    // % descriptors in _charfmt, then the ::sscanf will seg fault.
    // The user should have been warned of the situation
    // earlier with a ParseException in setFormat(),
    // when (nfields == MAX_OUTPUT_VALUES).
    // So one should be able to safely disable these
    // asserts with #define NDEBUG.
    assert(nout <= MAX_OUTPUT_VALUES);

    /*
     * The following sscanf parses up to 70 values.  If one wants
     * to increase MAX_OUTPUT_VALUES, then one must add more
     * _bufptrs[XX] here to the sscanf.
     */
    assert(MAX_OUTPUT_VALUES <= 120);

    int nparsed = ::sscanf(input,_charfmt,
	_bufptrs[ 0],_bufptrs[ 1],_bufptrs[ 2],_bufptrs[ 3],_bufptrs[ 4],
	_bufptrs[ 5],_bufptrs[ 6],_bufptrs[ 7],_bufptrs[ 8],_bufptrs[ 9],
	_bufptrs[10],_bufptrs[11],_bufptrs[12],_bufptrs[13],_bufptrs[14],
	_bufptrs[15],_bufptrs[16],_bufptrs[17],_bufptrs[18],_bufptrs[19],
	_bufptrs[20],_bufptrs[21],_bufptrs[22],_bufptrs[23],_bufptrs[24],
	_bufptrs[25],_bufptrs[26],_bufptrs[27],_bufptrs[28],_bufptrs[29],
	_bufptrs[30],_bufptrs[31],_bufptrs[32],_bufptrs[33],_bufptrs[34],
	_bufptrs[35],_bufptrs[36],_bufptrs[37],_bufptrs[38],_bufptrs[39],
	_bufptrs[40],_bufptrs[41],_bufptrs[42],_bufptrs[43],_bufptrs[44],
	_bufptrs[45],_bufptrs[46],_bufptrs[47],_bufptrs[48],_bufptrs[49],
	_bufptrs[50],_bufptrs[51],_bufptrs[52],_bufptrs[53],_bufptrs[54],
	_bufptrs[55],_bufptrs[56],_bufptrs[57],_bufptrs[58],_bufptrs[59],
        _bufptrs[60],_bufptrs[61],_bufptrs[62],_bufptrs[63],_bufptrs[64],
        _bufptrs[65],_bufptrs[66],_bufptrs[67],_bufptrs[68],_bufptrs[69],
        _bufptrs[70],_bufptrs[71],_bufptrs[72],_bufptrs[73],_bufptrs[74],
        _bufptrs[75],_bufptrs[76],_bufptrs[77],_bufptrs[78],_bufptrs[79],
        _bufptrs[80],_bufptrs[81],_bufptrs[82],_bufptrs[83],_bufptrs[84],
        _bufptrs[85],_bufptrs[86],_bufptrs[87],_bufptrs[88],_bufptrs[89],
        _bufptrs[90],_bufptrs[91],_bufptrs[92],_bufptrs[93],_bufptrs[94],
        _bufptrs[95],_bufptrs[96],_bufptrs[97],_bufptrs[98],_bufptrs[99],
        _bufptrs[100],_bufptrs[101],_bufptrs[102],_bufptrs[103],_bufptrs[104],
        _bufptrs[105],_bufptrs[106],_bufptrs[107],_bufptrs[108],_bufptrs[109],
        _bufptrs[110],_bufptrs[111],_bufptrs[112],_bufptrs[113],_bufptrs[114],
        _bufptrs[115],_bufptrs[116],_bufptrs[117],_bufptrs[118],_bufptrs[119]);

    /*
    std::cerr << "nparsed=" << nparsed << " fmt=" << charfmt <<
    	" data=" << input << std::endl;
    */

    // sscanf returns EOF (-1) if end-of-string reached before parsing anything.
    if (nparsed <= 0) return 0;

    if (nparsed < nout) nout = nparsed;

    if (_allFloats)
	memcpy(output,_bufptrs[0],nout*sizeof(float));
    else {
	// convert to float by hand
	for (int i = 0; i < nout; i++) {
	    switch(_fields[i]->type) {
	    case DOUBLE:
		output[i] = (float)*(double*)_bufptrs[i];
		break;
	    case FLOAT:
		output[i] = *(float*)_bufptrs[i];
		break;
	    case INT:
		output[i] = (float)*(int*)_bufptrs[i];
		break;
	    case UINT:
		output[i] = (float)*(unsigned int*)_bufptrs[i];
		break;
	    case LONG:
		output[i] = (float)*(long*)_bufptrs[i];
		break;
	    case ULONG:
		output[i] = (float)*(unsigned long*)_bufptrs[i];
		break;
	    case SHORT:
		output[i] = (float)*(short*)_bufptrs[i];
		break;
	    case USHORT:
		output[i] = (float)*(unsigned short*)_bufptrs[i];
		break;
	    case CHAR:
		// treats first character as unsigned int
		if (_fields[i]->length == 1)
		    output[i] = (float)*(unsigned char*)_bufptrs[i];
		else {
		// convert binary value of first two characters to float,
		// in little-endian style (first char is least-significant)
		    unsigned char* cp = (unsigned char*)_bufptrs[i];
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
