// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2008, Copyright University Corporation for Atmospheric Research
 **
 ** This program is free software; you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation; either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** This program is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** The LICENSE.txt file accompanying this software contains
 ** a copy of the GNU General Public License. If it is not found,
 ** write to the Free Software Foundation, Inc.,
 ** 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 **
 ********************************************************************
*/

#include "util.h"
#include "Process.h"
#include "Logger.h"

#include <sstream>
#include <iomanip>

using namespace std;

std::string nidas::util::replaceBackslashSequences(const std::string& str)
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
std::string nidas::util::addBackslashSequences(const std::string& str)
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

void nidas::util::replaceCharsIn(std::string& in,const std::string& pat, const std::string& rep)
{
    string::size_type patpos;
    while ((patpos = in.find(pat,0)) != string::npos)
        in.replace(patpos,pat.length(),rep);
}

std::string nidas::util::replaceChars(const std::string& in,const std::string& pat, const std::string& rep)
{
    string res = in;
    replaceCharsIn(res,pat,rep);
    return res;
}

std::string nidas::util::svnStatus(const std::string& path)
    throw (nidas::util::IOException)
{

    const string& cmd = "svn";

    vector<string> args;

    args.push_back(cmd);
    args.push_back("status");
    args.push_back("-v");
    // "--depth empty" means list status of only path itself.
    args.push_back("--depth");
    args.push_back("empty");
    args.push_back(path);

    nidas::util::Process proc = Process::spawn(cmd,args);

    istream& outst = proc.outStream();
    string strout;

    // first 8 characters: http://svnbook.red-bean.com/en/1.7/svn.ref.svn.c.status.html
    char flags[9];
    outst.read(flags,sizeof(flags)-1);
    flags[outst.gcount()] = 0;

    if (!outst.eof() && flags[0] != '?') {
        string rev;
        outst >> rev;
        if (!outst.fail()) 
            strout = rev + flags;
        trimString(strout);
    }

    istream& errst = proc.errStream();
    string strerr;
    for (; strerr.length() < 1024 && !errst.eof(); ) {
        char cbuf[32];
        errst.read(cbuf,sizeof(cbuf)-1);
        cbuf[errst.gcount()] = 0;
        strerr += cbuf;
    }

    int status;
    proc.wait(true,&status);
    if (!WIFEXITED(status) || WEXITSTATUS(status)) {
        throw IOException("svn status -v --depth empty",strerr);
    }
    return strout;
}

bool nidas::util::isNonPrintable(const char c, bool allowSTXETX)
{
    bool retval = false;
    const int STX = 2;
    const int ETX = 3;
    const int TAB = 9;
    const int NL = 10;
    const int CR = 13;

    // first check if c is not one of the standard printable characters.
    if (!RANGE_CHECK_INC(' ', c, '~')
            && c != TAB && c != CR && c != NL) {
        // if STX/ETX is allowed, then check to make sure it isn't one of those
        if (allowSTXETX ) {
            if (c != STX && c != ETX) {
                retval = true;
                DLOG(("isNonPrintable(): non printable character 0x%02x", c));
            }
        }
        else {
            retval = true;
            DLOG(("isNonPrintable(): non printable character 0x%02x", c));
        }
    }

    return retval;
}

bool nidas::util::containsNonPrintable(char const * buf, std::size_t len, bool allowSTXETX)
{
    bool retval = false;

    for (std::size_t i=0; i<len; ++i) {
        if (isNonPrintable(buf[i], allowSTXETX)) {
            retval = true;
            DLOG(("containsNonPrintable(): non-printable found at: %i", i));
            break;
        }
    }

    return retval;
}
