// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
    Copyright 2005 UCAR, NCAR, All Rights Reserved

    $LastChangedDate$

    $LastChangedRevision$

    $LastChangedBy$

    $HeadURL$
 ********************************************************************

*/

#include "util.h"
#include "Process.h"
#include <sstream>
#include <iomanip>

using namespace std;

string nidas::util::replaceBackslashSequences(const string& str)
{
    string::size_type bs;
    string res = str;
    for (string::size_type ic = 0; (bs = res.find('\\',ic)) != string::npos;
    	ic = bs) {
	bs++;
	if (bs == res.length()) break;
        switch(res[bs]) {
	case 'e':
	    res.erase(bs,1);
	    res[bs-1] = '\e';
	    break;
	case 'f':
	    res.erase(bs,1);
	    res[bs-1] = '\f';
	    break;
	case 'n':
	    res.erase(bs,1);
	    res[bs-1] = '\n';
	    break;
	case 'r':
	    res.erase(bs,1);
	    res[bs-1] = '\r';
	    break;
	case 't':
	    res.erase(bs,1);
	    res[bs-1] = '\t';
	    break;
	case '\\':
	    res.erase(bs,1);
	    res[bs-1] = '\\';
	    break;
	case 'x':	//  \xhh	hex
	    if (bs + 2 >= res.length()) break;
	    {
		istringstream ist(res.substr(bs+1,2));
		int hx;
		ist >> hex >> hx;
		if (!ist.fail()) {
		    res.erase(bs,3);
		    res[bs-1] = (char)(hx & 0xff);
		}
	    }
	    break;
	case '0':	//  \000   octal
	case '1':
	case '2':
	case '3':
	    if (bs + 2 >= res.length()) break;
	    {
		istringstream ist(res.substr(bs,3));
		int oc;
		ist >> oct >> oc;
		if (!ist.fail()) {
		    res.erase(bs,3);
		    res[bs-1] = (char)(oc & 0xff);
		}
	    }
	    break;
	}
    }
    return res;
}

/* static */
string nidas::util::addBackslashSequences(const string& str)
{
    string res;
    for (unsigned int ic = 0; ic < str.length(); ic++) {
	char c = str[ic];
        switch(c) {
	case '\e':
	    res.append("\\e");
	    break;
	case '\f':
	    res.append("\\f");
	    break;
	case '\n':
	    res.append("\\n");
	    break;
	case '\r':
	    res.append("\\r");
	    break;
	case '\t':
	    res.append("\\t");
	    break;
	case '\\':
	    res.append("\\\\");
	    break;
	default:
	    // in C locale isprint returns true for
	    // alpha-numeric, punctuation and the space character
	    // but not other white-space characters like tabs,
	    // newlines, carriage-returns,  form-feeds, etc.
	    if (::isprint(c)) res.push_back(c);
	    else {
		ostringstream ost;
		ost << "\\x" << hex << setw(2) << setfill('0') << (int)(unsigned char) c;
		res.append(ost.str());
	    }
	        
	    break;
	}
    }
    return res;
}

void nidas::util::trimString(std::string& str)
{
    for (string::iterator si = str.end(); si != str.begin(); ) {
        --si;
        if (!isspace(*si)) break;
        si = str.erase(si);
    }
}

void nidas::util::replaceCharsIn(string& in,const string& pat, const string& rep)
{
    string::size_type patpos;
    while ((patpos = in.find(pat,0)) != string::npos)
        in.replace(patpos,pat.length(),rep);
}

string nidas::util::replaceChars(const string& in,const string& pat, const string& rep)
{
    string res = in;
    replaceCharsIn(res,pat,rep);
    return res;
}

string nidas::util::svnversion(const string& path) throw (nidas::util::IOException)
{

    const string& cmd = "svnversion";

    vector<string> args;
    args.push_back(cmd);

    // for a path like "/a/b/c/d", do "subversion /a/b/c d"
    string::size_type slash = path.rfind('/');
    if (slash != string::npos) {
        args.push_back(path.substr(0,slash));
        args.push_back(path.substr(slash+1));
    }
    else args.push_back(path);

    nidas::util::Process proc = Process::spawn(cmd,args);

    istream& outst = proc.outStream();
    string strout;

    for (; !outst.eof();) {
        char cbuf[32];
        outst.read(cbuf,sizeof(cbuf)-1);
        cbuf[outst.gcount()] = 0;
        strout += cbuf;
    }

    istream& errst = proc.errStream();
    string strerr;
    for (; !errst.eof();) {
        char cbuf[32];
        errst.read(cbuf,sizeof(cbuf)-1);
        cbuf[errst.gcount()] = 0;
        strerr += cbuf;
    }

    int status;
    proc.wait(true,&status);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        throw IOException(cmd,strerr);
    }

    return strout;
}



