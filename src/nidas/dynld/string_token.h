// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4; -*-
// vim: set shiftwidth=4 softtabstop=4 expandtab:
/*
 ********************************************************************
 ** NIDAS: NCAR In-situ Data Acquistion Software
 **
 ** 2010, Copyright University Corporation for Atmospheric Research
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

#include <string>
#include <vector>

std::string &
trim (std::string &s)
{
    std::string::size_type i = 0;
    while (i < s.length() && isspace (s[i]))
	++i;
    s.erase (0, i);
    i = s.length() - 1;
    while (i > 0 && isspace (s[i]))
	--i;
    s.erase (i + 1, std::string::npos);
    return s;
}

/**
 * Split a string up into a vector of substrings delimited by commas.
 **/
void
string_token(std::vector<std::string>& substrings, const std::string& text, 
	     const std::string& delims = ",")
{
    std::string::size_type len = text.length();
    std::string::size_type pos = 0;
    std::string::size_type next;
    do
    {
	next = std::string::npos;
	if (pos < len)
	    next = text.find_first_of(delims, pos);
	std::string::size_type sublen = 
	    (next == std::string::npos) ? len - pos : next - pos;
	std::string ss = text.substr(pos, sublen);
	substrings.push_back(trim(ss));
	pos += sublen + 1;
    }
    while (next != std::string::npos);
}


