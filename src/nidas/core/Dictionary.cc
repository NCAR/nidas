// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2011, Copyright University Corporation for Atmospheric Research
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


#include <nidas/core/Dictionary.h>

using namespace std;
using namespace nidas::core;

string Dictionary::expandString(const string& input) const
{
    string::size_type dollar;

    string result;
    string tmp = input;

    for (;;) {
        string::size_type lastpos = 0;
        bool substitute = false;

        while ((dollar = tmp.find('$',lastpos)) != string::npos) {

            result.append(tmp.substr(lastpos,dollar-lastpos));
            lastpos = dollar;

            string::size_type openparen = tmp.find('{',dollar);
            string::size_type tokenStart;
            int tokenLen = 0;
            int totalLen;

            if (openparen == dollar + 1) {
                string::size_type closeparen = tmp.find('}',openparen);
                if (closeparen == string::npos) break;
                tokenStart = openparen + 1;
                tokenLen = closeparen - openparen - 1;
                totalLen = closeparen - dollar + 1;
                lastpos = closeparen + 1;
            }
            else {
                string::size_type endtok = tmp.find_first_of("/.$",dollar + 1);
                if (endtok == string::npos) endtok = tmp.length();
                tokenStart = dollar + 1;
                tokenLen = endtok - dollar - 1;
                totalLen = endtok - dollar;
                lastpos = endtok;
            }
            string value;
            if (tokenLen > 0 && getTokenValue(tmp.substr(tokenStart,tokenLen),value)) {
                substitute = true;
                result.append(value);
            }
            else result.append(tmp.substr(dollar,totalLen));
        }

        result.append(tmp.substr(lastpos));
        if (!substitute) break;
        tmp = result;
        result.clear();
    }
#ifdef DEBUG
    cerr << "input: \"" << input << "\" expanded to \"" <<
    	result << "\"" << endl;
#endif
    return result;
}

