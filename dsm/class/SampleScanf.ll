/*
 ********************************************************************
    Copyright by the National Center for Atmospheric Research

    $LastChangedDate: 2004-10-15 17:53:32 -0600 (Fri, 15 Oct 2004) $

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL: http://orion/svn/hiaper/ads3/dsm/class/RTL_DSMSensor.h $

    flex++ input file.  The lexical scanner will make sense
    of a scanf conversion format, like "%*2c %f %d %x",
    determining the number of conversions, and the type
    of each one.
 ********************************************************************

*/

/* definitions section */

%option c++

/* causes flex to create a subclass of FlexLexer called ScanfFlexLexer.
 * Then we can have more than one implementation of FlexLexer in a library.
 */
%option prefix="Scanf"

/* tells flex that we further subclass ScanfFlexLexer with SampleScanf */
%option yyclass="SampleScanf"

%option never-interactive
%option noyywrap

%{

#include <SampleScanf.h>
#include <Sample.h>
#include <assert.h>

using namespace dsm;

/* forward declarations */

%}

DIGIT	[0-9]

%%

%{
/* rules section */
%}
	currentField->length = 1;

\%\%		;	/* two parenthesis */

\%{DIGIT}*lf	{ return(DOUBLE); }
\%\*{DIGIT}*lf	;	/* %*lf means skip field */

\%{DIGIT}*lg	{ return(DOUBLE); }
\%\*{DIGIT}*lg	;	/* %*lg means skip field */

\%{DIGIT}*f	{ return(FLOAT); }
\%\*{DIGIT}*f	;	/* %*f means skip field */

\%{DIGIT}*g	{ return(FLOAT); }
\%\*{DIGIT}*g	;	/* %*g means skip field */

\%{DIGIT}*d	{ return(LONG); }
\%{DIGIT}*o	{ return(LONG); }
\%{DIGIT}*x	{ return(LONG); }
\%{DIGIT}*ld	{ return(LONG); }
\%{DIGIT}*lo	{ return(LONG); }
\%{DIGIT}*lx	{ return(LONG); }
\%{DIGIT}*u	{ return(ULONG); }
\%{DIGIT}*lu	{ return(ULONG); }
\%\*{DIGIT}*d	;	/* %*d means skip field */
\%\*{DIGIT}*o	;
\%\*{DIGIT}*x	;
\%\*{DIGIT}*u	;
\%\*{DIGIT}*ld	;
\%\*{DIGIT}*lo	;
\%\*{DIGIT}*lx	;
\%\*{DIGIT}*lu	;

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

[ \t]+		;

.		;

%%

/* user code section */

SampleScanf::SampleScanf(): 
	MAX_OUTPUT_VALUES(60),charfmt(0),allFloats(true),
	databuf0(0),bufptrs(new char*[MAX_OUTPUT_VALUES])
{
    for (int i = 0; i < MAX_OUTPUT_VALUES; i++)
    	bufptrs[i] = 0;
}
    
SampleScanf::~SampleScanf()
{
    delete [] charfmt;
    delete [] databuf0;
    delete [] bufptrs;
    for (int i = 0; i < (int)fields.size(); i++)
    	delete fields[i];
}

int SampleScanf::LexerInput(char* buf, int max_size)
{
    int l = format.size() - lexpos;
    if (l > max_size) l = max_size;
    if (l == 0) return l;

    format.copy(buf,l,lexpos);
    lexpos += l;

    return l;
}

void SampleScanf::setFormat(const std::string& val)
	  throw(atdUtil::ParseException)
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
	    throw atdUtil::ParseException(
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
	default:
	    throw atdUtil::ParseException(
	    	"unsupported field in scanf format",format);
	}

	size = currentField->size;
	length = currentField->length;

	/* Alignment */
	if ((align_adj = (((unsigned int)bufptr) % size))) {
	    bufptr += (size - align_adj);
	    tlen += (size - align_adj);
	}
	tlen += size * length;
	bufptr += size * length;
	bufptrs[nfields] = bufptr;
	fields.push_back(currentField);
	std::cerr << "nfields=" << nfields << " lexres=" << lexres << 
		" length=" << currentField->length << std::endl;
	if (currentField->size > maxsize) maxsize = currentField->size;
    }

    delete [] databuf0;
    bufptr = databuf0 = new char[tlen + maxsize];

    // first address aligned with largest field
    align_adj = ((unsigned int)bufptr) % maxsize;
    if (align_adj) bufptr += (maxsize - align_adj);

    for (nfields = 0; nfields < (int)fields.size(); nfields++)
	bufptrs[nfields] += (unsigned int)bufptr;

    // initialize the rest to the last pointer value.
    // It should never be dereferenced, but valgrind complains
    for ( ; nfields < MAX_OUTPUT_VALUES; nfields++)
    	bufptrs[nfields] = bufptrs[nfields-1];
}

bool SampleScanf::receive(const Sample*samp)
	throw(atdUtil::IOException,dsm::SampleParseException)
{
    assert(MAX_OUTPUT_VALUES <= 60);

    int nparsed = ::sscanf((const char*)samp->getConstVoidDataPtr(),charfmt,
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
	bufptrs[55],bufptrs[56],bufptrs[57],bufptrs[58],bufptrs[59]);

    std::cerr << "nparsed=" << nparsed << std::endl;

    if (!nparsed) {
        scanfFailures++;
	return nparsed;
    }
    else if (nparsed != (int)fields.size())
        scanfPartials++;

    FloatSample* outs = SamplePool<FloatSample>::getInstance()->getSample(nparsed);
    if (allFloats)
	memcpy(outs->getVoidDataPtr(),bufptrs[0],nparsed*sizeof(float));
    else {
	// convert to float by hand
	for (int i = 0; i < nparsed; i++) {
	    switch(fields[i]->type) {
	    case DOUBLE:
	      outs->getDataPtr()[i] = (float)*(double*)bufptrs[i];
	      break;
	    case FLOAT:
	      outs->getDataPtr()[i] = *(float*)bufptrs[i];
	      break;
	    case LONG:
	      outs->getDataPtr()[i] = (float)*(long*)bufptrs[i];
	      break;
	    case ULONG:
	      outs->getDataPtr()[i] = (float)*(unsigned long*)bufptrs[i];
	      break;
	    case SHORT:
	      outs->getDataPtr()[i] = (float)*(short*)bufptrs[i];
	      break;
	    case USHORT:
	      outs->getDataPtr()[i] = (float)*(unsigned short*)bufptrs[i];
	      break;
	    case CHAR:
	      // treats first character as unsigned int
	      outs->getDataPtr()[i] = (float)*(unsigned char*)bufptrs[i];
	      break;
	    }
	}
    }
    outs->setTimeTag(samp->getTimeTag());
    outs->setId(samp->getId());
    outs->setDataLength(nparsed);
    distribute(outs);
    return true;
}

/* #define DEBUG */

#ifdef DEBUG
int main (int argc, char *argv[])
{
  
    char* fmtstr = "%*c %f KDKDKD %ld %16c %*32c";

    SampleScanf parser;
    parser.setFormat(fmtstr);
}

#endif
